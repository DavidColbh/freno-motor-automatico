# Firmware del canal B · Freno de Motor Automático

Programa del microcontrolador **ATmega328P** (16 MHz). Es el "canal B" del sistema: mide por su cuenta las mismas señales que el canal analógico y solo permite frenar cuando los dos canales están de acuerdo.

| Archivo | Para qué sirve |
|---|---|
| `FrenoMotorAuto/FrenoMotorAuto.ino` | Programa fuente. Se abre con el IDE de Arduino. Está dividido en 17 bloques comentados. |
| `FrenoMotorAuto.hex` | El mismo programa ya compilado, con los ajustes de fábrica. Sirve de referencia o para grabarlo con un programador sin compilar. |
| `LEEME_FIRMWARE.md` | Esta guía. |

---

## 1. Cambios de hardware que acompañan a esta versión

Al revisar el circuito para escribir el firmware se encontró un error grave en la protoboard y se corrigió en todos los documentos. **Si ya armaste estas piezas, muévelas antes de encender.**

| Pieza | Antes (incorrecto) | Ahora | Motivo |
|---|---|---|---|
| W13 (cable SOL+) | A1 → D51 | **A1 → A27** | D51 estaba en la fila del pin 2 del ATmega: le llegaban 24 V y lo destruían. |
| W09 (cable BAT+) | F1 → J61 | **F1 → D9** | J61 estaba en la fila del pin 17 del ATmega: mismo problema. |
| R28 39 kΩ | C51 → C52 | **B27 → B28** | Divisor de SOL+ lejos del chip. |
| R29 10 kΩ | B52 → T−52 | **A28 → T−28** | |
| W68 (a la entrada A3) | A52 → G52 | **D28 → G52** | Solo el punto medio (máx. 4.9 V) llega al chip. |
| R30 100 kΩ | I61 → I60 | **E9 → F6** (cruza el canal en diagonal) | Divisor de batería lejos del chip. |
| R31 15 kΩ | H60 → B−60 | **J6 → B−6** | |
| W69 (a la entrada A4) | G60 → G51 | **G6 → G51** | Solo el punto medio (máx. 4.2 V) llega al chip. |
| W00 (puente de rieles GND) | T−1 → B−1 | **T−12 → B−12** | Compartía hueco con TVS1. |
| C2 100 nF | G3 → B−3 | **G3 → G4** | Compartía hueco con C1 (G4 ya es GND). |
| C6, C9, C12, R8, C14, C17 | B−9, B−22, B−42, B−28, T−25, T−31 | B−10, B−21, B−41, B−27, T−27, T−30 | Cada uno compartía hueco de riel con otra pieza. |

Con esto, los pines 2, 3, 17 y 18 del ATmega quedan libres (se usan para el monitor serie y para grabar el chip), y ningún pin de un chip de 5 V comparte fila con 12 V o 24 V. La tabla de 156 pasos del dossier, el esquemático, el diagrama 3D y el catálogo ya están actualizados.

**Corrección del DS1232 (pines 5 y 6):** el reset activo en bajo es el pin 6. Los cables W57 y W58 salen de G16 y H16, y el pin 5 (F17) queda libre.

---

## 2. Cómo grabar el programa (paso a paso)

### Por qué sin bootloader
El Arduino Uno trae un "bootloader" que, después de cada reinicio, espera cerca de 1 segundo por si llega un programa nuevo. El watchdog DS1232 no espera tanto: reiniciaría el chip una y otra vez y el programa nunca arrancaría. Por eso se graba con **"Subir usando programador"**, que elimina el bootloader.

### Lo que necesitas
- Un Arduino Uno que hará de programador (lo llamaremos "Uno programador").
- 6 cables puente y un condensador de 10 µF.
- El IDE de Arduino (versión 1.8 o 2.x).

### Pasos
1. **Convierte el Uno en programador.** Conéctalo por USB. En el IDE abre *Archivo → Ejemplos → 11.ArduinoISP → ArduinoISP* y súbelo de forma normal (botón Subir).
2. **Pon el condensador de 10 µF** entre RESET (+) y GND (−) del Uno programador. Evita que se reinicie solo durante la grabación.
3. **Conecta el Uno programador al ATmega de la protoboard.** El ATmega puede quedarse en su sitio:

   | Uno programador | ATmega328P (pin) | Hueco en la protoboard |
   |---|---|---|
   | D10 | RESET (1) | fila 50, lado A–E (por ejemplo A50) |
   | D11 | MOSI · PB3 (17) | fila 61, lado F–J (por ejemplo J61) |
   | D12 | MISO · PB4 (18) | fila 60, lado F–J (por ejemplo J60) |
   | D13 | SCK · PB5 (19) | fila 59, lado F–J (por ejemplo J59) |
   | GND | GND | riel T− o B− |

   Alimenta la protoboard con su propia fuente de 24 V y **no** conectes el 5 V del Uno a la protoboard (habría dos fuentes peleando). Quita la lámpara o la válvula de la bornera X3 mientras grabas.
4. **Abre el programa:** `FrenoMotorAuto/FrenoMotorAuto.ino`.
5. **Configura el IDE:** *Herramientas → Placa → Arduino Uno* y *Herramientas → Programador → Arduino as ISP*.
6. **Solo la primera vez con un chip nuevo:** *Herramientas → Quemar bootloader*. Esto configura el chip para usar el cristal de 16 MHz. Un chip nuevo de fábrica funciona a 1 MHz con su reloj interno.
7. **Graba el programa:** *Programa → Subir usando programador* (atajo Ctrl + Mayús + U). Espera el mensaje "Subido".
8. **Desconecta** los 5 cables del Uno programador.

### Cómo saber que quedó bien
- Al encender, el LED rojo se queda **encendido 1 segundo** (prueba de lámpara) y después se apaga.
- Con 1500 RPM simuladas, acelerador en 0.45 V y embrague suelto, la lámpara de prueba enciende.
- Si el LED se enciende 1 segundo, se apaga y vuelve a encenderse sin parar, el chip se está reiniciando: revisa el cable W59 del latido (D61 → G15) y que no hayas dejado el bootloader (repite el paso 7).

---

## 3. Mapa de pines

| Pin Arduino | Pin del chip | Hueco | Señal | Dirección |
|---|---|---|---|---|
| D2 (INT0) | 4 | E53 | TACH_F, pulsos del motor | Entrada |
| D3 | 5 | E54 | LED rojo de falla | Salida |
| D4 | 6 | E55 | CH_A, decisión del canal A | Entrada |
| D5 | 11 | E60 | ST del PROFET (1 = sin falla) | Entrada |
| D6 | 12 | E61 | Latido al DS1232 | Salida |
| D8 | 14 | E63 | MCU_EN, habilita el 74HCT08 | Salida |
| D9 | 15 | F63 | GATE del MOSFET Q1 | Salida |
| A0 | 23 | F55 | Acelerador (TPS) | Entrada analógica |
| A1 | 24 | F54 | Embrague | Entrada analógica |
| A2 | 25 | F53 | VFV, voltaje de RPM del LM2907 | Entrada analógica |
| A3 | 26 | F52 | SOL+ dividido entre 4.9 | Entrada analógica |
| A4 | 27 | F51 | Batería dividida entre 7.67 | Entrada analógica |
| D0, D1 | 2, 3 | E51, E52 | Monitor serie (solo en modo depuración) | Libres |
| D7, D10–D13, A5 | — | — | Sin uso: entrada con resistencia interna | Libres |

---

## 4. Cómo funciona por dentro

El programa repite el mismo trabajo cada **5 milisegundos**:

```
 ┌──────────────── cada 5 ms ─────────────────┐
 │ 1. Leer sensores  (RPM, TPS, embrague,      │
 │    batería, SOL+, VFV, CH_A, ST)            │
 │ 2. ¿Terminó el arranque de 1 s?             │
 │ 3. Diagnosticar fallas                      │
 │ 4. Decidir: ¿se cumplen las 3 condiciones   │
 │    durante 20 ms seguidos y no hay fallas?  │
 │ 5. Aplicar MCU_EN y GATE · LED · depuración │
 │ 6. Latido al DS1232 + reset del watchdog    │
 └─────────────────────────────────────────────┘
 En paralelo: la interrupción INT0 anota el tiempo entre pulsos del tacómetro.
```

Activar exige que las condiciones se mantengan 20 ms. Desactivar es inmediato, en el mismo ciclo.

| Bloque | Qué hace | ¿Lo tocarías? |
|---|---|---|
| 1 · Ajustes del usuario | Todos los valores de calibración | **Sí: aquí se hacen los ajustes** |
| 2 · Asignación de pines | Qué pin del chip hace qué | Solo si cambias el circuito |
| 3 · Constantes calculadas | Valores derivados y comprobaciones al compilar | No |
| 4 · Códigos de falla | Lista de fallas y orden de prioridad del LED | No |
| 5 · Tipos y variables | Estructuras de datos y prototipos | No |
| 6 · Arranque seguro | Guarda la causa del reinicio y apaga el watchdog interno antes de setup | No |
| 7 · Interrupción del tacómetro | Mide el tiempo entre pulsos y descarta el ruido | No |
| 8 · Utilidades de tiempo | Función `persiste`: "esta condición duró X ms" | No |
| 9 · Lectura de sensores | ADC con promedio y cálculo de RPM | No |
| 10 · Decisión del canal B | Las 3 condiciones con histéresis | Solo para cambiar la lógica |
| 11 · Diagnóstico | Detecta las 9 fallas | Solo para cambiar la lógica |
| 12 · Salidas | Mueve MCU_EN y GATE juntas | No |
| 13 · LED de falla | Códigos de parpadeo | No |
| 14 · EEPROM | Guarda la última falla y los contadores | No |
| 15 · Depuración | Mensajes al monitor serie | No |
| 16 · setup() | Configuración inicial en orden seguro | No |
| 17 · loop() | El ciclo de 5 ms descrito arriba | No |

---

## 5. Cómo hacer ajustes

Todos los ajustes están en el **BLOQUE 1**. Después de cambiar algo, vuelve a grabar (sección 2, paso 7).

> **Regla de oro:** el canal A (hardware) y el canal B (este programa) deben tener los mismos umbrales. Si cambias uno y no el otro, aparecerán las fallas 4, 5 o 9 y el sistema no frenará. La columna "En el hardware" dice qué debes cambiar también en el circuito.

| Ajuste | Valor | Qué controla | En el hardware |
|---|---|---|---|
| `SISTEMA_AUTOMATICO_HABILITADO` | 1 | 0 = el canal B nunca habilita la válvula | — |
| `MODO_DEPURACION` | 0 | 1 = envía datos por el monitor serie | — |
| `SISTEMA_24V` | 1 | 1 = batería válida de 18 a 32 V · 0 = de 9 a 16 V | TVS y fusible para 12 V (dossier 4.6) |
| `PRUEBA_WATCHDOG_MS` | 0 | Cuelgue intencional para probar el watchdog | — |
| `PULSOS_POR_VUELTA` | 4 | Pulsos del tacómetro por vuelta del motor | Condensador C13 (dossier, tabla 1.3.a) |
| `RPM_ACTIVAR` | 1300 | RPM a partir de las cuales se permite frenar | Trimmer RV2 (Vref2) |
| `RPM_LIBERAR` | 1170 | RPM bajo las que se suelta | Resistencia R19 (histéresis) |
| `TPS_SUELTO_V` | 1.30 | Bajo este voltaje el pedal cuenta como suelto | R14, R15, R16 |
| `TPS_PISADO_V` | 1.40 | Sobre este voltaje el pedal cuenta como pisado | R14, R15, R16 |
| `TPS_MINIMO_VALIDO` | 0.20 | Menos = cable roto o en corto a masa | R21, R22 |
| `TPS_MAXIMO_VALIDO` | 4.80 | Más = corto a +5 V | R23, R24 |
| `EMBRAGUE_SUELTO_MIN_V` / `MAX_V` | 3.10 / 3.90 | Ventana de "embrague suelto" | R9, R10 y la resistencia de 1 kΩ del pedal |
| `VOLTAJE_REFERENCIA_ADC` | 5.00 | Mide el riel de +5 V y escribe el valor real | — |
| `ANTIRREBOTE_ACTIVAR_MS` | 20 | Tiempo estable antes de frenar | — |
| `ARRANQUE_MS` | 1000 | Tiempo sin frenar al encender | — |

**Ejemplo: subir el umbral a 1400 RPM.** En el programa pon `RPM_ACTIVAR = 1400.0f` y `RPM_LIBERAR = 1260.0f`. En el circuito, con el generador NE555 a 93.3 Hz (1400 × 4 / 60), gira RV2 hasta que la salida RPM_OK (hueco F30) cambie de 0 a 5 V.

Si escribes un ajuste incoherente (por ejemplo `RPM_LIBERAR` mayor que `RPM_ACTIVAR`), el IDE no compila y muestra el motivo.

---

## 6. Códigos de falla del LED rojo

El LED parpadea N veces (medio segundo cada destello), hace una pausa de 1.5 s y repite.

| Destellos | Tipo | Qué significa | Qué revisar |
|---|---|---|---|
| 1 | Leve | Acelerador fuera de 0.20–4.80 V | Cable del TPS cortado o en corto, R7, R8, C11 |
| 2 | Leve | Embrague en corto a +5 V | Cableado del interruptor del pedal |
| 3 | Leve | Batería fuera de rango | Fuente, fusible F1, divisor R30/R31, `SISTEMA_24V` |
| 4 | Leve | RPM del canal A y del B no coinciden | C13, R12, alimentación de 12 V del LM2907, `PULSOS_POR_VUELTA` |
| 5 | Grave | Canal A dice "frenar" cuando claramente no corresponde | Comparadores U5/U6, 74HCT08, cable W52 |
| 6 | Grave | Hay tensión en la salida sin orden (PROFET pegado) | PROFET U7; si hay un interruptor manual en paralelo, ver nota |
| 7 | Grave | Se dio la orden y la salida no tiene tensión | PROFET U7, cable W51, fusible F2 |
| 8 | Grave | El PROFET avisa carga en corto o abierta | Válvula o lámpara, bornera X3, diodo D5 |
| 9 | Aviso | El canal A no responde cuando debería | Ajuste de RV2, comparadores, 74HCT08 |

- **Leve:** el sistema no frena mientras dure la falla y se recupera solo 2 s después de que desaparezca.
- **Grave:** el sistema queda bloqueado hasta apagar y encender. Así un defecto peligroso no se "esconde".
- **Nota sobre el interruptor manual:** el programa supone que solo el PROFET alimenta SOL+. Si conectas el interruptor original del tablero en paralelo, al usarlo aparecerá la falla 6. Consulta antes de hacer esa conexión.
- La EEPROM guarda la última falla, cuántas fallas hubo y cuántas veces reinició un watchdog. Se ven en el modo depuración, al arrancar.

---

## 7. Modo depuración (monitor serie)

1. En el BLOQUE 1 pon `#define MODO_DEPURACION 1` y graba.
2. Conecta un adaptador USB-serie de 5 V (FTDI o CH340): su **RX** al pin 3 del chip (hueco D52), y su **GND** a GND. El TX del adaptador al pin 2 (D51) es opcional.
3. Abre el monitor serie a **115200 baudios**. Verás una línea cada 250 ms:

```
Freno de motor automatico - canal B v1.0
Causa de reinicio MCUSR=0x3
Ultima falla registrada: 0
Fallas registradas: 0
Reinicios por watchdog: 0
t=1.48 RPM=1499 VFV=1.22 TPS=0.45 EMB=3.51 BAT=24.0 SOL=24.0 A=1 ST=1 EN=1 EST=NORMAL F=0x0
```

`A` es la decisión del canal A, `EN` la del canal B, y `F` las fallas activas en hexadecimal (bit 0 = falla 1, bit 1 = falla 2, y así).

Vuelve a `MODO_DEPURACION 0` antes de instalarlo en el camión.

---

## 8. Prueba del watchdog en el banco

1. Pon `#define PRUEBA_WATCHDOG_MS 5000` y graba.
2. Enciende con las condiciones de frenado cumplidas: la lámpara enciende.
3. A los 5 s el programa se cuelga a propósito. En menos de un cuarto de segundo el DS1232 lo reinicia: la lámpara se apaga y el LED rojo se enciende fijo 1 s. Como el programa vuelve a empezar, el ciclo se repite cada 5 s y algo.
4. Si quitas el cable W59 (latido) y se repite el cuelgue, el que reinicia es el watchdog interno, a los 125 ms.
5. Vuelve a `PRUEBA_WATCHDOG_MS 0` y graba de nuevo.

Mientras el programa está colgado, y hasta que el watchdog actúa (62 a 250 ms), las salidas quedan como estaban. En cuanto llega el reinicio, el DS1232 apaga la compuerta final y las resistencias R26 y R1 llevan MCU_EN y GATE a 0.

---

## 9. Verificación realizada

**Compilación** con avr-gcc 7.3 y el núcleo de Arduino AVR para Uno, con todos los avisos activados (`-Wall -Wextra`):

| Variante | Memoria de programa | Errores | Avisos |
|---|---|---|---|
| Configuración de fábrica | 10 592 bytes (33 %) | 0 | 0 |
| Depuración + sistema de 12 V | 12 880 bytes (40 %) | 0 | 0 |
| Sistema deshabilitado | 10 518 bytes (33 %) | 0 | 0 |
| Prueba de watchdog | 10 650 bytes (33 %) | 0 | 0 |

**Simulación** del archivo HEX en un ATmega328P emulado (avr8js), con un modelo del circuito: tacómetro, comparadores del canal A, PROFET, MOSFET, válvula y DS1232 con su tiempo de espera más corto (62.5 ms). Resultado: **57 de 57 comprobaciones correctas**.

| Medición | Resultado |
|---|---|
| Soltar el acelerador hasta habilitar | 24.5 ms |
| Corriente de la válvula al 90 % después de habilitar | 28.5 ms |
| Pisar el acelerador hasta cortar | 5.0 ms |
| Pisar el embrague hasta cortar | 5.0 ms |
| Bajar a 1100 RPM hasta cortar | 27 ms |
| Pérdida de la señal TACH hasta cortar | 14.5 ms |
| Cable del TPS roto hasta bloquear | 7 ms |
| Ruido de ±0.04 V en el TPS | 0 cambios de la válvula |
| Separación máxima entre latidos al DS1232 | 18.1 ms (límite 62.5 ms) |
| Cuelgue del programa hasta reinicio por DS1232 | dentro del tiempo del modelo |
| Cuelgue sin DS1232 hasta reinicio por watchdog interno | ≈ 125 ms |

Fallas comprobadas: TPS roto, embrague en corto a +5 V, batería baja, RPM no plausibles, pérdida de TACH, PROFET pegado (sin corriente en la válvula y bloqueo permanente), solenoide en corto (ST), canal A atascado, reinicio por el DS1232 y registro en la EEPROM.

**Límite de esta verificación:** la simulación reproduce el chip y un modelo del circuito, no el circuito físico. Antes de instalarlo en el camión, haz las pruebas de banco del dossier (sección 3.3) con el programa grabado.

---

## 10. Historial de versiones

| Versión | Cambios |
|---|---|
| 1.0 | Primera versión completa: canal B, 9 diagnósticos, LED de códigos, EEPROM, modo depuración, prueba de watchdog. Corrige la protoboard (divisores de sensado lejos del ATmega) y el pin del DS1232. |
