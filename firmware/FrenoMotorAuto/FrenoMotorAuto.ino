/* ============================================================================
   FRENO DE MOTOR AUTOMÁTICO · FIRMWARE DEL CANAL B
   Microcontrolador: ATmega328P a 16 MHz (el mismo chip del Arduino Uno)
   Versión: 1.0
   ----------------------------------------------------------------------------
   QUÉ HACE ESTE PROGRAMA
   El sistema tiene dos canales que deben estar de acuerdo para frenar:
     · Canal A (hardware): LM2907 + comparadores LM393 + compuertas 74HCT08.
     · Canal B (este programa): mide las mismas señales por su cuenta.
   La válvula del freno de motor solo se energiza cuando AMBOS canales dicen
   que sí. Este programa:
     1. Mide las RPM contando el tiempo entre pulsos del tacómetro.
     2. Lee el acelerador (TPS), el embrague, la batería, la salida SOL+ y el
        voltaje de RPM del canal A (VFV).
     3. Decide si se cumplen las 3 condiciones: RPM >= 1300, acelerador suelto
        (< 1.35 V con histéresis de 0.10 V) y embrague suelto.
     4. Vigila fallas: cables rotos o en corto, batería fuera de rango, canal A
        que no coincide, PROFET pegado, salida que no responde, carga en corto.
     5. Maneja dos salidas: MCU_EN (habilita la compuerta final del canal A) y
        GATE (cierra el MOSFET Q1 del lado bajo de la válvula).
     6. Envía un latido al watchdog externo DS1232 y usa el watchdog interno.
     7. Enciende el LED rojo con un código de parpadeo si detecta una falla.

   ESTADO SEGURO: válvula desenergizada (MCU_EN = 0 y GATE = 0). Ante cualquier
   duda, el programa siempre elige ese estado.

   ÍNDICE DE BLOQUES
     BLOQUE 1  · Ajustes del usuario (calibración)  <- aquí se hacen los cambios
     BLOQUE 2  · Asignación de pines
     BLOQUE 3  · Constantes calculadas (no tocar)
     BLOQUE 4  · Códigos de falla
     BLOQUE 5  · Tipos de datos y variables globales
     BLOQUE 6  · Arranque seguro (se ejecuta antes que setup)
     BLOQUE 7  · Interrupción del tacómetro (mide el tiempo entre pulsos)
     BLOQUE 8  · Utilidades de tiempo
     BLOQUE 9  · Lectura de sensores
     BLOQUE 10 · Lógica de decisión del canal B
     BLOQUE 11 · Diagnóstico de fallas
     BLOQUE 12 · Salidas hacia la etapa de potencia
     BLOQUE 13 · LED de falla (códigos de parpadeo)
     BLOQUE 14 · Registro de fallas en la EEPROM
     BLOQUE 15 · Modo depuración (monitor serie)
     BLOQUE 16 · setup()
     BLOQUE 17 · loop()

   CÓMO GRABARLO: ver el archivo LEEME_FIRMWARE.md (se graba SIN bootloader,
   con "Subir usando programador", porque el watchdog DS1232 no deja esperar
   al bootloader).
   ============================================================================ */

#include <Arduino.h>
#include <avr/wdt.h>

/* ============================================================================
   BLOQUE 1 · AJUSTES DEL USUARIO (CALIBRACIÓN)
   Todos los números que se pueden ajustar están aquí. Si cambias algo,
   anota el valor anterior. Los valores vienen de los cálculos del dossier.
   ============================================================================ */

// --- 1.1 Modos de funcionamiento ------------------------------------------
// 1 = el sistema automático funciona. 0 = el canal B nunca habilita la válvula
// (útil para probar el resto del circuito sin activar nada).
#define SISTEMA_AUTOMATICO_HABILITADO 1

// 1 = envía datos al monitor serie (115200 baudios) cada 250 ms por los pines
// 2 y 3 del chip (RX/TX). Úsalo para calibrar. Déjalo en 0 en el camión.
#define MODO_DEPURACION 0

// 1 = camión de 24 V. 0 = camión de 12 V (cambia el rango válido de batería).
#define SISTEMA_24V 1

// Prueba del watchdog en el banco. Con un valor distinto de 0, el programa se
// cuelga A PROPÓSITO esa cantidad de milisegundos después de arrancar. El
// DS1232 debe reiniciarlo: verás el LED encenderse fijo 1 s, una y otra vez.
// Déjalo SIEMPRE en 0 en el camión.
#define PRUEBA_WATCHDOG_MS 0

// --- 1.2 Señal de RPM -------------------------------------------------------
// Pulsos del tacómetro por cada vuelta del motor. DEBE coincidir con el
// condensador C13 del LM2907 (ver tabla 1.3.a del dossier). Valor de diseño: 4.
constexpr uint8_t  PULSOS_POR_VUELTA = 4;

constexpr float    RPM_ACTIVAR  = 1300.0f;  // a partir de aquí se permite frenar
constexpr float    RPM_LIBERAR  = 1170.0f;  // por debajo de aquí se suelta (histéresis)
constexpr float    RPM_MAXIMA_VALIDA = 4000.0f; // pulsos más rápidos que esto = ruido
constexpr uint16_t TACH_SIN_PULSOS_MS = 300;     // sin pulsos este tiempo = motor parado

// --- 1.3 Acelerador (TPS) ---------------------------------------------------
constexpr float TPS_SUELTO_V     = 1.30f;  // bajo este valor el pedal está suelto
constexpr float TPS_PISADO_V     = 1.40f;  // sobre este valor el pedal está pisado
constexpr float TPS_MINIMO_VALIDO = 0.20f; // menos = cable roto o en corto a masa
constexpr float TPS_MAXIMO_VALIDO = 4.80f; // más  = corto a +5 V o a batería

// --- 1.4 Embrague (interruptor codificado: 3.51 V = suelto, 0 V = pisado) --
constexpr float EMBRAGUE_SUELTO_MIN_V = 3.10f;
constexpr float EMBRAGUE_SUELTO_MAX_V = 3.90f;
constexpr float EMBRAGUE_CORTO_5V_V   = 4.30f; // más que esto = cable en corto a +5 V

// --- 1.5 Batería ---------------------------------------------------------------
#if SISTEMA_24V
constexpr float BATERIA_MIN_V = 18.0f;
constexpr float BATERIA_MAX_V = 32.0f;
#else
constexpr float BATERIA_MIN_V = 9.0f;
constexpr float BATERIA_MAX_V = 16.0f;
#endif

// --- 1.6 Conversor analógico (ADC) y divisores de tensión -------------------
// Mide con el multímetro el riel de +5 V y escribe aquí el valor real.
constexpr float VOLTAJE_REFERENCIA_ADC = 5.00f;
// Divisor de SOL+:   (R28 39 kΩ + R29 10 kΩ) / R29
constexpr float DIVISOR_SOL     = (39.0f + 10.0f) / 10.0f;    // = 4.90
// Divisor de batería: (R30 100 kΩ + R31 15 kΩ) / R31
constexpr float DIVISOR_BATERIA = (100.0f + 15.0f) / 15.0f;   // = 7.67
// Lecturas promediadas por cada canal en cada ciclo (más = menos ruido, más lento).
constexpr uint8_t MUESTRAS_ADC = 4;

// --- 1.7 Comparación con el canal A (convertidor LM2907) -------------------
// Voltaje que da el LM2907 por cada RPM: 12 V · pulsos/s · R12 · C13.
constexpr float VFV_POR_RPM = 12.0f * (PULSOS_POR_VUELTA / 60.0f) * 68000.0f * 15.0e-9f; // ≈ 0.000816 V/RPM
constexpr float VFV_CONSTANTE_TIEMPO_MS = 68.0f;  // R12 · C14 = 68 kΩ · 1 µF
constexpr float VFV_TOLERANCIA_V   = 0.25f;  // diferencia mínima admitida
constexpr float VFV_TOLERANCIA_REL = 0.20f;  // o el 20 % del valor esperado

// --- 1.8 Umbral para saber si SOL+ (salida a la válvula) tiene tensión ------
constexpr float SOL_CON_TENSION_V = 6.0f;

// --- 1.9 Tiempos (milisegundos) ---------------------------------------------
constexpr uint16_t CICLO_MS              = 5;    // el programa repite todo cada 5 ms
constexpr uint16_t ANTIRREBOTE_ACTIVAR_MS = 20;  // condiciones estables antes de frenar
constexpr uint16_t ARRANQUE_MS           = 1000; // al encender: no frena y prueba el LED
constexpr uint16_t CONFIRMAR_FALLA_MS    = 300;  // una falla debe durar esto para contarse
constexpr uint16_t RECUPERAR_FALLA_MS    = 2000; // sin falla este tiempo = se borra (fallas leves)
constexpr uint16_t PLAUSIBILIDAD_MS      = 500;  // canal A y B distintos durante esto = falla 4
constexpr uint16_t CANAL_A_ATASCADO_MS   = 1000; // canal A dice "sí" cuando claramente es "no"
constexpr uint16_t CANAL_A_SIN_RESP_MS   = 2000; // canal A dice "no" cuando claramente es "sí"
constexpr uint16_t PROFET_PEGADO_MS      = 100;  // SOL+ con tensión sin orden
constexpr uint16_t SALIDA_NO_RESPONDE_MS = 100;  // orden dada pero SOL+ sin tensión
constexpr uint16_t ST_FALLA_MS           = 50;   // PROFET avisa falla por ST
constexpr uint16_t DEPURACION_CADA_MS    = 250;

/* ============================================================================
   BLOQUE 2 · ASIGNACIÓN DE PINES
   Número de Arduino  = pin físico del ATmega328P (DIP-28) = hueco en la protoboard
   ============================================================================ */
constexpr uint8_t PIN_TACH        = 2;   // PD2 / INT0 · pin 4  · E53 · pulsos del motor (TACH_F)
constexpr uint8_t PIN_LED_FALLA   = 3;   // PD3        · pin 5  · E54 · LED rojo (con R32 330 Ω)
constexpr uint8_t PIN_CANAL_A     = 4;   // PD4        · pin 6  · E55 · CH_A, decisión del canal A
constexpr uint8_t PIN_PROFET_ST   = 5;   // PD5        · pin 11 · E60 · ST del PROFET (1 = sin falla)
constexpr uint8_t PIN_LATIDO_WD   = 6;   // PD6        · pin 12 · E61 · latido al DS1232 (ST)
constexpr uint8_t PIN_MCU_EN      = 8;   // PB0        · pin 14 · E63 · habilita 74HCT08 (pin 10)
constexpr uint8_t PIN_GATE_Q1     = 9;   // PB1        · pin 15 · F63 · compuerta del MOSFET Q1
constexpr uint8_t PIN_TPS         = A0;  // PC0        · pin 23 · F55 · acelerador filtrado
constexpr uint8_t PIN_EMBRAGUE    = A1;  // PC1        · pin 24 · F54 · embrague filtrado
constexpr uint8_t PIN_VFV         = A2;  // PC2        · pin 25 · F53 · voltaje de RPM del LM2907
constexpr uint8_t PIN_SOL         = A3;  // PC3        · pin 26 · F52 · SOL+ dividido entre 4.9
constexpr uint8_t PIN_BATERIA     = A4;  // PC4        · pin 27 · F51 · batería dividida entre 7.67
// Pines sin uso: se dejan como entrada con resistencia interna a +5 V (estado
// definido, no flotan). D0 y D1 se usan solo en modo depuración.
const uint8_t PINES_LIBRES[] = { 7, 10, 11, 12, 13, A5 };

/* ============================================================================
   BLOQUE 3 · CONSTANTES CALCULADAS (NO TOCAR)
   ============================================================================ */
constexpr uint32_t PERIODO_MINIMO_US = (uint32_t)(60.0e6f / (RPM_MAXIMA_VALIDA * PULSOS_POR_VUELTA));
constexpr uint32_t PERIODO_MAXIMO_US = (uint32_t)TACH_SIN_PULSOS_MS * 1000UL;
constexpr uint8_t  PERIODOS_PROMEDIO = 4;   // se promedian los últimos 4 periodos
constexpr float    ADC_A_VOLTIOS     = VOLTAJE_REFERENCIA_ADC / 1024.0f;
constexpr float    VFV_ALFA          = CICLO_MS / (VFV_CONSTANTE_TIEMPO_MS + CICLO_MS);

// Comprobaciones al compilar: si algún ajuste es incoherente, el compilador
// se detiene y muestra el mensaje entre comillas.
static_assert(RPM_LIBERAR < RPM_ACTIVAR, "RPM_LIBERAR debe ser menor que RPM_ACTIVAR");
static_assert(TPS_SUELTO_V < TPS_PISADO_V, "TPS_SUELTO_V debe ser menor que TPS_PISADO_V");
static_assert(TPS_MINIMO_VALIDO < TPS_SUELTO_V, "TPS_MINIMO_VALIDO debe ser menor que TPS_SUELTO_V");
static_assert(EMBRAGUE_SUELTO_MIN_V < EMBRAGUE_SUELTO_MAX_V, "Ventana del embrague invertida");
static_assert(EMBRAGUE_SUELTO_MAX_V < EMBRAGUE_CORTO_5V_V, "El umbral de corto debe estar sobre la ventana");
static_assert(BATERIA_MIN_V < BATERIA_MAX_V, "Rango de batería invertido");
static_assert(PULSOS_POR_VUELTA >= 1, "PULSOS_POR_VUELTA debe ser 1 o más");
static_assert(CICLO_MS >= 4 && CICLO_MS <= 20, "CICLO_MS fuera de rango seguro (4 a 20 ms)");
static_assert(PERIODO_MINIMO_US < PERIODO_MAXIMO_US, "Revisa RPM_MAXIMA_VALIDA y TACH_SIN_PULSOS_MS");

/* ============================================================================
   BLOQUE 4 · CÓDIGOS DE FALLA
   El LED rojo parpadea tantas veces como el número del código, hace una
   pausa y repite. "Leve" = se borra sola cuando el problema desaparece 2 s.
   "Grave" = bloquea el sistema hasta apagar y encender (quitar la llave).
   ============================================================================ */
enum CodigoFalla : uint8_t {
  FALLA_NINGUNA          = 0,
  FALLA_TPS_FUERA        = 1,  // Leve:  acelerador fuera de 0.20–4.80 V (cable roto o en corto)
  FALLA_EMBRAGUE_CORTO   = 2,  // Leve:  embrague en corto a +5 V
  FALLA_BATERIA          = 3,  // Leve:  batería fuera de rango
  FALLA_RPM_NO_PLAUSIBLE = 4,  // Leve:  RPM del canal A y del canal B no coinciden
  FALLA_CANAL_A_ATASCADO = 5,  // Grave: canal A dice "frenar" cuando claramente no corresponde
  FALLA_PROFET_PEGADO    = 6,  // Grave: hay tensión en SOL+ sin haber dado la orden
  FALLA_SALIDA_NO_RESP   = 7,  // Grave: se dio la orden y SOL+ no tiene tensión
  FALLA_CARGA_ST         = 8,  // Grave: el PROFET avisa carga en corto o abierta (ST = 0)
  FALLA_CANAL_A_SIN_RESP = 9   // Aviso: canal A no responde (el sistema no frena, no bloquea)
};
// Orden en que se muestran si hay varias activas (la primera tiene prioridad).
const uint8_t PRIORIDAD_LED[] = { 6, 8, 7, 5, 4, 1, 2, 3, 9 };

/* ============================================================================
   BLOQUE 5 · TIPOS DE DATOS Y VARIABLES GLOBALES
   ============================================================================ */
// Estados del sistema.
enum Estado : uint8_t {
  ESTADO_ARRANQUE  = 0,  // primeros 1000 ms: nunca frena, prueba el LED
  ESTADO_NORMAL    = 1,  // funciona: frena cuando se cumplen las condiciones
  ESTADO_BLOQUEADO = 2   // falla grave: nunca frena hasta reiniciar
};

// Todas las mediciones de un ciclo, ya convertidas a unidades reales.
struct Entradas {
  float rpm;        // RPM medidas por el canal B
  float vfv;        // voltaje del LM2907 (canal A), en V
  float tps;        // acelerador, en V
  float embrague;   // embrague, en V
  float bateria;    // batería, en V
  float sol;        // salida SOL+ hacia la válvula, en V
  bool  canalA;     // CH_A: 1 = el canal A dice que se cumplen las condiciones
  bool  stOk;       // ST del PROFET: 1 = sin falla
};

// Ayuda para "esta condición se mantuvo X milisegundos seguidos".
struct Persistencia {
  bool     activa;
  uint32_t desde;
};

// Una falla leve: necesita aparecer CONFIRMAR_FALLA_MS y desaparecer
// RECUPERAR_FALLA_MS para cambiar de estado.
struct FallaLeve {
  Persistencia aparece;
  Persistencia desaparece;
  bool activa;
};

// Variables compartidas con la interrupción del tacómetro (volatile).
volatile uint32_t tachUltimoFlancoUs = 0;
volatile uint32_t tachPeriodosUs[PERIODOS_PROMEDIO];
volatile uint8_t  tachIndice  = 0;
volatile uint8_t  tachValidos = 0;

// Estado general.
Estado   estado          = ESTADO_ARRANQUE;
uint8_t  codigoBloqueo   = FALLA_NINGUNA;   // falla grave que causó el bloqueo
uint16_t fallasActivas   = 0;               // un bit por código: bit 0 = código 1, etc.
uint32_t inicioMs        = 0;               // millis() al terminar setup()
uint32_t proximoCicloMs  = 0;

// Decisión del canal B (con histéresis).
bool rpmSuficiente   = false;
bool aceleradorSuelto = false;
bool salidaActiva    = false;   // lo que se aplicó a MCU_EN y GATE en el último ciclo
uint32_t salidaActivaDesdeMs = 0;
float vfvEsperado    = 0.0f;    // VFV que debería dar el canal A según nuestras RPM

// Temporizadores de persistencia.
Persistencia pAntirrebote, pPlausibilidad, pCanalAAtascado, pCanalASinResp,
             pProfetPegado, pSalidaNoResponde, pFallaST;
FallaLeve fTps, fEmbrague, fBateria, fPlausibilidad;

// Causa del último reinicio (la llena el BLOQUE 6 antes de setup).
uint8_t registroReinicio __attribute__((section(".noinit")));

// Direcciones de la EEPROM (memoria que no se borra al apagar).
constexpr uint16_t EE_FIRMA           = 0;  // 1 byte: 0xA5 = memoria ya inicializada
constexpr uint16_t EE_ULTIMA_FALLA    = 1;  // 1 byte: último código de falla registrado
constexpr uint16_t EE_CONTADOR_FALLAS = 2;  // 2 bytes: cuántas fallas se registraron en total
constexpr uint16_t EE_REINICIOS_WD    = 4;  // 2 bytes: cuántas veces reinició un watchdog
constexpr uint8_t FIRMA_EEPROM = 0xA5;

// Prototipos de funciones (para que el orden en el archivo no importe).
void capturarCausaReinicio(void) __attribute__((naked, used, section(".init3")));
bool persiste(Persistencia &p, bool condicion, uint16_t ms, uint32_t ahora);
void evaluarFallaLeve(FallaLeve &f, bool hayProblema, uint8_t codigo, uint32_t ahora);
void activarBit(uint8_t codigo, bool activo);
bool bitActivo(uint8_t codigo);
void bloquear(uint8_t codigo);
float leerVoltios(uint8_t pin, float factor);
float calcularRPM();
void leerEntradas(Entradas &e);
bool decidirCanalB(const Entradas &e);
void diagnosticar(const Entradas &e, uint32_t ahora);
void aplicarSalidas(bool activar, uint32_t ahora);
void actualizarLed(uint32_t ahora);
void registrarFallaEEPROM(uint8_t codigo);
void prepararEEPROM();
uint8_t eeLeerByte(uint16_t direccion);
void eeEscribirByte(uint16_t direccion, uint8_t valor);
uint16_t eeLeerPalabra(uint16_t direccion);
void eeEscribirPalabra(uint16_t direccion, uint16_t valor);
void enviarLatido();
void imprimirDepuracion(const Entradas &e, uint32_t ahora);

/* ============================================================================
   BLOQUE 6 · ARRANQUE SEGURO (SE EJECUTA ANTES QUE setup)
   Si el chip se reinició por su watchdog interno, ese watchdog sigue activo
   con el tiempo más corto (15 ms) y reiniciaría el chip una y otra vez antes
   de llegar a setup(). Esta función corre en la sección .init3, apenas el chip
   arranca: guarda la causa del reinicio y apaga el watchdog interno.
   Mientras tanto MCU_EN y GATE quedan en 0 por las resistencias R26 y R1.
   ============================================================================ */
void capturarCausaReinicio(void) {
  registroReinicio = MCUSR;  // bits: PORF encendido, EXTRF externo (DS1232), WDRF watchdog interno
  MCUSR = 0;
  wdt_disable();
}

/* ============================================================================
   BLOQUE 7 · INTERRUPCIÓN DEL TACÓMETRO
   Se ejecuta sola en cada flanco de subida del pin 2 (INT0). Solo guarda el
   tiempo entre pulsos; el cálculo de RPM se hace fuera (BLOQUE 9).
   Filtro de ruido: un pulso que llega antes de PERIODO_MINIMO_US se ignora
   sin mover la referencia, así el siguiente pulso real se mide bien.
   ============================================================================ */
ISR(INT0_vect) {
  uint32_t ahora = micros();
  uint32_t periodo = ahora - tachUltimoFlancoUs;
  if (periodo < PERIODO_MINIMO_US) return;          // ruido: se descarta
  tachUltimoFlancoUs = ahora;
  if (periodo > PERIODO_MAXIMO_US) {                // primer pulso después de estar parado
    tachValidos = 0;
    return;
  }
  tachPeriodosUs[tachIndice] = periodo;
  tachIndice = (uint8_t)((tachIndice + 1) % PERIODOS_PROMEDIO);
  if (tachValidos < PERIODOS_PROMEDIO) tachValidos++;
}

/* ============================================================================
   BLOQUE 8 · UTILIDADES DE TIEMPO
   ============================================================================ */
// Devuelve true cuando "condicion" lleva al menos "ms" milisegundos seguidos.
// Si la condición se interrumpe, la cuenta vuelve a cero.
bool persiste(Persistencia &p, bool condicion, uint16_t ms, uint32_t ahora) {
  if (!condicion) {
    p.activa = false;
    return false;
  }
  if (!p.activa) {
    p.activa = true;
    p.desde = ahora;
  }
  return (uint32_t)(ahora - p.desde) >= ms;
}

/* ============================================================================
   BLOQUE 9 · LECTURA DE SENSORES
   ============================================================================ */
// Lee un canal analógico y lo convierte a voltios reales.
// La primera lectura se descarta: al cambiar de canal el ADC necesita un
// momento para estabilizarse (los divisores tienen 8 a 13 kΩ de impedancia).
float leerVoltios(uint8_t pin, float factor) {
  analogRead(pin);
  uint16_t suma = 0;
  for (uint8_t i = 0; i < MUESTRAS_ADC; i++) suma += (uint16_t)analogRead(pin);
  return ((float)suma / MUESTRAS_ADC) * ADC_A_VOLTIOS * factor;
}

// Calcula las RPM a partir de los periodos guardados por la interrupción.
float calcularRPM() {
  uint32_t ultimo, periodos[PERIODOS_PROMEDIO];
  uint8_t validos, indice;
  noInterrupts();                              // copia sin que la interrupción cambie nada
  ultimo  = tachUltimoFlancoUs;
  validos = tachValidos;
  indice  = tachIndice;
  for (uint8_t i = 0; i < PERIODOS_PROMEDIO; i++) periodos[i] = tachPeriodosUs[i];
  interrupts();

  uint32_t desdeUltimo = micros() - ultimo;
  if (validos < 2 || desdeUltimo > PERIODO_MAXIMO_US) return 0.0f;  // motor parado o sin señal

  // Promedio de los "validos" periodos más recientes. "indice" apunta a la
  // posición que se escribirá después, así que se recorre hacia atrás.
  uint32_t suma = 0;
  for (uint8_t k = 0; k < validos; k++) {
    uint8_t pos = (uint8_t)((indice + PERIODOS_PROMEDIO - 1 - k) % PERIODOS_PROMEDIO);
    suma += periodos[pos];
  }
  float periodo = (float)suma / validos;
  // Si el motor está bajando rápido, el tiempo desde el último pulso ya es
  // mayor que el promedio: se usa ese valor para reaccionar antes.
  if ((float)desdeUltimo > periodo) periodo = (float)desdeUltimo;
  return 60.0e6f / (periodo * PULSOS_POR_VUELTA);
}

// Llena la estructura con todas las mediciones del ciclo.
void leerEntradas(Entradas &e) {
  e.rpm      = calcularRPM();
  e.tps      = leerVoltios(PIN_TPS, 1.0f);
  e.embrague = leerVoltios(PIN_EMBRAGUE, 1.0f);
  e.vfv      = leerVoltios(PIN_VFV, 1.0f);
  e.sol      = leerVoltios(PIN_SOL, DIVISOR_SOL);
  e.bateria  = leerVoltios(PIN_BATERIA, DIVISOR_BATERIA);
  e.canalA   = digitalRead(PIN_CANAL_A) == HIGH;
  e.stOk     = digitalRead(PIN_PROFET_ST) == HIGH;
}

/* ============================================================================
   BLOQUE 10 · LÓGICA DE DECISIÓN DEL CANAL B
   Devuelve true si, según este canal, se cumplen todas las condiciones.
   La histéresis evita que la válvula se encienda y apague sin parar cuando
   una señal está justo en el límite.
   ============================================================================ */
bool decidirCanalB(const Entradas &e) {
  // Condición 1: RPM >= 1300 (se suelta bajo 1170).
  if (!rpmSuficiente && e.rpm >= RPM_ACTIVAR) rpmSuficiente = true;
  if (rpmSuficiente && e.rpm < RPM_LIBERAR)   rpmSuficiente = false;

  // Condición 2: acelerador suelto, < 1.30 V para soltar y > 1.40 V para pisar.
  if (!aceleradorSuelto && e.tps < TPS_SUELTO_V) aceleradorSuelto = true;
  if (aceleradorSuelto && e.tps > TPS_PISADO_V)  aceleradorSuelto = false;
  bool tpsValido = (e.tps > TPS_MINIMO_VALIDO) && (e.tps < TPS_MAXIMO_VALIDO);

  // Condición 3: embrague suelto, solo si el voltaje cae en la ventana de 3.10 a 3.90 V.
  bool embragueSuelto = (e.embrague > EMBRAGUE_SUELTO_MIN_V) && (e.embrague < EMBRAGUE_SUELTO_MAX_V);

  // Requisitos de seguridad adicionales.
  bool bateriaOk = (e.bateria > BATERIA_MIN_V) && (e.bateria < BATERIA_MAX_V);

  return rpmSuficiente && aceleradorSuelto && tpsValido && embragueSuelto && bateriaOk;
}

/* ============================================================================
   BLOQUE 11 · DIAGNÓSTICO DE FALLAS
   ============================================================================ */
void activarBit(uint8_t codigo, bool activo) {
  uint16_t mascara = (uint16_t)1 << (codigo - 1);
  if (activo) fallasActivas |= mascara; else fallasActivas &= (uint16_t)~mascara;
}

bool bitActivo(uint8_t codigo) {
  return (fallasActivas & ((uint16_t)1 << (codigo - 1))) != 0;
}

// Falla grave: el sistema queda bloqueado hasta quitar la alimentación.
void bloquear(uint8_t codigo) {
  activarBit(codigo, true);
  if (estado != ESTADO_BLOQUEADO) {
    estado = ESTADO_BLOQUEADO;
    codigoBloqueo = codigo;
    registrarFallaEEPROM(codigo);
  }
}

// Falla leve: se activa si dura CONFIRMAR_FALLA_MS y se borra tras
// RECUPERAR_FALLA_MS sin el problema.
void evaluarFallaLeve(FallaLeve &f, bool hayProblema, uint8_t codigo, uint32_t ahora) {
  if (!f.activa && persiste(f.aparece, hayProblema, CONFIRMAR_FALLA_MS, ahora)) {
    f.activa = true;
    registrarFallaEEPROM(codigo);
  }
  if (!hayProblema) f.aparece.activa = false;
  if (f.activa && persiste(f.desaparece, !hayProblema, RECUPERAR_FALLA_MS, ahora)) {
    f.activa = false;
  }
  if (hayProblema) f.desaparece.activa = false;
  activarBit(codigo, f.activa);
}

void diagnosticar(const Entradas &e, uint32_t ahora) {
  // --- 11.1 Fallas leves de sensores y alimentación --------------------------
  evaluarFallaLeve(fTps, !(e.tps > TPS_MINIMO_VALIDO && e.tps < TPS_MAXIMO_VALIDO), FALLA_TPS_FUERA, ahora);
  evaluarFallaLeve(fEmbrague, e.embrague > EMBRAGUE_CORTO_5V_V, FALLA_EMBRAGUE_CORTO, ahora);
  evaluarFallaLeve(fBateria, !(e.bateria > BATERIA_MIN_V && e.bateria < BATERIA_MAX_V), FALLA_BATERIA, ahora);

  // --- 11.2 Plausibilidad de RPM entre canal A (VFV) y canal B ---------------
  // Se simula el retardo del filtro del LM2907 para comparar en igualdad.
  vfvEsperado += (e.rpm * VFV_POR_RPM - vfvEsperado) * VFV_ALFA;
  float limite = max(VFV_TOLERANCIA_V, VFV_TOLERANCIA_REL * vfvEsperado);
  bool hayRpm = (e.rpm > 400.0f) || (e.vfv > 0.35f);
  bool noCoinciden = hayRpm && (fabs(e.vfv - vfvEsperado) > limite);
  evaluarFallaLeve(fPlausibilidad, persiste(pPlausibilidad, noCoinciden, PLAUSIBILIDAD_MS, ahora),
                   FALLA_RPM_NO_PLAUSIBLE, ahora);

  // --- 11.3 Canal A atascado en "sí" (grave) ---------------------------------
  // CH_A = 1 mientras el canal B ve claramente que NO corresponde frenar.
  bool claramenteNo = (e.embrague < 2.0f) ||
                      (e.tps > TPS_PISADO_V + 0.30f && e.tps < TPS_MAXIMO_VALIDO) ||
                      (e.rpm < RPM_LIBERAR - 200.0f && e.vfv < (RPM_LIBERAR - 200.0f) * VFV_POR_RPM);
  if (persiste(pCanalAAtascado, e.canalA && claramenteNo, CANAL_A_ATASCADO_MS, ahora)) {
    bloquear(FALLA_CANAL_A_ATASCADO);
  }

  // --- 11.4 Canal A sin respuesta (aviso) -----------------------------------
  bool claramenteSi = (e.rpm > RPM_ACTIVAR + 200.0f) && (e.tps < TPS_SUELTO_V - 0.30f) &&
                      (e.tps > TPS_MINIMO_VALIDO) &&
                      (e.embrague > EMBRAGUE_SUELTO_MIN_V) && (e.embrague < EMBRAGUE_SUELTO_MAX_V);
  activarBit(FALLA_CANAL_A_SIN_RESP,
             persiste(pCanalASinResp, !e.canalA && claramenteSi, CANAL_A_SIN_RESP_MS, ahora));

  // --- 11.5 Etapa de potencia ------------------------------------------------
  bool solConTension = e.sol > SOL_CON_TENSION_V;
  // PROFET pegado: hay tensión en SOL+ sin que este canal haya dado la orden.
  // (En ese caso Q1 ya está abierto, así que la válvula no recibe corriente.)
  if (persiste(pProfetPegado, !salidaActiva && solConTension, PROFET_PEGADO_MS, ahora)) {
    bloquear(FALLA_PROFET_PEGADO);
  }
  // Con la orden dada y el canal A de acuerdo, el PROFET debe estar encendido.
  bool profetDebeConducir = salidaActiva && e.canalA &&
                            (uint32_t)(ahora - salidaActivaDesdeMs) > 20UL;
  if (persiste(pSalidaNoResponde, profetDebeConducir && !solConTension, SALIDA_NO_RESPONDE_MS, ahora)) {
    bloquear(FALLA_SALIDA_NO_RESP);
  }
  if (persiste(pFallaST, profetDebeConducir && !e.stOk, ST_FALLA_MS, ahora)) {
    bloquear(FALLA_CARGA_ST);
  }
}

/* ============================================================================
   BLOQUE 12 · SALIDAS HACIA LA ETAPA DE POTENCIA
   MCU_EN habilita la compuerta final del canal A (74HCT08 pin 10).
   GATE cierra el MOSFET Q1 (lado bajo de la válvula).
   Ambas se mueven juntas: o las dos en 1, o las dos en 0.
   ============================================================================ */
void aplicarSalidas(bool activar, uint32_t ahora) {
  if (activar && !salidaActiva) salidaActivaDesdeMs = ahora;
  salidaActiva = activar;
  digitalWrite(PIN_MCU_EN,  activar ? HIGH : LOW);
  digitalWrite(PIN_GATE_Q1, activar ? HIGH : LOW);
}

/* ============================================================================
   BLOQUE 13 · LED DE FALLA (CÓDIGOS DE PARPADEO)
   · Arranque: encendido fijo 1 s (prueba de que el LED funciona).
   · Sin fallas: apagado.
   · Con falla: N destellos (250 ms encendido, 250 ms apagado), pausa de
     1.5 s y repite. N = código de falla (BLOQUE 4).
   ============================================================================ */
void actualizarLed(uint32_t ahora) {
  if (estado == ESTADO_ARRANQUE) { digitalWrite(PIN_LED_FALLA, HIGH); return; }

  uint8_t codigo = FALLA_NINGUNA;
  if (estado == ESTADO_BLOQUEADO) codigo = codigoBloqueo;
  else {
    for (uint8_t i = 0; i < sizeof(PRIORIDAD_LED); i++) {
      if (bitActivo(PRIORIDAD_LED[i])) { codigo = PRIORIDAD_LED[i]; break; }
    }
  }
  if (codigo == FALLA_NINGUNA) { digitalWrite(PIN_LED_FALLA, LOW); return; }

  uint32_t duracionSecuencia = (uint32_t)codigo * 500UL + 1500UL;
  uint32_t t = (ahora - inicioMs) % duracionSecuencia;
  bool encendido = (t < (uint32_t)codigo * 500UL) && ((t % 500UL) < 250UL);
  digitalWrite(PIN_LED_FALLA, encendido ? HIGH : LOW);
}

/* ============================================================================
   BLOQUE 14 · REGISTRO DE FALLAS EN LA EEPROM
   Guarda el último código de falla y cuántas hubo. Las funciones eeLeer y
   eeEscribir usan directamente los registros EEAR, EEDR y EECR del chip
   (procedimiento de la hoja de datos del ATmega328P, sección EEPROM).
   Solo se escribe si el valor cambia, para cuidar la vida útil de la memoria.
   ============================================================================ */
uint8_t eeLeerByte(uint16_t direccion) {
  while (EECR & _BV(EEPE)) { }          // espera si hay una escritura en curso
  EEAR = direccion;
  EECR |= _BV(EERE);                    // orden de lectura
  return EEDR;
}

void eeEscribirByte(uint16_t direccion, uint8_t valor) {
  if (eeLeerByte(direccion) == valor) return;   // ya tiene ese valor: no se gasta
  while (EECR & _BV(EEPE)) { }
  EEAR = direccion;
  EEDR = valor;
  uint8_t sreg = SREG;
  cli();                                // EEMPE y EEPE deben ir seguidos (4 ciclos)
  EECR |= _BV(EEMPE);
  EECR |= _BV(EEPE);                    // la escritura dura unos 3.4 ms, en segundo plano
  SREG = sreg;
}

uint16_t eeLeerPalabra(uint16_t direccion) {
  return (uint16_t)eeLeerByte(direccion) | ((uint16_t)eeLeerByte(direccion + 1) << 8);
}

void eeEscribirPalabra(uint16_t direccion, uint16_t valor) {
  eeEscribirByte(direccion, (uint8_t)(valor & 0xFF));
  eeEscribirByte(direccion + 1, (uint8_t)(valor >> 8));
}

void prepararEEPROM() {
  if (eeLeerByte(EE_FIRMA) != FIRMA_EEPROM) {
    eeEscribirByte(EE_FIRMA, FIRMA_EEPROM);
    eeEscribirByte(EE_ULTIMA_FALLA, FALLA_NINGUNA);
    eeEscribirPalabra(EE_CONTADOR_FALLAS, 0);
    eeEscribirPalabra(EE_REINICIOS_WD, 0);
  }
  // Reinicio por watchdog interno (WDRF) o por el DS1232 (EXTRF sin PORF).
  bool porWatchdog = (registroReinicio & _BV(WDRF)) ||
                     ((registroReinicio & _BV(EXTRF)) && !(registroReinicio & _BV(PORF)));
  if (porWatchdog) {
    uint16_t n = eeLeerPalabra(EE_REINICIOS_WD);
    if (n < 0xFFFF) eeEscribirPalabra(EE_REINICIOS_WD, n + 1);
  }
}

void registrarFallaEEPROM(uint8_t codigo) {
  // Límite de escrituras por encendido: una falla que aparece y desaparece
  // muchas veces no debe gastar la EEPROM (vida útil: 100 000 escrituras).
  static uint8_t escriturasEsteEncendido = 0;
  if (escriturasEsteEncendido >= 20) return;
  escriturasEsteEncendido++;
  eeEscribirByte(EE_ULTIMA_FALLA, codigo);
  uint16_t n = eeLeerPalabra(EE_CONTADOR_FALLAS);
  if (n < 0xFFFF) eeEscribirPalabra(EE_CONTADOR_FALLAS, n + 1);
}

/* ============================================================================
   BLOQUE 15 · MODO DEPURACIÓN (MONITOR SERIE)
   Solo se compila si MODO_DEPURACION = 1. Formato de cada línea:
   t=segundos RPM VFV TPS EMB BAT SOL A ST EN EST F
   ============================================================================ */
void imprimirDepuracion(const Entradas &e, uint32_t ahora) {
#if MODO_DEPURACION
  static uint32_t ultimo = 0;
  if ((uint32_t)(ahora - ultimo) < DEPURACION_CADA_MS) return;
  ultimo = ahora;
  Serial.print(F("t="));    Serial.print((ahora - inicioMs) / 1000.0f, 2);
  Serial.print(F(" RPM=")); Serial.print(e.rpm, 0);
  Serial.print(F(" VFV=")); Serial.print(e.vfv, 2);
  Serial.print(F(" TPS=")); Serial.print(e.tps, 2);
  Serial.print(F(" EMB=")); Serial.print(e.embrague, 2);
  Serial.print(F(" BAT=")); Serial.print(e.bateria, 1);
  Serial.print(F(" SOL=")); Serial.print(e.sol, 1);
  Serial.print(F(" A="));   Serial.print(e.canalA ? 1 : 0);
  Serial.print(F(" ST="));  Serial.print(e.stOk ? 1 : 0);
  Serial.print(F(" EN="));  Serial.print(salidaActiva ? 1 : 0);
  Serial.print(F(" EST=")); Serial.print(estado == ESTADO_ARRANQUE ? F("ARRANQUE") :
                                         estado == ESTADO_NORMAL ? F("NORMAL") : F("BLOQUEADO"));
  Serial.print(F(" F=0x")); Serial.println(fallasActivas, HEX);
#else
  (void)e; (void)ahora;
#endif
}

/* ============================================================================
   LATIDO DEL WATCHDOG
   Un pulso corto (alto-bajo-alto) en el pin ST del DS1232 en cada ciclo.
   Se llama SOLO al final de loop(): si el programa se cuelga en cualquier
   parte, los pulsos se detienen y el DS1232 reinicia el chip (62 a 250 ms).
   ============================================================================ */
void enviarLatido() {
  digitalWrite(PIN_LATIDO_WD, LOW);
  delayMicroseconds(5);
  digitalWrite(PIN_LATIDO_WD, HIGH);
}

/* ============================================================================
   BLOQUE 16 · setup()  (se ejecuta una vez al encender o reiniciar)
   ============================================================================ */
void setup() {
  // 16.1 Salidas en estado seguro LO PRIMERO.
  digitalWrite(PIN_MCU_EN, LOW);
  digitalWrite(PIN_GATE_Q1, LOW);
  pinMode(PIN_MCU_EN, OUTPUT);
  pinMode(PIN_GATE_Q1, OUTPUT);

  // 16.2 Latido inmediato: el DS1232 espera un pulso poco después de soltar el reset.
  digitalWrite(PIN_LATIDO_WD, HIGH);
  pinMode(PIN_LATIDO_WD, OUTPUT);
  enviarLatido();

  // 16.3 Resto de pines.
  pinMode(PIN_LED_FALLA, OUTPUT);
  digitalWrite(PIN_LED_FALLA, HIGH);           // prueba del LED durante el arranque
  pinMode(PIN_CANAL_A, INPUT);                 // lo maneja el 74HCT08
  pinMode(PIN_PROFET_ST, INPUT);               // tiene pull-up externo R3
  pinMode(PIN_TACH, INPUT);                    // señal ya filtrada y recortada a 5.1 V
  for (uint8_t i = 0; i < sizeof(PINES_LIBRES); i++) pinMode(PINES_LIBRES[i], INPUT_PULLUP);
  analogReference(DEFAULT);                    // referencia del ADC = AVCC (+5 V)

#if MODO_DEPURACION
  Serial.begin(115200);
#else
  pinMode(0, INPUT_PULLUP);
  pinMode(1, INPUT_PULLUP);
#endif

  // 16.4 Interrupción del tacómetro: INT0 en flanco de subida.
  EICRA = (EICRA & (uint8_t)~(_BV(ISC01) | _BV(ISC00))) | _BV(ISC01) | _BV(ISC00);
  EIFR  = _BV(INTF0);                          // borra cualquier aviso pendiente
  EIMSK |= _BV(INT0);

  // 16.5 Memoria de fallas.
  prepararEEPROM();
  enviarLatido();

#if MODO_DEPURACION
  Serial.println(F("Freno de motor automatico - canal B v1.0"));
  Serial.print(F("Causa de reinicio MCUSR=0x")); Serial.println(registroReinicio, HEX);
  Serial.print(F("Ultima falla registrada: ")); Serial.println(eeLeerByte(EE_ULTIMA_FALLA));
  Serial.print(F("Fallas registradas: ")); Serial.println(eeLeerPalabra(EE_CONTADOR_FALLAS));
  Serial.print(F("Reinicios por watchdog: ")); Serial.println(eeLeerPalabra(EE_REINICIOS_WD));
#endif

  // 16.6 Watchdog interno: si loop() no termina en 120 ms, el chip se reinicia.
  wdt_enable(WDTO_120MS);

  inicioMs = millis();
  proximoCicloMs = inicioMs;
  estado = ESTADO_ARRANQUE;
}

/* ============================================================================
   BLOQUE 17 · loop()  (se repite sin parar; el trabajo se hace cada 5 ms)
   Orden de cada ciclo:
     1. Leer sensores           4. Aplicar salidas
     2. Diagnosticar fallas     5. Actualizar LED y depuración
     3. Decidir                 6. Latido al DS1232 y al watchdog interno
   ============================================================================ */
void loop() {
  uint32_t ahora = millis();
  if ((int32_t)(ahora - proximoCicloMs) < 0) return;      // todavía no toca
  proximoCicloMs += CICLO_MS;
  if ((int32_t)(ahora - proximoCicloMs) > (int32_t)(10 * CICLO_MS)) proximoCicloMs = ahora + CICLO_MS;

#if PRUEBA_WATCHDOG_MS > 0
  // Cuelgue intencional (BLOQUE 1.1). Sin latido ni wdt_reset: el DS1232
  // reinicia el chip en 62 a 250 ms, y si no lo hace, el watchdog interno en 125 ms.
  if ((uint32_t)(ahora - inicioMs) > (uint32_t)PRUEBA_WATCHDOG_MS) { while (true) { } }
#endif

  // 1. Leer sensores.
  Entradas e;
  leerEntradas(e);

  // 2. Fin del arranque.
  if (estado == ESTADO_ARRANQUE && (uint32_t)(ahora - inicioMs) >= ARRANQUE_MS) {
    estado = ESTADO_NORMAL;
  }

  // 3. Diagnóstico (también corre durante el arranque: detecta un PROFET pegado).
  diagnosticar(e, ahora);

  // 4. Decisión final del canal B.
  bool condiciones = decidirCanalB(e);
  bool hayFallaLeve = bitActivo(FALLA_TPS_FUERA) || bitActivo(FALLA_EMBRAGUE_CORTO) ||
                      bitActivo(FALLA_BATERIA) || bitActivo(FALLA_RPM_NO_PLAUSIBLE);
  bool permitido = (SISTEMA_AUTOMATICO_HABILITADO != 0) && (estado == ESTADO_NORMAL) && !hayFallaLeve;
  // Activar exige condiciones estables 20 ms. Desactivar es inmediato.
  bool activar = persiste(pAntirrebote, condiciones && permitido, ANTIRREBOTE_ACTIVAR_MS, ahora);

  // 5. Salidas, LED y depuración.
  aplicarSalidas(activar, ahora);
  actualizarLed(ahora);
  imprimirDepuracion(e, ahora);

  // 6. Todo el ciclo terminó bien: latido al DS1232 y al watchdog interno.
  enviarLatido();
  wdt_reset();
}
