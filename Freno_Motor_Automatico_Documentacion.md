# Sistema Automático de Activación de Freno de Motor (Jake Brake) — Dossier de Ingeniería

**Versión:** 2.0 (arquitectura mejorada de grado automotriz) · **Norma de referencia:** ISO 26262 (concepto de seguridad funcional, ASIL B objetivo) · **Alimentación:** 24 V nominal (18–32 V), compatible 12 V con cambio de 3 componentes.

> **Lógica de activación (inalterable):** la válvula solenoide del freno de motor se energiza **solo** cuando, de forma simultánea:
> 1. RPM del motor ≥ **1300 RPM**
> 2. Pedal de acelerador suelto: V_TPS < **1.35 V** (histéresis **0.10 V**: activa por debajo de 1.30 V, libera por encima de 1.40 V)
> 3. Pedal de embrague **suelto**
>
> Y además la señal del TPS es **plausible** (0.20 V < V_TPS < 4.80 V), el embrague tiene cable sano, la tensión de batería es válida, el microcontrolador está vivo (watchdog) y **ambos canales independientes** (analógico y digital) coinciden.

Archivos entregados:

| Archivo | Contenido |
|---|---|
| `Freno_Motor_Automatico_Documentacion.md` | Entregables 1 a 4 (este documento) |
| `freno_motor_simulador_3d.html` | Entregable 5: simulador web 3D (Three.js) de un solo archivo |

---

## ENTREGABLE 1 — Arquitectura mejorada, justificación de seguridad y BOM

### 1.1 Estado seguro y filosofía de diseño (ISO 26262)

| Elemento | Decisión |
|---|---|
| **Evento peligroso principal** | Pérdida de frenos por *brake fade* en descenso (el sistema debe **ayudar** a frenar). |
| **Evento peligroso secundario** | Activación **espuria** del freno de motor con embrague pisado o acelerador pisado (bloqueo de ruedas motrices en piso mojado, calado, pérdida de tracción). |
| **Estado seguro (safe state)** | Válvula **desenergizada**. El freno de motor sigue disponible manualmente por el interruptor original de tablero, que queda **en paralelo** (el sistema automático añade, nunca quita). |
| **Arquitectura** | **2oo2 para activar** (dos canales deben decir "sí") y **1oo2 para cortar** (cualquiera corta). Canal A = analógico (comparadores), Canal B = microcontrolador con watchdog externo. |
| **Dos interruptores de potencia en serie** | PROFET lado alto (mandado por A·B·WD) + MOSFET lado bajo (mandado solo por B). Un PROFET pegado en corto no puede activar el freno por sí solo. |
| **Fail-safe de cableado** | Cable TPS roto o en corto a masa → V_TPS < 0.20 V → **bloqueo**. Corto a +5 V/+24 V → V_TPS > 4.80 V → bloqueo. Cable de embrague roto o en corto a masa → 0 V → bloqueo. Interruptor codificado por resistencia (4.12 V nominal) para distinguir "suelto" de "corto a +5 V". |
| **Tiempo de reacción objetivo** | < 60 ms desde que el conductor suelta el acelerador hasta plena corriente en la válvula (un humano tarda 1–2 s en recordar y accionar el interruptor). |

### 1.2 Qué se reemplazó respecto al diseño base y por qué salva vidas

| Bloque | Diseño base | Diseño mejorado | Por qué es superior |
|---|---|---|---|
| Convertidor F/V | LM2907 | **LM2907N-8 conservado como canal analógico** (probado, 6–28 V) **+ medición digital de periodo en el MCU** (canal B) | El MCU mide el periodo pulso a pulso (resolución 0.1 RPM, 12–25 ms de latencia a 1300 RPM). El LM2907 queda como canal independiente con tecnología distinta → **diversidad** que evita fallas de causa común. |
| Comparador | LM393, histéresis 0.10 V | **LM393 (versión -Q1 / LM2903B-Q1 en PCB) con histéresis calculada exacta de 0.10 V**, más **comparador de ventana** (U6) para detectar cable roto/corto | El comparador de ventana convierte una falla de cableado en un bloqueo seguro. El -Q1 está calificado AEC-Q100 (−40…125 °C, vibración, humedad). |
| Transistor + relé | 2N2222 + relé 12 V (10–15 ms, contactos que rebotan con vibración y se oxidan) | **PROFET BTS432E2 / BTT6020-1ERA** (interruptor inteligente lado alto, t_on ≈ 150 µs, protección contra cortocircuito, sobretemperatura, batería invertida, salida de diagnóstico ST) **+ MOSFET IRLZ44N / IPD50N06S4L-12** lado bajo | Sin partes móviles: inmune a vibración, 100 veces más rápido, se autoprotege ante un solenoide en corto y **avisa al MCU** (pin ST) si la carga está abierta o en corto. |
| Regulador | 7812 lineal (disipa 3–6 W a 24–32 V, se calienta, no soporta load-dump) | **Buck OKI-78SR-12 (7–36 V, 1 A, η≈90 %)** en protoboard; **TPS54360B-Q1 / LMR36015-Q1 (hasta 60 V)** en PCB, seguido de LDO **TPS7B6950-Q1** (5 V, 40 V de entrada, AEC-Q100) | Soporta 18–32 V sin calentarse; el LDO sobrevive transitorios de 40 V. |
| Lógica de decisión | Un solo comparador | **Canal A analógico + Canal B MCU ATmega328P (AEC-Q100 disponible: ATmega328P-15AZ)** con watchdog interno **y externo DS1232 / TPS3823-Q1** | Si el firmware se cuelga, el watchdog externo quita la habilitación en ≤150 ms. El MCU además vigila plausibilidad (VFV vs. periodo), tensión de batería, estado del PROFET y retroalimentación de la salida. |
| Protección eléctrica | Ninguna | **TVS 1.5KE33CA / SMCJ33CA (load dump ISO 7637-2 pulso 5)**, Schottky de batería invertida, filtro π LC, clamp 1N4148/BAV99 en cada entrada, zener 5.1 V en TACH, fusibles 2 A (lógica) y 7.5 A (actuador) | Un pico de alternador de 80–100 V no destruye la electrónica ni provoca una activación falsa. |
| Compuerta final | — | **74HC08-Q1 (4 × AND)** | Decisión final por hardware combinacional (sin software en el camino crítico): RPM_OK · TPS_GO · CLUTCH · MCU_EN · WD_OK. |

### 1.3 Cálculos de calibración

#### a) Frecuencia de la señal de RPM
Se asume la salida de tacómetro del ECM / sensor con **PPR = 4 pulsos por revolución** (ajustable por firmware y por R1/C1). 

f = RPM × PPR / 60 → **f(1300 RPM) = 1300 × 4 / 60 = 86.67 Hz** · f(2500 RPM) = 166.7 Hz · f(600 RPM ralentí) = 40 Hz.

| PPR del camión | f a 1300 RPM | C1 recomendado (R1 = 68 kΩ) |
|---|---|---|
| 1 | 21.67 Hz | 56 nF + 4.7 nF |
| 2 | 43.33 Hz | 30 nF (2 × 15 nF) |
| **4** | **86.67 Hz** | **15 nF** |
| 6 | 130 Hz | 10 nF |
| 12 | 260 Hz | 4.7 nF |

#### b) Convertidor F/V LM2907N-8 (canal A)
V_OUT = V_CC · f_IN · R1 · C1, con V_CC = 12.0 V (buck regulado ±2 %), R1 = 68 kΩ (1 %), C1 = 15 nF (film 5 %).

**V_OUT(1300 RPM) = 12 × 86.67 × 68 000 × 15×10⁻⁹ = 1.061 V** (sensibilidad **0.816 mV/RPM**).
V_OUT(2500 RPM) = 2.04 V; V_OUT(3960 RPM, máximo del generador de prueba) = 3.23 V — siempre dentro del rango de modo común del LM393 (0 a V_CC − 1.5 V = 3.5 V).

Rizado: ΔV = (V_CC/2)·(C1/C2)·(1 − V_CC·f·C1/I2) = 6 × (15 nF/1 µF) × (1 − 12 × 86.67 × 15×10⁻⁹ / 180×10⁻⁶) = **82 mV pico-pico**.
Constante de tiempo del filtro: τ = R1·C2 = 68 kΩ × 1 µF = **68 ms** (respuesta al 90 % en 157 ms; irrelevante porque las RPM varían lentamente, lo que define el tiempo de reacción es la caída del TPS).

#### c) Comparador de RPM (U5-B) con histéresis
Configuración no inversora: VFV → R_S = 10 kΩ → IN+; realimentación R_F2 = 470 kΩ desde OUT2 (pull-up 2.2 kΩ a 5 V) a IN+; Vref2 en IN− (trimmer).

Histéresis: ΔV_H = 5 V × R_S/(R_S + R_F2) = 5 × 10/480 = **0.104 V** (≈ **128 RPM**).
Umbral de disparo (salida baja → la red R_S/R_F2 atenúa): V_IN+ = VFV × 470/480 = 0.979·VFV = Vref2 → **Vref2 = 1.061 × 0.979 = 1.039 V** (se ajusta con el trimmer RV2 hasta que OUT2 conmute exactamente a 86.67 Hz).
Umbral de liberación: VFV = (1.039 − 0.104)/0.979 = 0.955 V → **1170 RPM**. El freno se activa a 1300 RPM y no "parpadea" hasta bajar de 1170 RPM.

#### d) Comparador TPS (U5-A): umbral 1.35 V, histéresis 0.10 V
Configuración inversora (OUT1 alto cuando V_TPS < Vref1). Divisor R_H1 = 7.5 kΩ (a 5 V), R_L1 = 2.7 kΩ (a GND), realimentación R_F1 = 100 kΩ desde OUT1 (pull-up 2.2 kΩ).

V_nom = 5 × 2.7/(7.5 + 2.7) = **1.3235 V**; R_TH = 7.5 ∥ 2.7 = 1.985 kΩ; k = R_TH/(R_TH + R_F1) = 1.985/101.985 = 0.01946.
Umbral para activar (salida baja, pedal se va soltando): V_LO = V_nom × (1 − k) = **1.298 V ≈ 1.30 V**.
Umbral para liberar (salida alta, pedal se pisa): V_HI = V_nom + (5 − V_nom) × k = 1.3235 + 3.6765 × 0.01946 = **1.395 V ≈ 1.40 V**.
Centro = **1.346 V ≈ 1.35 V**, histéresis = **0.097 V ≈ 0.10 V** ✔ (error < 3 %, resistencias 1 %).

#### e) Ventana fail-safe del TPS (U6)
Ref. baja: 5 V × 1 kΩ/(24 kΩ + 1 kΩ) = **0.200 V** → OUT1 alto si V_TPS > 0.20 V.
Ref. alta: 5 V × 24 kΩ/(1 kΩ + 24 kΩ) = **4.800 V** → OUT2 alto si V_TPS < 4.80 V.
Las tres salidas de colector abierto (U5-A, U6-A, U6-B) comparten el mismo pull-up: **TPS_GO = (V_TPS < 1.35 V) AND (V_TPS > 0.20 V) AND (V_TPS < 4.80 V)** sin compuertas adicionales.
Con el cable roto, el pull-down R8 = 220 kΩ lleva la entrada a 0 V (< 0.20 V) → bloqueo. Error introducido por R8 en operación normal: 1.35 V × 1 kΩ/221 kΩ = 6 mV (despreciable).

#### f) Entrada de embrague codificada
Interruptor **normalmente cerrado con el pedal suelto**, alimentado desde +5V_SW de la tarjeta con 1 kΩ en serie en el pedal; en la tarjeta R9 = 1 kΩ y pull-down R10 = 4.7 kΩ.
Suelto: V = 5 × 4.7/(1 + 1 + 4.7) = **3.51 V** → nivel alto 74HC (V_IH = 3.5 V a 5 V, usar 74HC08 con V_IH typ 2.4 V o 74HCT08: V_IH = 2.0 V, recomendado **74HCT08**). Pisado (abierto): 0 V. Cable roto: 0 V. Corto a masa: 0 V. Corto a +5 V: 5.0 V → el MCU lo detecta fuera de la ventana [3.1 V, 3.9 V] y bloquea.

#### g) Tiempo de respuesta estimado (soltar el acelerador a RPM ≥ 1300)
| Etapa | Tiempo |
|---|---|
| Filtro RC TPS (1 kΩ · 1 µF) | 1.0 ms (τ) |
| Comparador LM393 + 74HC08 | < 2 µs |
| Canal B: muestreo ADC 1 kHz + promedio 8 + antirrebote 20 ms | ≈ 28 ms |
| PROFET t_on | 0.15 ms |
| Solenoide (L/R ≈ 12 ms) al 90 % de corriente | 28 ms |
| **Total** | **≈ 57 ms** (vs. 1–2 s de un conductor) |

#### h) Solenoide y disipación
Válvula típica Jacobs 24 V: 12 Ω → **2.0 A** (3 solenoides en paralelo: 6 A). PROFET BTS432E2 R_DS(on) = 30 mΩ → P = 6² × 0.03 = **1.1 W** (sin disipador en PCB con área de cobre 4 cm²). MOSFET IRLZ44N 22 mΩ → 0.8 W. Diodo de rueda libre SB560: energía ½·L·I² = ½ × 0.15 H × 6² = 2.7 J por corte → pulso < 10 ms, admisible.

### 1.4 Lista de materiales (BOM) — prototipo protoboard y PCB final

| Ref. | Cant. | Descripción | Referencia comercial (protoboard) | Referencia automotriz (PCB final) | Función |
|---|---|---|---|---|---|
| X1 | 1 | Bornera 2 pines 2.54 mm | KF128-2.54-2P | Conector Deutsch DT04-4P (BAT+, GND, SOL+, SOL−) | Entrada de batería / salida de potencia |
| X2 | 1 | Bornera 4 pines 2.54 mm | KF128-2.54-4P | Deutsch DT04-6P (TPS, TACH, CLUTCH, +5V_SW, GND_S, reserva) | Sensores |
| X3 | 1 | Bornera 2 pines 2.54 mm | KF128-2.54-2P | (incluido en DT04-4P) | Solenoide |
| F1 | 1 | Fusible 2 A (lógica) | Portafusible aéreo + 2 A ATO | Fusible SMD 2 A Littelfuse 0453002 | Protección alimentación lógica |
| F2 | 1 | Fusible 7.5 A (actuador) | Portafusible ATO 7.5 A en arnés | Portafusible ATO 7.5 A en arnés | Protección rama de potencia |
| TVS1 | 1 | TVS 33 V bidireccional 1500 W | 1.5KE33CA | SMCJ33CA (SMC) ×2 en paralelo | Load dump, picos de alternador |
| D1 | 1 | Schottky 3 A 60 V | SR360 | SS36 / SM360 | Batería invertida (lógica) |
| D5 | 1 | Schottky 5 A 60 V | SB560 | SS5P6-M3 (PowerDI) | Rueda libre del solenoide |
| D6, D7 | 2 | Diodo señal | 1N4148 | BAV99 | Clamp entradas TPS / CLUTCH a 5 V |
| DZ1 | 1 | Zener 5.1 V 1 W | 1N4733A | SMAZ5V1 | Clamp entrada TACH |
| U1 | 1 | Buck 12 V 1 A, 7–36 V | OKI-78SR-12/1.0-W36-C (Murata, SIP-3) | TPS54360B-Q1 (60 V, 3.5 A) + L 33 µH + MBRS360 | 12 V regulado |
| U2 | 1 | LDO 5 V | L7805CV | TPS7B6950QDCYRQ1 (40 V in, AEC-Q100) | 5 V lógica/analógica |
| U3 | 1 | Temporizador (solo laboratorio) | NE555P | — (no va en PCB) | Generador de pulsos TACH de prueba |
| U4 | 1 | Convertidor F/V | LM2907N-8 | LM2907M-8 (SOIC) | Canal A: RPM → voltaje |
| U5, U6 | 2 | Comparador dual | LM393P | LM2903BQDRQ1 (AEC-Q100) | Umbrales 1.35 V / 1300 RPM y ventana fail-safe |
| U7 | 1 | PROFET lado alto | BTS432E2 (TO-220-5) + adaptador 2.54 mm | BTT6020-1ERA (PROFET+ 24 V, 20 mΩ, PG-TDSO-14) | Interruptor inteligente de la válvula |
| Q1 | 1 | MOSFET N lógico | IRLZ44N (TO-220) | IPD50N06S4L-12 (DPAK, AEC-Q101) | Segundo corte en serie (lado bajo) |
| U8 | 1 | Cuádruple AND | SN74HCT08N | SN74HCT08QDRQ1 | Lógica final por hardware |
| U9 | 1 | Microcontrolador | ATmega328P-PU (DIP-28) + cristal 16 MHz | ATmega328P-15AZ (AEC-Q100, TQFP) | Canal B, diagnóstico, watchdog interno |
| U10 | 1 | Supervisor/watchdog | DS1232LP (DIP-8) | TPS3823-50QDBVRQ1 | Reset si el firmware se cuelga |
| Y1 | 1 | Cristal 16 MHz + 2 × 22 pF | HC-49S | ABLS-16.000MHZ (AEC) | Reloj MCU |
| RV1 | 1 | Potenciómetro 100 kΩ lineal | WH148-100K | — (laboratorio) | Simula RPM |
| RV2 | 1 | Trimmer 10 kΩ multivuelta | 3296W-1-103 | 3224W-1-103 (SMD) | Ajuste fino 1300 RPM |
| R1, R3, R13, R25, R26, R29 | 6 | 10 kΩ 1 % | CF/MF 1/4 W | 0603 1 % AEC-Q200 | Pull-up/down, divisores |
| R2 | 1 | 150 Ω | 1/4 W | 0805 | GND PROFET (batería invertida) |
| R4, R10 | 2 | 4.7 kΩ | 1/4 W | 0603 | 555 RB / pull-down embrague |
| R5, R17, R20 | 3 | 2.2 kΩ | 1/4 W | 0603 | RA 555 / pull-ups comparadores |
| R6, R18 | 2 | 10 kΩ | 1/4 W | 0603 | Serie TACH / Rs RPM |
| R7, R9, R22, R23 | 4 | 1 kΩ 1 % | 1/4 W | 0603 1 % | Serie TPS/CLUTCH, divisores 0.20/4.80 V |
| R8 | 1 | 220 kΩ | 1/4 W | 0603 | Pull-down detección cable roto |
| R11 | 1 | 100 kΩ | 1/4 W | 0603 | Polarización entrada AC LM2907 |
| R12 | 1 | 68 kΩ 1 % | MF 1/4 W | 0603 1 % 25 ppm | R1 ganancia F/V |
| R14 | 1 | 7.5 kΩ 1 % | MF 1/4 W | 0603 1 % | Divisor 1.35 V (alto) |
| R15 | 1 | 2.7 kΩ 1 % | MF 1/4 W | 0603 1 % | Divisor 1.35 V (bajo) |
| R16 | 1 | 100 kΩ 1 % | MF 1/4 W | 0603 | RF histéresis TPS 0.10 V |
| R19 | 1 | 470 kΩ | 1/4 W | 0603 | RF histéresis RPM |
| R21, R24 | 2 | 24 kΩ 1 % | MF 1/4 W | 0603 1 % | Divisores 0.20 / 4.80 V |
| R27 | 1 | 100 Ω | 1/4 W | 0603 | Serie gate Q1 |
| R28 | 1 | 39 kΩ | 1/4 W | 0603 | Sensado SOL+ (alto) |
| R30 | 1 | 100 kΩ | 1/4 W | 0603 | Sensado VBAT (alto) |
| R31 | 1 | 15 kΩ | 1/4 W | 0603 | Sensado VBAT (bajo) |
| R32 | 1 | 330 Ω | 1/4 W | 0603 | LED falla |
| C1 | 1 | 100 µF 50 V electrolítico | Panasonic FR | Polímero 100 µF 50 V AEC | Filtro entrada |
| C3, C6 | 2 | 10 µF 25 V | Electrolítico / cerámico X7R | 10 µF 50 V X7R 1210 | Filtro 12 V / 5 V |
| C4 | 1 | 330 nF | Cerámico | 0603 X7R | Entrada 7805 |
| C2, C5, C9, C10, C12, C15–C22, C25, C26 | 16 | 100 nF | Cerámico X7R 50 V | 0603 X7R 50 V | Desacoples y filtros |
| C7 | 1 | 10 nF | Cerámico | — | CV del 555 |
| C8 | 1 | 470 nF | Film | — | Temporización 555 |
| C11, C14 | 2 | 1 µF | Film / cerámico X7R | 0805 X7R | Filtro TPS / C2 del LM2907 |
| C13 | 1 | 15 nF 5 % | Film poliéster | 0805 NP0/C0G | C1 bomba de carga (precisión) |
| C23, C24 | 2 | 22 pF | Cerámico NP0 | 0603 NP0 | Cristal |
| LED1 | 1 | LED verde 24 V con resistencia interna | LED 5 mm 24 V | LED 0805 + 2.2 kΩ | Indicador ACTUADOR ACTIVO |
| LED2 | 1 | LED rojo 5 mm | — | LED 0805 | Indicador FALLA |
| Cables | 1 | Kit puentes 22 AWG (colores) | — | — | Protoboard |
| Actuador | 1–3 | Válvula solenoide freno de motor 24 V, 12 Ω | Jacobs 1021955 / equivalente OEM | Igual | Carga |

> **Importante para el laboratorio:** la protoboard soporta ≈1 A por contacto. Las pruebas en protoboard se hacen con una **lámpara de 24 V/10 W** o un **relé de 24 V** como carga de prueba (≤ 0.5 A). La válvula real (2–6 A) solo se conecta a la **PCB final**.

---

## ENTREGABLE 2 — Diagrama esquemático completo y mapa de conexiones pin por pin

### 2.1 Diagrama de bloques del esquemático

```
 BAT+ 24V ──F2 7.5A──────────────────────────────────────────────┐
   │                                                              │ Vbb
   ├─F1 2A─┬─TVS1 1.5KE33CA─┐                                     ▼
   │       └─D1 SR360─►V_IN─┤C1 100µ ─┤C2 100n            ┌──────────────┐
   │                        │                             │ U7 PROFET    │ ST ──10k──5V ──► MCU PD5
   │                        ▼                             │ BTS432E2     │
   │              ┌──────────────────┐  12V   ┌────────┐  │ IN ◄── U8 Y4 │
   │              │U1 OKI-78SR-12    │───┬───►│U2 7805 │─►│ OUT ─────────┼──► SOL+ ──┐
   │              │Buck 7–36V→12V    │   │    │  5V    │  └──────────────┘           │ Válvula
   │              └──────────────────┘   │    └───┬────┘   GND─150Ω─┘                 ⊥ freno motor
   │                                     │        │  5V (riel T+ y B+)                │ 12Ω/24V
   │                                     ▼        ▼                       D5 SB560 ◄──┤
   │   U4 LM2907N-8 (V+=12V)   U5 LM393 (5V)  U6 LM393 (5V)                           │ SOL−
   │                                                                             Q1 IRLZ44N D ◄┘
   │                                                                              G ◄─100Ω─ MCU PB1
   │                                                                              S ─► GND
   │
 TACH ─10k─┬─DZ1 5.1V─┬─100n─┐    ┌─────────────────┐  VFV
   (X2.3)  │ GND      GND    ├100n┤1 U4 LM2907    4├──┬─10k(RE)─GND
           └──────────────► MCU PD2 (INT0)         7├──┘ (seguidor)      ┌──────────────┐
                        100k─GND  2├─15nF─GND   5,6├─12V                │  U5-B RPM     │
                                   3├─68k‖1µF─GND  8├─GND          VFV──10k─►IN2+(5)    │
                                                                          470k◄──┘OUT2(7)├─2.2k─5V ──► U8 pin1 (RPM_OK)
                                                                   Vref2 1.039V(trimmer)─►IN2−(6)│
 TPS ─1k──┬─1µF─GND ┬─220k─GND ┬─1N4148─►5V                              └──────────────┘
  (X2.2)  └─────────┴──────────┴──► TPS_F ──────────► IN1−(2) U5-A  OUT1(1)──┬─2.2k─5V ──► U8 pin2 (TPS_GO)
                     5V─7.5k─┬─2.7k─GND  Vref1 1.3235V ──────► IN1+(3)        │
                             └──────────────100k (RF)─────────────────────────┘
          TPS_F ──► IN1+(3) U6-A ◄── 0.20V(2)  OUT1(1) ─┐ colector abierto, AND cableado con TPS_GO
          TPS_F ──► IN2−(6) U6-B ◄── 4.80V(5)  OUT2(7) ─┘
 CLUTCH ─1k─┬─4.7k─GND ┬─100n─GND ┬─1N4148─►5V ──► CLUTCH_F ──► U8 pin5 y MCU PC1 (A1)
  (X2.4)   (3.51V suelto · 0V pisado/roto/corto)
 +5V_SW (X2.1) ◄── 5V (alimenta el interruptor de embrague con 1kΩ en serie en el pedal)

 U8 74HCT08:  Y1 = RPM_OK·TPS_GO → A2 ;  Y2 = Y1·CLUTCH_F = CH_A → A3 (y MCU PD4)
              Y3 = CH_A·MCU_EN(PB0) → A4 ;  Y4 = Y3·WD_OK(/RST DS1232) ──► U7 IN
 U10 DS1232:  ST ◄── MCU PD6 (latido cada 50 ms) ; /RST ──► MCU RESET y U8 pin13 (WD_OK)
 U9 ATmega328P: A0=TPS_F  A1=CLUTCH_F  A2=VFV  A3=SOL+/4.9  A4=VBAT/7.67  PD2=TACH  PD4=CH_A  PD5=ST
                PB0=MCU_EN (pull-down 10k)  PB1=GATE Q1  PD6=WDI  PD3=LED FALLA
```

### 2.2 Mapa de pines — Alimentación

| Dispositivo | Pin | Nombre | Conecta a |
|---|---|---|---|
| X1 bornera | 1 | BAT+ | Fusible F1 2 A ← batería 24 V; también a TVS1 (G1), D1 ánodo (I1), Vbb U7 (cable H1→C20), R30 100 kΩ (sensado VBAT) |
| X1 bornera | 2 | BAT− | Riel GND (B−) |
| TVS1 1.5KE33CA | 1–2 | — | BAT+ ↔ GND |
| D1 SR360 | Ánodo | — | BAT+ (I1) |
| D1 SR360 | Cátodo | — | V_IN (I3): C1 +, C2, U1 pin 1 |
| U1 OKI-78SR-12 | 1 | Vin | V_IN (J3) |
| U1 | 2 | GND | GND (J4 → B−) |
| U1 | 3 | Vout 12 V | C3 +, U2 pin 1 (cable H5→H7), U4 pin 6 V+ (cable G5→J25) |
| U2 L7805 | 1 | IN | 12 V (J7), C4 330 nF |
| U2 | 2 | GND | GND (J8 → B−) |
| U2 | 3 | OUT 5 V | C5, C6, rieles T+ y B+ (cables F9→T+9, G9→B+9) |

### 2.3 Mapa de pines — U4 LM2907N-8 (convertidor F/V, canal A)

| Pin | Nombre | Hueco | Conecta a |
|---|---|---|---|
| 1 | TACH+ (entrada) | E23 | C10 100 nF (acople AC desde TACH_F F22) y R11 100 kΩ a GND (C23) |
| 2 | CP1 (condensador de bomba) | E24 | C13 15 nF a GND (A24) |
| 3 | CP2 / IN+ op-amp | E25 | R12 68 kΩ a GND (D25) ‖ C14 1 µF a GND (C25) |
| 4 | EMIT (salida seguidor) = **VFV** | E26 | R13 10 kΩ a GND (A26); pin 7 (cable G24→D26); R18 10 kΩ → U5 pin 5 (vía J33/I33→I32); MCU A2/PC2 (cable C26→G53) |
| 5 | COL (colector) | F26 | 12 V (cable I26→I25 = pin 6) |
| 6 | V+ | F25 | 12 V (cable G5→J25), C15 100 nF a GND (G25) |
| 7 | IN− op-amp | F24 | Pin 4 EMIT (seguidor unitario, cable G24→D26) |
| 8 | GND / TACH− | F23 | GND (cable G23→B−23) |

### 2.4 Mapa de pines — U5 LM393 (TPS 1.35 V y RPM 1300)

| Pin | Nombre | Hueco | Conecta a |
|---|---|---|---|
| 1 | OUT1 = **TPS_GO** | E29 | R17 2.2 kΩ → 5 V (C29); R16 100 kΩ → pin 3 (D29→D31); U8 pin 2 (cable B29→D44); U6 pin 1 (cable D37→A29) y U6 pin 7 (cable G38→C37) [AND cableado] |
| 2 | IN1− | E30 | **TPS_F** (cable J28→D30); U6 pin 3 (cable C30→D39); MCU A0/PC0 (cable B30→G55) |
| 3 | IN1+ = Vref1 1.3235 V | E31 | R14 7.5 kΩ → 5 V (C31); R15 2.7 kΩ → GND (B31); C17 100 nF → GND (A31); R16 desde pin 1 |
| 4 | GND | E32 | GND (A32→T−32) |
| 5 | IN2+ | F32 | R18 10 kΩ desde VFV (I33→I32); R19 470 kΩ desde pin 7 (H32→H30) |
| 6 | IN2− = Vref2 1.039 V | F31 | Cursor trimmer RV2 (cable C7→G31); C18 100 nF → GND (H31) |
| 7 | OUT2 = **RPM_OK** | F30 | R20 2.2 kΩ → 5 V (G30); R19 470 kΩ → pin 5; U8 pin 1 (cable I30→D43) |
| 8 | VCC | F29 | 5 V (J29→B+29); C16 100 nF → GND (G29) |

### 2.5 Mapa de pines — U6 LM393 (ventana fail-safe 0.20 V < TPS < 4.80 V)

| Pin | Nombre | Hueco | Conecta a |
|---|---|---|---|
| 1 | OUT1 (TPS > 0.20 V) | E37 | Nodo TPS_GO (cable D37→A29) — colector abierto |
| 2 | IN1− = 0.200 V | E38 | R21 24 kΩ → 5 V (D38); R22 1 kΩ → GND (C38) |
| 3 | IN1+ = TPS_F | E39 | TPS_F (cable C30→D39); pin 6 (cable C39→G39) |
| 4 | GND | E40 | GND (A40→T−40) |
| 5 | IN2+ = 4.800 V | F40 | R23 1 kΩ → 5 V (G40); R24 24 kΩ → GND (H40) |
| 6 | IN2− = TPS_F | F39 | Pin 3 (cable C39→G39) |
| 7 | OUT2 (TPS < 4.80 V) | F38 | Nodo TPS_GO (cable G38→C37) — colector abierto |
| 8 | VCC | F37 | 5 V (J37→B+37); C19 100 nF → GND (G37) |

### 2.6 Mapa de pines — U8 74HCT08 (lógica final)

| Pin | Nombre | Hueco | Conecta a |
|---|---|---|---|
| 1 | 1A | E43 | RPM_OK (U5 pin 7, cable I30→D43) |
| 2 | 1B | E44 | TPS_GO (U5 pin 1, cable B29→D44) |
| 3 | 1Y | E45 | Pin 4 (cable D45→D46) |
| 4 | 2A | E46 | Pin 3 |
| 5 | 2B | E47 | CLUTCH_F (cable J42→D47); MCU A1/PC1 (cable C47→G54) |
| 6 | 2Y = **CH_A** | E48 | Pin 9 (cable D48→G48); MCU PD4 (cable C48→D55) |
| 7 | GND | E49 | GND (A49→T−49) |
| 8 | 3Y | F49 | Pin 12 (cable G49→G45) |
| 9 | 3A | F48 | CH_A (pin 6) |
| 10 | 3B | F47 | MCU_EN = MCU PB0 (cable D63→G47) |
| 11 | 4Y = **PROFET_IN** | F46 | U7 pin 2 IN (cable G46→C19) |
| 12 | 4A | F45 | Pin 8 |
| 13 | 4B | F44 | WD_OK = /RST DS1232 (cable H17→G44) |
| 14 | VCC | F43 | 5 V (J43→B+43); C20 100 nF → GND (G43) |

### 2.7 Mapa de pines — U7 BTS432E2 PROFET y Q1 IRLZ44N

| Dispositivo | Pin | Nombre | Hueco | Conecta a |
|---|---|---|---|---|
| U7 BTS432E2 | 1 | GND | B18 | R2 150 Ω → GND (A18→T−18) |
| U7 | 2 | IN | B19 | U8 pin 11 (cable G46→C19) |
| U7 | 3 | Vbb (unido al tab) | B20 | BAT+ tras F2 (cable H1→C20) |
| U7 | 4 | ST (diagnóstico, bajo = falla) | B21 | R3 10 kΩ → 5 V (A21→T+21); MCU PD5 (cable C21→D60) |
| U7 | 5 | OUT | B22 | SOL+ (cable C22→B1): X3 pin 1, D5 cátodo, LED1 ánodo, R28 39 kΩ (sensado) |
| Q1 IRLZ44N | 1 | G (gate) | B3 | R27 100 Ω ← MCU PB1 (G63→A41, B41→B42, C42→C3); R1 10 kΩ → GND (A3) |
| Q1 | 2 | D (drenador) | B4 | SOL− (cable A2→A4): X3 pin 2, D5 ánodo, LED1 cátodo |
| Q1 | 3 | S (fuente) | B5 | GND (A5→T−5) |

### 2.8 Mapa de pines — U9 ATmega328P-PU (canal B)

| Pin | Nombre | Hueco | Función en el sistema | Conecta a |
|---|---|---|---|---|
| 1 | /RESET | E50 | Reset por watchdog externo | R25 10 kΩ → 5 V (D50); U10 pin 5 (cable G17→C50) |
| 2 | PD0 | E51 | (entrada alta-Z, nodo auxiliar) | SOL+ (cable A1→D51); R28 39 kΩ |
| 3 | PD1 | E52 | (entrada alta-Z, nodo auxiliar) | Nodo sensado: R28/R29 10 kΩ → GND (B52); cable A52→G52 (A3) |
| 4 | PD2 / INT0 | E53 | Medición de periodo TACH | TACH_F (cable J22→D53) |
| 5 | PD3 | E54 | LED FALLA | Cable D54→J36 → R32 330 Ω (I36→I35) → LED2 (H35/H34) → GND (G34→B−34) |
| 6 | PD4 | E55 | Lectura de CH_A (plausibilidad) | U8 pin 6 (cable C48→D55) |
| 7 | VCC | E56 | 5 V | T+ (A56); C22 100 nF (B56→T−56) |
| 8 | GND | E57 | GND | T− (A57) |
| 9 | XTAL1 | E58 | Cristal 16 MHz | Y1 (D58); C23 22 pF (C58→T−58) |
| 10 | XTAL2 | E59 | Cristal 16 MHz | Y1 (D59); C24 22 pF (C59→T−59) |
| 11 | PD5 | E60 | Diagnóstico PROFET (ST) | U7 pin 4 (cable C21→D60) |
| 12 | PD6 | E61 | WDI: latido al watchdog | U10 pin 7 ST (cable D61→G15) |
| 13 | PD7 | E62 | Reserva (zumbador) | — |
| 14 | PB0 | E63 | **MCU_EN** (habilitación canal B) | U8 pin 10 (cable D63→G47); R26 10 kΩ → GND (C63) |
| 15 | PB1 | F63 | **GATE** lado bajo | Cable G63→A41 → R27 100 Ω → Q1 gate |
| 16 | PB2 | F62 | Reserva | — |
| 17 | PB3 | F61 | (entrada alta-Z, nodo auxiliar) | BAT+ (cable F1→J61); R30 100 kΩ |
| 18 | PB4 | F60 | (entrada alta-Z, nodo auxiliar) | Nodo VBAT: R30/R31 15 kΩ → GND (H60); cable G60→G51 (A4) |
| 19 | PB5 | F59 | Reserva (ISP SCK) | — |
| 20 | AVCC | F58 | 5 V analógico | B+ (J58); C25 100 nF (G58→B−58) |
| 21 | AREF | F57 | Referencia ADC | C26 100 nF → GND (G57) |
| 22 | GND | F56 | GND | B− (G56) |
| 23 | PC0 / A0 | F55 | ADC TPS | TPS_F (cable B30→G55) |
| 24 | PC1 / A1 | F54 | ADC embrague | CLUTCH_F (cable C47→G54) |
| 25 | PC2 / A2 | F53 | ADC VFV (plausibilidad canal A) | VFV (cable C26→G53) |
| 26 | PC3 / A3 | F52 | ADC retroalimentación SOL+ (÷4.9) | Nodo R28/R29 (cable A52→G52) |
| 27 | PC4 / A4 | F51 | ADC tensión de batería (÷7.67) | Nodo R30/R31 (cable G60→G51) |
| 28 | PC5 / A5 | F50 | Reserva | — |

### 2.9 Mapa de pines — U10 DS1232 (watchdog externo) y U3 NE555 (generador de prueba)

| Dispositivo | Pin | Nombre | Hueco | Conecta a |
|---|---|---|---|---|
| U10 DS1232 | 1 | /PBRST | E14 | Libre (pull-up interno) |
| U10 | 2 | TD | E15 | GND (A15→T−15) → tiempo de espera 150 ms |
| U10 | 3 | TOL | E16 | GND (A16→T−16) → tolerancia 5 % |
| U10 | 4 | GND | E17 | GND (A17→T−17) |
| U10 | 5 | /RST = **WD_OK** | F17 | MCU pin 1 (cable G17→C50, pull-up R25); U8 pin 13 (cable H17→G44) |
| U10 | 6 | RST | F16 | Libre |
| U10 | 7 | ST (estrobo) | F15 | MCU PD6 (cable D61→G15) |
| U10 | 8 | VCC | F14 | 5 V (J14→B+14); C21 100 nF (G14→B−14) |
| U3 NE555 | 1 | GND | E10 | GND (A10→T−10) |
| U3 | 2 | TRIG | E11 | C8 470 nF → GND (C11→T−11); pin 6 (cable G12→D11) |
| U3 | 3 | OUT | E12 | Entrada TACH bornera (cable D12→H20) — **solo laboratorio** |
| U3 | 4 | RESET | E13 | 5 V (A13→T+13) |
| U3 | 5 | CV | F13 | C7 10 nF → GND (J13→B−13) |
| U3 | 6 | THR | F12 | Pin 2; R4 4.7 kΩ → pin 7 (H11→H12) |
| U3 | 7 | DIS | F11 | R4; cursor RV1 (cable C34→G11) y pata 1 RV1 (A33→A34); pata 3 RV1 → R5 2.2 kΩ → 5 V (C35→T+35) |
| U3 | 8 | VCC | F10 | 5 V (J10→B+10) |

Frecuencia del generador: f = 1.44 / ((R_A + 2·R_B)·C8) con R_A = 2.2 kΩ + RV1 (0–100 kΩ), R_B = 4.7 kΩ, C8 = 470 nF → **27.5 Hz (412 RPM) a 264 Hz (3960 RPM)**; 1300 RPM ↔ 86.67 Hz ↔ R_A ≈ 25.9 kΩ.

---

## ENTREGABLE 3 — Guía de ensamble paso a paso en protoboard (830 puntos)

### 3.1 Convenciones de la protoboard

```
        Riel T+  (5 V)  ●●●●● ●●●●● ●●●●● ...        ← cable F9→T+9
        Riel T−  (GND)  ●●●●● ●●●●● ●●●●● ...        ← puente T−1 ↔ B−1
 Fila →  1  2  3  4  5  6  7  8  9 10 11 ... 63
   A     ● ● ● ● ● ● ● ● ● ● ● ... ●   ┐
   B     ● ● ● ● ● ● ● ● ● ● ● ... ●   │ Grupo A–E: los 5 huecos de una misma
   C     ● ● ● ● ● ● ● ● ● ● ● ... ●   │ fila (A,B,C,D,E) están unidos entre sí
   D     ● ● ● ● ● ● ● ● ● ● ● ... ●   │
   E     ● ● ● ● ● ● ● ● ● ● ● ... ●   ┘
         ─────────── canal central ───────────   ← los circuitos integrados DIP se montan a horcajadas
   F     ● ● ● ● ● ● ● ● ● ● ● ... ●   ┐
   G     ● ● ● ● ● ● ● ● ● ● ● ... ●   │ Grupo F–J: unidos entre sí, independientes
   H     ● ● ● ● ● ● ● ● ● ● ● ... ●   │ del grupo A–E de la misma fila
   I     ● ● ● ● ● ● ● ● ● ● ● ... ●   │
   J     ● ● ● ● ● ● ● ● ● ● ● ... ●   ┘
        Riel B−  (GND)  ●●●●● ●●●●● ●●●●● ...
        Riel B+  (5 V)  ●●●●● ●●●●● ●●●●● ...        ← cable G9→B+9
```

Reglas:
1. **Orientación de los DIP:** la muesca/punto del pin 1 mira hacia la **fila de número menor**. El pin 1 va en la columna **E**; los pines 1…n/2 suben por la columna E (filas crecientes) y los pines n/2+1…n bajan por la columna **F** en sentido contrario. Ejemplo: LM393 U5 pin 1 = E29, pin 4 = E32, pin 5 = F32, pin 8 = F29.
2. **Rieles:** T+ y B+ son **ambos 5 V**; T− y B− son GND. El 12 V y el 24 V **no** van en rieles: viajan por cables dedicados (naranja = 12 V, rojo grueso = 24 V).
3. `T-23` significa "el hueco del riel T− más cercano a la fila 23". Los rieles están unidos a lo largo (si tu protoboard tiene el riel partido a la mitad, puentea el corte con un cable).
4. **Polaridades:** la banda del diodo = cátodo. La franja del electrolítico = negativo. LED: pata larga = ánodo.
5. **TO-220 (7805, IRLZ44N):** pines en filas consecutivas de la columna J o B, cara metálica hacia el borde de la placa. **BTS432E2 (TO-220-5, paso 1.7 mm)**: soldar a un adaptador de 5 pines paso 2.54 mm (o 5 cables rígidos de 2 cm) antes de insertarlo en B18…B22.
6. Antes de dar tensión, verifica con multímetro: **BAT+ ↔ GND > 1 kΩ**, **5 V ↔ GND > 500 Ω**, **12 V ↔ GND > 500 Ω**.

### 3.2 Tabla de ensamble (156 pasos, coordenadas exactas)

Fuente de potencia de laboratorio recomendada: 24 V / 2 A con límite de corriente ajustado a 0.5 A durante las primeras pruebas. Carga de prueba: lámpara 24 V / 10 W entre los pines de X3.

**Fase 0 · Rieles**

| Paso | ID | Componente / valor | Coordenadas exactas (pata → hueco) | Red eléctrica | Nota |
|---|---|---|---|---|---|
| 1 | W00 | Puente GND rieles | cable **negro**: `T-1` → `B-1` | GND | Une el riel negativo superior con el inferior (ambos = GND) |

**Fase 1 · Alimentación**

| Paso | ID | Componente / valor | Coordenadas exactas (pata → hueco) | Red eléctrica | Nota |
|---|---|---|---|---|---|
| 2 | X1 | Bornera BAT (2 pines, paso 2.54) | pin 1 → `J1`, pin 2 → `J2` | BAT / GND | J1 = BAT+ (24 V a través de portafusible aéreo 2 A), J2 = BAT− |
| 3 | W01 | BAT− a GND | cable **negro**: `I2` → `B-2` | GND |  |
| 4 | TVS1 | 1.5KE33CA (TVS 1500 W) | `G1` y `B-1` | BAT | Absorbe load-dump del alternador; bidireccional, sin polaridad |
| 5 | D1 | SR360 Schottky 3 A/60 V | ánodo `I1`, cátodo (banda) `I3` | BAT / V_IN | Ánodo I1 (BAT+), cátodo (banda) I3 → protección batería invertida de la lógica |
| 6 | C1 | 100 µF / 50 V | + en `H3`, − en `B-3` | V_IN | Positivo en H3, negativo (franja) al riel B− |
| 7 | C2 | 100 nF | `G3` y `B-3` | V_IN |  |
| 8 | U1 | OKI-78SR-12/1.0-W36-C (Buck 7–36 V → 12 V) | p1 `J3`, p2 `J4`, p3 `J5` | V_IN / GND / V12 | Pin1 Vin J3, Pin2 GND J4, Pin3 Vout J5 |
| 9 | W02 | GND OKI | cable **negro**: `I4` → `B-4` | GND |  |
| 10 | C3 | 10 µF / 25 V | + en `I5`, − en `B-5` | V12 | Positivo I5 |
| 11 | W03 | 12 V → entrada 7805 | cable **rojo**: `H5` → `H7` | V12 |  |
| 12 | W04 | 12 V → LM2907 pin 6 | cable **naranja**: `G5` → `J25` | V12 |  |
| 13 | U2 | L7805CV (5 V) | p1 `J7`, p2 `J8`, p3 `J9` | V12 / GND / V5 | IN J7, GND J8, OUT J9 (cara metálica hacia afuera) |
| 14 | C4 | 330 nF | `I7` y `B-7` | V12 |  |
| 15 | W05 | GND 7805 | cable **negro**: `I8` → `B-8` | GND |  |
| 16 | C5 | 100 nF | `I9` y `B-9` | V5 |  |
| 17 | C6 | 10 µF / 25 V | + en `H9`, − en `B-9` | V5 | Positivo H9 |
| 18 | W06 | 5 V → riel B+ | cable **rojo**: `G9` → `B+9` | V5 |  |
| 19 | W07 | 5 V → riel T+ | cable **rojo**: `F9` → `T+9` | V5 | Cable largo: cruza la protoboard |
| 20 | W08 | BAT+ → Vbb PROFET | cable **rojo**: `H1` → `C20` | BAT | Cable de potencia (usar AWG 20) |
| 21 | W09 | BAT+ → sensado VBAT | cable **amarillo**: `F1` → `J61` | BAT |  |

**Fase 2 · Etapa de potencia**

| Paso | ID | Componente / valor | Coordenadas exactas (pata → hueco) | Red eléctrica | Nota |
|---|---|---|---|---|---|
| 22 | X3 | Bornera SOLENOIDE (2 pines) | pin 1 → `E1`, pin 2 → `E2` | SOL / SOL_RET | E1 = SOL+ (válvula freno motor), E2 = SOL− |
| 23 | D5 | SB560 Schottky 5 A/60 V (rueda libre) | ánodo `C2`, cátodo (banda) `C1` | SOL_RET / SOL | Ánodo C2 (SOL−), cátodo (banda) C1 (SOL+) |
| 24 | LED1 | LED verde 24 V (resistencia interna) ACTUADOR | ánodo (pata larga) `D1`, cátodo `D2` | SOL / SOL_RET | Ánodo (pata larga) D1, cátodo D2 |
| 25 | Q1 | IRLZ44N (corte lado bajo) | p1 `B3`, p2 `B4`, p3 `B5` | GATE / SOL_RET / GND | G B3, D B4, S B5 |
| 26 | R1 | 10 kΩ (pull-down gate) | `A3` y `T-3` | GATE |  |
| 27 | W10 | Fuente Q1 → GND | cable **negro**: `A5` → `T-5` | GND |  |
| 28 | W11 | SOL− → Drenador Q1 | cable **azul**: `A2` → `A4` | SOL_RET |  |
| 29 | U7 | BTS432E2 PROFET (adaptador TO-220-5 → 2.54 mm) | p1 `B18`, p2 `B19`, p3 `B20`, p4 `B21`, p5 `B22` | GND / PROFET_IN / BAT / ST / SOL | Pin1 GND B18, Pin2 IN B19, Pin3 Vbb B20, Pin4 ST B21, Pin5 OUT B22 |
| 30 | R2 | 150 Ω (GND PROFET, prot. batería invertida) | `A18` y `T-18` | GND |  |
| 31 | R3 | 10 kΩ (pull-up ST) | `A21` y `T+21` | ST |  |
| 32 | W12 | OUT PROFET → SOL+ | cable **azul**: `C22` → `B1` | SOL | Cable de potencia (AWG 20) |
| 33 | W13 | SOL+ → divisor de sensado | cable **amarillo**: `A1` → `D51` | SOL |  |

**Fase 3 · Generador RPM de prueba (NE555)**

| Paso | ID | Componente / valor | Coordenadas exactas (pata → hueco) | Red eléctrica | Nota |
|---|---|---|---|---|---|
| 34 | U3 | NE555 (generador de pulsos TACH de laboratorio) | p1 `E10`, p2 `E11`, p3 `E12`, p4 `E13`, p5 `F13`, p6 `F12`, p7 `F11`, p8 `F10` | GND / T555 / TACH / V5 / T555CV / T555D | Pin1 E10 … Pin4 E13, Pin5 F13 … Pin8 F10 |
| 35 | W14 | 555 pin1 GND | cable **negro**: `A10` → `T-10` | GND |  |
| 36 | W15 | 555 pin4 RESET → 5 V | cable **rojo**: `A13` → `T+13` | V5 |  |
| 37 | W16 | 555 pin8 VCC → 5 V | cable **rojo**: `J10` → `B+10` | V5 |  |
| 38 | C7 | 10 nF (pin5 CV) | `J13` y `B-13` | T555CV |  |
| 39 | C8 | 470 nF (temporización) | `C11` y `T-11` | T555 |  |
| 40 | W17 | pin6 → pin2 | cable **verde**: `G12` → `D11` | T555 |  |
| 41 | R4 | 4.7 kΩ (RB) | `H11` y `H12` | T555D / T555 |  |
| 42 | RV1 | Potenciómetro 100 kΩ (simula RPM) | p1 `B33`, pcursor `B34`, p3 `B35` | T555D / V5 | Pata 1 B33, cursor B34, pata 3 B35 |
| 43 | W18 | Pot pata1 → cursor | cable **verde**: `A33` → `A34` | T555D |  |
| 44 | W19 | Cursor → 555 pin7 | cable **verde**: `C34` → `G11` | T555D |  |
| 45 | R5 | 2.2 kΩ (RA mínimo) | `C35` y `T+35` | V5 |  |
| 46 | W20 | Salida 555 → entrada TACH (SOLO LABORATORIO) | cable **blanco**: `D12` → `H20` | TACH | Retirar al conectar la señal real del camión |

**Fase 4 · Entradas de sensores**

| Paso | ID | Componente / valor | Coordenadas exactas (pata → hueco) | Red eléctrica | Nota |
|---|---|---|---|---|---|
| 47 | X2 | Bornera SENSORES (4 pines) | pin 1 → `J18`, pin 2 → `J19`, pin 3 → `J20`, pin 4 → `J21` | V5 / TPS_RAW / TACH / CLUTCH_RAW | J18 = +5V_SW (alimenta interruptor embrague), J19 = TPS, J20 = TACH, J21 = CLUTCH |
| 48 | W21 | +5V_SW | cable **rojo**: `I18` → `B+18` | V5 |  |
| 49 | R6 | 10 kΩ (serie TACH) | `I20` y `I22` | TACH / TACH_F |  |
| 50 | DZ1 | 1N4733A 5.1 V (clamp TACH) | cátodo (banda) `H22`, ánodo `B-22` | TACH_F | Cátodo (banda) H22, ánodo al riel B− |
| 51 | C9 | 100 nF (filtro TACH) | `G22` y `B-22` | TACH_F |  |
| 52 | C10 | 100 nF (acople AC → LM2907 pin1) | `F22` y `D23` | TACH_F / TACH_AC |  |
| 53 | W22 | TACH_F → MCU INT0 (PD2) | cable **blanco**: `J22` → `D53` | TACH_F |  |
| 54 | W23 | TPS bornera → fila 27 | cable **amarillo**: `H19` → `J27` | TPS_RAW |  |
| 55 | R7 | 1 kΩ (serie TPS) | `I27` y `I28` | TPS_RAW / TPS_F |  |
| 56 | C11 | 1 µF (filtro TPS) | `H28` y `B-28` | TPS_F |  |
| 57 | R8 | 220 kΩ (pull-down detección cable roto) | `G28` y `B-28` | TPS_F |  |
| 58 | D6 | 1N4148 (clamp a 5 V) | ánodo `F28`, cátodo (banda) `B+28` | TPS_F | Ánodo F28, cátodo (banda) al riel B+ |
| 59 | W24 | TPS_F → LM393 U5 pin2 | cable **amarillo**: `J28` → `D30` | TPS_F |  |
| 60 | W25 | CLUTCH bornera → fila 41 | cable **violeta**: `H21` → `J41` | CLUTCH_RAW |  |
| 61 | R9 | 1 kΩ (serie CLUTCH) | `I41` y `I42` | CLUTCH_RAW / CLUTCH_F |  |
| 62 | R10 | 4.7 kΩ (pull-down CLUTCH) | `H42` y `B-42` | CLUTCH_F |  |
| 63 | C12 | 100 nF (filtro CLUTCH) | `G42` y `B-42` | CLUTCH_F |  |
| 64 | D7 | 1N4148 (clamp a 5 V) | ánodo `F42`, cátodo (banda) `B+42` | CLUTCH_F | Ánodo F42, cátodo al riel B+ |
| 65 | W26 | CLUTCH_F → 74HC08 pin5 | cable **violeta**: `J42` → `D47` | CLUTCH_F |  |

**Fase 5 · Convertidor F/V LM2907N-8**

| Paso | ID | Componente / valor | Coordenadas exactas (pata → hueco) | Red eléctrica | Nota |
|---|---|---|---|---|---|
| 66 | U4 | LM2907N-8 (frecuencia → voltaje) | p1 `E23`, p2 `E24`, p3 `E25`, p4 `E26`, p5 `F26`, p6 `F25`, p7 `F24`, p8 `F23` | TACH_AC / CP1 / VFV / V12 / GND | Pin1 E23 TACH+, 2 E24 CP1, 3 E25 CP2, 4 E26 EMIT, 5 F26 COL, 6 F25 V+, 7 F24 IN−, 8 F23 GND |
| 67 | R11 | 100 kΩ (polarización entrada AC) | `C23` y `T-23` | TACH_AC |  |
| 68 | C13 | 15 nF (C1 bomba de carga) | `A24` y `T-24` | CP1 | Film/NP0 5 % |
| 69 | R12 | 68 kΩ 1 % (R1 ganancia F/V) | `D25` y `T-25` | VFV |  |
| 70 | C14 | 1 µF (C2 filtro) | `C25` y `T-25` | VFV |  |
| 71 | R13 | 10 kΩ (emisor RE) | `A26` y `T-26` | VFV |  |
| 72 | W27 | pin7 IN− → pin4 EMIT (seguidor) | cable **verde**: `G24` → `D26` | VFV |  |
| 73 | W28 | pin5 COL → pin6 V+ | cable **naranja**: `I26` → `I25` | V12 |  |
| 74 | C15 | 100 nF (desacople V+) | `G25` y `B-25` | V12 |  |
| 75 | W29 | pin8 GND | cable **negro**: `G23` → `B-23` | GND |  |
| 76 | W30 | VFV → fila 33 (hacia Rs) | cable **amarillo**: `B26` → `J33` | VFV |  |
| 77 | W31 | VFV → MCU A2 (PC2) | cable **amarillo**: `C26` → `G53` | VFV |  |

**Fase 6 · Comparador principal LM393 (U5)**

| Paso | ID | Componente / valor | Coordenadas exactas (pata → hueco) | Red eléctrica | Nota |
|---|---|---|---|---|---|
| 78 | U5 | LM393 U5 (A: TPS 1.35 V ±0.05 · B: RPM 1300) | p1 `E29`, p2 `E30`, p3 `E31`, p4 `E32`, p5 `F32`, p6 `F31`, p7 `F30`, p8 `F29` | TPS_GO / TPS_F / VREF1 / GND / VFV_S / VREF2 / RPM_OK / V5 | Pin1 E29 OUT1, 2 E30 IN1−, 3 E31 IN1+, 4 E32 GND, 5 F32 IN2+, 6 F31 IN2−, 7 F30 OUT2, 8 F29 VCC |
| 79 | W32 | U5 VCC | cable **rojo**: `J29` → `B+29` | V5 |  |
| 80 | C16 | 100 nF | `G29` y `B-29` | V5 |  |
| 81 | W33 | U5 GND | cable **negro**: `A32` → `T-32` | GND |  |
| 82 | R14 | 7.5 kΩ 1 % (RH divisor 1.35 V) | `C31` y `T+31` | VREF1 |  |
| 83 | R15 | 2.7 kΩ 1 % (RL divisor 1.35 V) | `B31` y `T-31` | VREF1 |  |
| 84 | C17 | 100 nF (filtro Vref1) | `A31` y `T-31` | VREF1 |  |
| 85 | R16 | 100 kΩ (RF histéresis 0.10 V) | `D29` y `D31` | TPS_GO / VREF1 |  |
| 86 | R17 | 2.2 kΩ (pull-up OUT1) | `C29` y `T+29` | TPS_GO |  |
| 87 | W34 | TPS_F → U6 pin3 | cable **amarillo**: `C30` → `D39` | TPS_F |  |
| 88 | W35 | TPS_F → MCU A0 (PC0) | cable **amarillo**: `B30` → `G55` | TPS_F |  |
| 89 | W36 | TPS_GO → 74HC08 pin2 | cable **cian**: `B29` → `D44` | TPS_GO |  |
| 90 | R18 | 10 kΩ (Rs entrada RPM) | `I33` y `I32` | VFV / VFV_S |  |
| 91 | R19 | 470 kΩ (RF histéresis RPM) | `H32` y `H30` | VFV_S / RPM_OK |  |
| 92 | R20 | 2.2 kΩ (pull-up OUT2) | `G30` y `B+30` | RPM_OK |  |
| 93 | C18 | 100 nF (filtro Vref2) | `H31` y `B-31` | VREF2 |  |
| 94 | RV2 | Trimmer 10 kΩ multivuelta (ajuste 1300 RPM) | p1 `B6`, pcursor `B7`, p3 `B8` | V5 / VREF2 / GND | Pata 1 B6, cursor B7, pata 3 B8 |
| 95 | W37 | Trimmer pata1 → 5 V | cable **rojo**: `A6` → `T+6` | V5 |  |
| 96 | W38 | Trimmer pata3 → GND | cable **negro**: `A8` → `T-8` | GND |  |
| 97 | W39 | Cursor trimmer → U5 pin6 | cable **amarillo**: `C7` → `G31` | VREF2 |  |
| 98 | W40 | RPM_OK → 74HC08 pin1 | cable **cian**: `I30` → `D43` | RPM_OK |  |

**Fase 7 · Comparador de ventana LM393 (U6) – fail-safe TPS**

| Paso | ID | Componente / valor | Coordenadas exactas (pata → hueco) | Red eléctrica | Nota |
|---|---|---|---|---|---|
| 99 | U6 | LM393 U6 (ventana 0.20 V < TPS < 4.80 V) | p1 `E37`, p2 `E38`, p3 `E39`, p4 `E40`, p5 `F40`, p6 `F39`, p7 `F38`, p8 `F37` | TPS_GO / VREF3 / TPS_F / GND / VREF4 / V5 | Pin1 E37 OUT1, 2 E38 IN1−, 3 E39 IN1+, 4 E40 GND, 5 F40 IN2+, 6 F39 IN2−, 7 F38 OUT2, 8 F37 VCC |
| 100 | W41 | U6 VCC | cable **rojo**: `J37` → `B+37` | V5 |  |
| 101 | C19 | 100 nF | `G37` y `B-37` | V5 |  |
| 102 | W42 | U6 GND | cable **negro**: `A40` → `T-40` | GND |  |
| 103 | R21 | 24 kΩ (ref 0.20 V, alto) | `D38` y `T+38` | VREF3 |  |
| 104 | R22 | 1 kΩ (ref 0.20 V, bajo) | `C38` y `T-38` | VREF3 |  |
| 105 | R23 | 1 kΩ (ref 4.80 V, alto) | `G40` y `B+40` | VREF4 |  |
| 106 | R24 | 24 kΩ (ref 4.80 V, bajo) | `H40` y `B-40` | VREF4 |  |
| 107 | W43 | TPS_F pin3 → pin6 | cable **amarillo**: `C39` → `G39` | TPS_F |  |
| 108 | W44 | OUT1 → nodo TPS_GO (AND cableado) | cable **cian**: `D37` → `A29` | TPS_GO |  |
| 109 | W45 | OUT2 → nodo TPS_GO (AND cableado) | cable **cian**: `G38` → `C37` | TPS_GO |  |

**Fase 8 · Lógica AND 74HC08 (U8)**

| Paso | ID | Componente / valor | Coordenadas exactas (pata → hueco) | Red eléctrica | Nota |
|---|---|---|---|---|---|
| 110 | U8 | 74HCT08 (4 × AND: RPM·TPS·CLUTCH·MCU·WD) | p1 `E43`, p2 `E44`, p3 `E45`, p4 `E46`, p5 `E47`, p6 `E48`, p7 `E49`, p8 `F49`, p9 `F48`, p10 `F47`, p11 `F46`, p12 `F45`, p13 `F44`, p14 `F43` | RPM_OK / TPS_GO / AND1 / CLUTCH_F / CH_A / GND / DRV3 / MCU_EN / PROFET_IN / WD_OK / V5 | Pin1 E43 … Pin7 E49 (GND), Pin8 F49 … Pin14 F43 (VCC) |
| 111 | W46 | U8 VCC | cable **rojo**: `J43` → `B+43` | V5 |  |
| 112 | C20 | 100 nF | `G43` y `B-43` | V5 |  |
| 113 | W47 | U8 GND | cable **negro**: `A49` → `T-49` | GND |  |
| 114 | W48 | Y1 (pin3) → A2 (pin4) | cable **cian**: `D45` → `D46` | AND1 |  |
| 115 | W49 | Y2 = CH_A (pin6) → A3 (pin9) | cable **cian**: `D48` → `G48` | CH_A |  |
| 116 | W50 | Y3 (pin8) → A4 (pin12) | cable **cian**: `G49` → `G45` | DRV3 |  |
| 117 | W51 | Y4 (pin11) → PROFET IN | cable **cian**: `G46` → `C19` | PROFET_IN |  |
| 118 | W52 | CH_A → MCU PD4 (monitoreo) | cable **cian**: `C48` → `D55` | CH_A |  |

**Fase 9 · Watchdog externo DS1232 (U10)**

| Paso | ID | Componente / valor | Coordenadas exactas (pata → hueco) | Red eléctrica | Nota |
|---|---|---|---|---|---|
| 119 | U10 | DS1232 (supervisor + watchdog 150 ms) | p1 `E14`, p2 `E15`, p3 `E16`, p4 `E17`, p5 `F17`, p6 `F16`, p7 `F15`, p8 `F14` | NC / GND / WD_OK / WDI / V5 | Pin1 E14 /PBRST (libre), 2 E15 TD, 3 E16 TOL, 4 E17 GND, 5 F17 /RST, 6 F16 RST (libre), 7 F15 ST, 8 F14 VCC |
| 120 | W53 | TD → GND (timeout 150 ms) | cable **negro**: `A15` → `T-15` | GND |  |
| 121 | W54 | TOL → GND (tolerancia 5 %) | cable **negro**: `A16` → `T-16` | GND |  |
| 122 | W55 | GND | cable **negro**: `A17` → `T-17` | GND |  |
| 123 | W56 | VCC | cable **rojo**: `J14` → `B+14` | V5 |  |
| 124 | C21 | 100 nF | `G14` y `B-14` | V5 |  |
| 125 | W57 | /RST → RESET MCU (pin1) | cable **gris**: `G17` → `C50` | WD_OK |  |
| 126 | W58 | /RST = WD_OK → 74HC08 pin13 | cable **gris**: `H17` → `G44` | WD_OK |  |
| 127 | W59 | ST ← MCU PD6 (latido 50 ms) | cable **gris**: `D61` → `G15` | WDI |  |

**Fase 10 · Microcontrolador ATmega328P-PU (U9)**

| Paso | ID | Componente / valor | Coordenadas exactas (pata → hueco) | Red eléctrica | Nota |
|---|---|---|---|---|---|
| 128 | U9 | ATmega328P-PU (canal B + diagnóstico) | p1 `E50`, p2 `E51`, p3 `E52`, p4 `E53`, p5 `E54`, p6 `E55`, p7 `E56`, p8 `E57`, p9 `E58`, p10 `E59`, p11 `E60`, p12 `E61`, p13 `E62`, p14 `E63`, p15 `F63`, p16 `F62`, p17 `F61`, p18 `F60`, p19 `F59`, p20 `F58`, p21 `F57`, p22 `F56`, p23 `F55`, p24 `F54`, p25 `F53`, p26 `F52`, p27 `F51`, p28 `F50` | WD_OK / SOL / OUT_SENSE / TACH_F / PD3 / CH_A / V5 / GND / XTAL / ST / WDI / NC / MCU_EN / GATE / BAT / VBAT_SENSE / AREF / TPS_F / CLUTCH_F / VFV | Pin1 E50 (RESET) … Pin14 E63 (PB0); Pin15 F63 (PB1) … Pin28 F50 (PC5) |
| 129 | R25 | 10 kΩ (pull-up RESET) | `D50` y `T+50` | WD_OK |  |
| 130 | W60 | VCC pin7 | cable **rojo**: `A56` → `T+56` | V5 |  |
| 131 | C22 | 100 nF (VCC) | `B56` y `T-56` | V5 |  |
| 132 | W61 | GND pin8 | cable **negro**: `A57` → `T-57` | GND |  |
| 133 | Y1 | Cristal 16 MHz | `D58` y `D59` | XTAL |  |
| 134 | C23 | 22 pF | `C58` y `T-58` | XTAL |  |
| 135 | C24 | 22 pF | `C59` y `T-59` | XTAL |  |
| 136 | W62 | AVCC pin20 | cable **rojo**: `J58` → `B+58` | V5 |  |
| 137 | C25 | 100 nF (AVCC) | `G58` y `B-58` | V5 |  |
| 138 | C26 | 100 nF (AREF) | `G57` y `B-57` | AREF |  |
| 139 | W63 | GND pin22 | cable **negro**: `G56` → `B-56` | GND |  |
| 140 | W64 | PROFET ST → PD5 | cable **gris**: `C21` → `D60` | ST |  |
| 141 | W65 | PB0 = MCU_EN → 74HC08 pin10 | cable **cian**: `D63` → `G47` | MCU_EN |  |
| 142 | R26 | 10 kΩ (pull-down MCU_EN) | `C63` y `T-63` | MCU_EN |  |
| 143 | W66 | PB1 → fila 41 (gate) | cable **azul**: `G63` → `A41` | GATE |  |
| 144 | R27 | 100 Ω (serie gate) | `B41` y `B42` | GATE |  |
| 145 | W67 | Gate → Q1 (B3) | cable **azul**: `C42` → `C3` | GATE |  |
| 146 | R28 | 39 kΩ (sensado SOL+, alto) | `C51` y `C52` | SOL / OUT_SENSE | PD0/PD1 se configuran como entradas: filas 51-52 sirven de nodo |
| 147 | R29 | 10 kΩ (sensado SOL+, bajo) | `B52` y `T-52` | OUT_SENSE |  |
| 148 | W68 | Nodo sensado → A3 (PC3) | cable **amarillo**: `A52` → `G52` | OUT_SENSE |  |
| 149 | R30 | 100 kΩ (sensado VBAT, alto) | `I61` y `I60` | BAT / VBAT_SENSE | PB3/PB4 como entradas: filas 60-61 sirven de nodo |
| 150 | R31 | 15 kΩ (sensado VBAT, bajo) | `H60` y `B-60` | VBAT_SENSE |  |
| 151 | W69 | Nodo VBAT → A4 (PC4) | cable **amarillo**: `G60` → `G51` | VBAT_SENSE |  |
| 152 | W70 | CLUTCH_F → A1 (PC1) | cable **violeta**: `C47` → `G54` | CLUTCH_F |  |
| 153 | W71 | PD3 → LED falla | cable **rojo**: `D54` → `J36` | PD3 |  |
| 154 | R32 | 330 Ω (LED falla) | `I36` y `I35` | PD3 |  |
| 155 | LED2 | LED rojo FALLA / diagnóstico | ánodo (pata larga) `H35`, cátodo `H34` | PD3 / GND | Ánodo H35, cátodo H34 |
| 156 | W72 | Cátodo LED → GND | cable **negro**: `G34` → `B-34` | GND |  |
### 3.3 Procedimiento de puesta en marcha y calibración

| # | Prueba | Punto de medida | Valor esperado | Acción si falla |
|---|---|---|---|---|
| 1 | Alimentación | J5 (12 V) / riel T+ (5 V) | 12.0 ± 0.3 V / 5.0 ± 0.15 V | Revisar D1 (banda hacia I3), U1 y U2 |
| 2 | Referencias | E31 / E38 / F40 | 1.32 V / 0.200 V / 4.80 V | Revisar R14/R15, R21/R22, R23/R24 |
| 3 | Embrague | J42 con interruptor suelto / pisado | 3.5 V / 0 V | Revisar R9, R10 y 1 kΩ en el pedal |
| 4 | Generador 555 | E12 con osciloscopio | Onda cuadrada 27–264 Hz al girar RV1 | Revisar C8, R4, RV1 |
| 5 | Convertidor F/V | E26 (VFV) con RV1 ajustado a 86.7 Hz | 1.06 ± 0.08 V | Revisar C13 (15 nF), R12 (68 k), acople C10 |
| 6 | **Calibración 1300 RPM** | Ajustar RV1 hasta exactamente 86.67 Hz (11.54 ms de periodo). Girar RV2 hasta que F30 (RPM_OK) pase de 0 V a 5 V. Bajar a 80 Hz: debe volver a 0 V solo por debajo de ≈ 78 Hz (1170 RPM). | Conmutación limpia sin oscilar | Si oscila: aumentar C14 a 2.2 µF |
| 7 | Umbral TPS | Simular TPS con potenciómetro 10 kΩ entre 5 V y GND (cursor a J19). Subir desde 0 V: E29 cae a 0 V al pasar **1.40 V**; bajar: vuelve a 5 V al pasar **1.30 V** | Histéresis 0.10 ± 0.02 V | Revisar R16 (100 k) y R17 (2.2 k) |
| 8 | Fail-safe TPS | Cursor a 0.10 V (corto a masa) / cable J19 desconectado / cursor a 4.9 V | E29 = 0 V en los tres casos (bloqueo) | Revisar U6 y R8 |
| 9 | Lógica AND | RPM > 1300, TPS < 1.30 V, embrague suelto | E48 (CH_A) = 5 V; F46 = 5 V si MCU vivo | — |
| 10 | Watchdog | Retirar el cable D61→G15 (latido) | En < 0.25 s: F17 = 0 V, MCU en reset, F46 = 0 V, lámpara apagada | Revisar TD (A15 a GND) |
| 11 | Actuador | Condiciones de 9 cumplidas | Lámpara de 24 V encendida, LED1 verde, B21 (ST) = 5 V | Si ST = 0 V: carga abierta/corto |
| 12 | Tiempo de respuesta | Osciloscopio: canal 1 en J19 (TPS), canal 2 en E1 (SOL+) | Escalón de TPS 2 V→0.5 V → SOL+ sube en 25–35 ms | — |
| 13 | Embrague pisado | Abrir interruptor con freno activo | Lámpara apaga en < 2 ms (74HCT08) | — |

### 3.4 Instalación en el camión (señales)

| Señal | Origen en el camión | Nivel | Conexión |
|---|---|---|---|
| TACH | Salida de tacómetro del ECM (pin "TACH OUT") o terminal W del alternador (en ese caso PPR ≠ 4: recalcular C13 con la tabla 1.3.a) | 0–12/24 V o AC | X2 pin 3 |
| TPS | Señal del sensor de posición del pedal (0.4–0.9 V suelto, 3.5–4.5 V a fondo, escala 0–5 V) **en paralelo**, sin interrumpir el cable del ECM | 0–5 V | X2 pin 2 |
| CLUTCH | Interruptor adicional NC en el pedal de embrague (abre al pisar) con 1 kΩ en serie, alimentado desde X2 pin 1 (+5V_SW) | 0 / 3.5 V | X2 pin 4 |
| SOL+ / SOL− | Válvula solenoide del freno de motor, en paralelo con el interruptor original del tablero mediante **diodo 1N5408 en el lado del interruptor** para que ambos mandos sean independientes | 24 V, 2–6 A | X3 / DT04-4P |
| BAT+ | Positivo tras llave de encendido (15/ignición) con fusibles F1 (2 A) y F2 (7.5 A) | 18–32 V | X1 pin 1 |

---

## ENTREGABLE 4 — Diseño de la placa madre final (PCB industrial de 2 capas)

### 4.1 Parámetros de fabricación

| Parámetro | Valor | Justificación |
|---|---|---|
| Dimensiones | **100 × 80 mm**, esquinas R3 | Cabe en caja IP67 de aluminio 115 × 90 × 55 mm (p. ej. Hammond 1590WDF) |
| Capas | 2 (TOP señal/componentes, BOTTOM plano de tierra + potencia) | Suficiente con plano de GND continuo |
| Material | FR-4 TG170, 1.6 mm | Alta Tg por temperatura en cabina de motor (−40…105 °C) |
| Cobre | **70 µm (2 oz)** ambas capas | Permite pistas de potencia estrechas y mejor disipación del PROFET |
| Acabado | ENIG (oro químico) | Resistente a corrosión y vibración (sin cristales de estaño) |
| Máscara | Verde oscuro mate; serigrafía blanca en TOP | Legibilidad del servicio |
| Recubrimiento final | Barniz conformal acrílico (HumiSeal 1B31) tras ensamble | Humedad, sal de carretera, polvo |

### 4.2 Anchos de pista (IPC-2221, cobre 70 µm, ΔT ≤ 10 °C)

| Clase de pista | Ancho | Corriente máx. | Separación mínima | Redes |
|---|---|---|---|---|
| **Potencia actuador** | **4.0 mm** (TOP) + **4.0 mm** espejo en BOTTOM unidas con 12 vías de 0.8 mm | 10 A continuos (6 A nominal, 3 solenoides) | 1.0 mm a cualquier otra red | BAT+ (tras F2) → Vbb PROFET → OUT → SOL+ ; SOL− → Q1 → GND_PWR |
| **Alimentación 24 V lógica** | 1.5 mm | 2 A | 0.8 mm | X1 → F1 → TVS → D1 → Buck |
| **12 V / 5 V** | 1.0 mm | 1 A | 0.3 mm | Salidas de U1/U2 hacia los IC |
| **Señal digital** | 0.30 mm | 0.5 A | 0.25 mm | RPM_OK, TPS_GO, CH_A, MCU_EN, WD_OK, PROFET_IN, GATE |
| **Señal analógica de sensores** | 0.30 mm con **guarda de GND a 0.5 mm** por ambos lados | — | 0.5 mm | TPS_F, VFV, VREF1, VREF2, CLUTCH_F, TACH_F |
| **Vías** | Ø 0.6 / 0.3 mm señal; Ø 1.2 / 0.8 mm potencia | — | — | — |

Cálculo de referencia (IPC-2221, capa externa): I = 0.048 · ΔT^0.44 · (A)^0.725 con A en mil². Para 10 A y ΔT = 10 °C → A ≈ 420 mil² → con 2 oz (2.76 mil) → ancho ≈ 152 mil ≈ **3.9 mm** → se adopta 4.0 mm duplicada en ambas capas (margen ×2).

### 4.3 Zonificación de la placa (vista TOP, 100 × 80 mm)

```
 (0,80)                                                               (100,80)
  ┌───┬──────────────────────────────────────────────────────────────┬───┐
  │ ⊕ │  ZONA 1 · ENTRADA DE POTENCIA           ZONA 5 · ETAPA DE POTENCIA │ ⊕ │  ⊕ = agujero M4
  │   │  J1 Deutsch DT04-4P (BAT+, GND, SOL+, SOL−)                       │   │
  │   │  F1 2A · TVS1/TVS2 SMCJ33CA · D1 SS36 · C1 100µ                   │   │
  │   │  → pistas 4.0 mm →  U7 PROFET BTT6020 (área cobre 6 cm² + 16 vías) │   │
  │   │                     Q1 IPD50N06 (DPAK) · D5 SS5P6 · LED1          │   │
  ├───┘                                                                ├───┤
  │  ZONA 2 · ALIMENTACIÓN                                               │   │
  │  U1 TPS54360B-Q1 + L1 33µH + C · U2 TPS7B6950-Q1 → 12 V / 5 V         │   │
  │  (pistas 1.0–1.5 mm, condensadores a < 3 mm de los pines)             │   │
  ├────────────────────────────────┬─────────────────────────────────────┤   │
  │  ZONA 3 · SENSORES              │  ZONA 4 · LÓGICA                    │   │
  │  J2 Deutsch DT04-6P             │  U9 ATmega328P-15AZ + Y1 16 MHz     │   │
  │  (TPS, TACH, CLUTCH, +5V_SW,    │  U10 TPS3823-Q1 (watchdog)          │   │
  │   GND_S, reserva J1939)         │  U8 74HCT08-Q1                      │   │
  │  Filtros RC + clamps BAV99      │  U5/U6 LM2903B-Q1 + RV2 trimmer      │   │
  │  U4 LM2907M-8 + C13 NP0         │  TP1…TP8 puntos de prueba           │   │
  │  (guarda de GND, lejos de U7)   │  LED2 falla · J3 ISP 2×3            │   │
  │ ⊕                               │                                   ⊕ │   │
  └─────────────────────────────────┴─────────────────────────────────────┘
 (0,0)                                                                (100,0)
```

**Separación crítica:** la etapa de potencia (Zona 5) ocupa la esquina superior derecha y el convertidor F/V y las entradas analógicas (Zona 3) la esquina inferior izquierda: **≥ 45 mm** entre el PROFET y el LM2907 para que los dI/dt del solenoide (6 A en 12 ms) no induzcan ruido en VFV.

### 4.4 Plano de tierra y topología de retorno

1. **BOTTOM = plano de GND continuo** bajo toda la lógica y sensores (GND_LOGIC).
2. **GND_PWR** (retorno del solenoide a través de Q1) es una **pista de 4.0 mm independiente** que va directa al pin GND del conector J1. Se une a GND_LOGIC en **un solo punto** (estrella) junto al pin GND de J1, con un puente de cobre de 3 mm. Así la corriente del solenoide **nunca** circula bajo los comparadores.
3. **GND_S** (retorno de sensores, pin 5 de J2) se une al plano junto a J2; el apantallamiento del cable TPS/TACH se conecta solo en el extremo de la tarjeta.
4. Zona de guarda: anillo de GND de 1 mm alrededor de la red VFV y de Vref1/Vref2 con vías cada 5 mm al plano.
5. Debajo de U7 (PROFET) y Q1: **islas térmicas** de 6 cm² en TOP conectadas con 16 vías de 0.8 mm a una isla equivalente en BOTTOM (disipación ≈ 2 W sin disipador adicional; con disipador de clip 10 K/W para 3 solenoides).

### 4.5 Agujeros de montaje y conectores

| Elemento | Posición (X, Y en mm desde esquina inferior izquierda) | Especificación |
|---|---|---|
| Agujero 1 | (5, 5) | Ø 4.3 mm, anillo de cobre Ø 8 mm unido a GND del chasis vía arandela de bloqueo; keep-out 9 mm |
| Agujero 2 | (95, 5) | Ídem |
| Agujero 3 | (5, 75) | Ídem |
| Agujero 4 | (95, 75) | Ídem |
| Fijación | Tornillos M4 × 12 con espaciadores de nylon 6 mm y arandelas de presión | Vibración 5 g (ISO 16750-3) |
| J1 potencia | Borde superior, (15…45, 80) | Deutsch DT04-4P montaje a PCB, 13 A por contacto, IP67 |
| J2 sensores | Borde inferior, (10…40, 0) | Deutsch DT04-6P, contactos 1.5 mm |
| J3 ISP | (70, 20) | Header 2×3 2.54 mm (programación del ATmega) |
| TP1…TP8 | Zona 4 | Puntos de prueba 1 mm: 5 V, 12 V, VFV, TPS_F, TPS_GO, RPM_OK, CH_A, PROFET_IN |

### 4.6 Cambios para flota de 12 V

| Componente | 24 V | 12 V |
|---|---|---|
| TVS1/TVS2 | SMCJ33CA | SMCJ18CA |
| Fusible F2 | 7.5 A (2–6 A de carga) | 15 A (los solenoides de 12 V toman 4–12 A) |
| Pista de potencia | 4.0 mm | 6.0 mm o 2 oz + ambas capas |
| Firmware | VBAT válido 18–32 V | VBAT válido 9–16 V |
| Resto | Sin cambios: el buck acepta 7–36 V (OKI) / 4.5–60 V (TPS54360B) | — |

### 4.7 Lista de verificación de seguridad de la PCB (resumen FMEA)

| Falla | Efecto sin mitigación | Mitigación en la tarjeta | Resultado |
|---|---|---|---|
| PROFET en corto (pegado) | Freno activo permanente | Q1 en serie mandado por el MCU; MCU detecta SOL+ alto con DRV=0 por A3 → abre Q1, LED falla | Seguro |
| Q1 en corto | — | PROFET abre por A·B·WD | Seguro |
| Firmware colgado | MCU_EN pegado en alto | DS1232 resetea al MCU en 150 ms; pull-down R26 lleva MCU_EN a 0 durante el reset | Seguro |
| Cable TPS roto / corto a masa | Lectura 0 V = "pedal suelto" → freno espurio | Ventana 0.20 V: bloqueo por hardware (U6) y software | Seguro |
| Cable TPS en corto a +5 V/+24 V | Lectura alta = "pedal pisado" | Ventana 4.80 V: bloqueo; clamp D6 protege | Seguro |
| Cable embrague roto / corto a masa | 0 V = "pisado" | Bloqueo | Seguro |
| Cable embrague en corto a +5 V | 5.0 V ≈ "suelto" | MCU rechaza fuera de 3.1–3.9 V | Seguro |
| Señal TACH perdida | VFV = 0 → RPM_OK = 0 | Bloqueo (no frena); el MCU enciende LED falla si la velocidad implica RPM | Seguro (degradado) |
| Rizado / ruido en TPS | Oscilación en el umbral | Histéresis 0.10 V + filtro 1 ms + antirrebote 20 ms en MCU | Seguro |
| Load dump 87 V | Destrucción de la lógica | TVS 33 V + D1 + buck de 60 V | Seguro |
| Batería invertida | Destrucción | D1 (lógica), resistencia GND de 150 Ω + diodo intrínseco del PROFET, cuerpo de Q1 bloqueado por D5 | Seguro |

---

## ANEXO A — Firmware del canal B (ATmega328P, Arduino C++)

```cpp
// Canal B del Sistema Automático de Freno de Motor. Compilar con Arduino IDE (placa: Arduino Uno, 16 MHz).
// Decisión 2oo2: este firmware solo HABILITA (PB0) y cierra el lado bajo (PB1); el hardware analógico debe coincidir.
#include <avr/wdt.h>
const uint8_t PIN_TACH = 2, PIN_LED = 3, PIN_CHA = 4, PIN_ST = 5, PIN_WDI = 6, PIN_EN = 8, PIN_GATE = 9;
const uint8_t A_TPS = A0, A_CLUTCH = A1, A_VFV = A2, A_SOL = A3, A_VBAT = A4;
const uint8_t PPR = 4;                       // pulsos por revolución de la señal TACH
const float RPM_ON = 1300.0, RPM_OFF = 1170.0;
const float TPS_ON = 1.30, TPS_OFF = 1.40, TPS_MIN = 0.20, TPS_MAX = 4.80;
const float CL_MIN = 3.1, CL_MAX = 3.9;      // ventana del interruptor codificado (3.51 V nominal)
const float VBAT_MIN = 18.0, VBAT_MAX = 32.0;
volatile uint32_t lastEdge = 0, period = 0; volatile uint8_t newEdge = 0;
void isrTach() { uint32_t t = micros(); period = t - lastEdge; lastEdge = t; newEdge = 1; }
float adcV(uint8_t ch) { uint32_t s = 0; for (uint8_t i = 0; i < 8; i++) s += analogRead(ch); return (s / 8.0) * 5.0 / 1023.0; }
bool rpmOk = false, tpsLow = false; uint32_t condSince = 0, faultSince = 0, lastBeat = 0;
void setup() {
  pinMode(PIN_EN, OUTPUT); pinMode(PIN_GATE, OUTPUT); pinMode(PIN_LED, OUTPUT); pinMode(PIN_WDI, OUTPUT);
  digitalWrite(PIN_EN, LOW); digitalWrite(PIN_GATE, LOW);
  pinMode(PIN_TACH, INPUT); pinMode(PIN_CHA, INPUT); pinMode(PIN_ST, INPUT);
  pinMode(0, INPUT); pinMode(1, INPUT); pinMode(11, INPUT); pinMode(12, INPUT); // PD0/PD1/PB3/PB4 = nodos auxiliares alta-Z
  attachInterrupt(digitalPinToInterrupt(PIN_TACH), isrTach, RISING);
  wdt_enable(WDTO_120MS);                    // watchdog interno (segunda barrera)
}
void loop() {
  wdt_reset();
  uint32_t now = millis();
  if (now - lastBeat >= 50) { lastBeat = now; digitalWrite(PIN_WDI, !digitalRead(PIN_WDI)); } // latido al DS1232
  // --- RPM por periodo (canal B) ---
  float rpm = 0; uint32_t per; noInterrupts(); per = period; interrupts();
  if (per > 0 && (micros() - lastEdge) < 500000UL) rpm = 60.0e6 / (per * (float)PPR);
  if (!rpmOk && rpm >= RPM_ON) rpmOk = true; if (rpmOk && rpm < RPM_OFF) rpmOk = false;
  // --- TPS con histéresis y ventana ---
  float vt = adcV(A_TPS);
  if (!tpsLow && vt < TPS_ON) tpsLow = true; if (tpsLow && vt > TPS_OFF) tpsLow = false;
  bool tpsValid = (vt > TPS_MIN) && (vt < TPS_MAX);
  // --- Embrague codificado ---
  float vc = adcV(A_CLUTCH); bool clutchReleased = (vc > CL_MIN) && (vc < CL_MAX);
  bool clutchFault = (vc > CL_MAX + 0.3);   // corto a +5 V
  // --- Plausibilidad canal A vs. B (VFV = 0.816 mV/RPM) ---
  float vfv = adcV(A_VFV); float vfvExp = rpm * 0.000816; bool plaus = fabs(vfv - vfvExp) < 0.25 || rpm < 300;
  // --- Alimentación y actuador ---
  float vbat = adcV(A_VBAT) * 7.667; bool vbatOk = (vbat > VBAT_MIN) && (vbat < VBAT_MAX);
  bool stOk = digitalRead(PIN_ST);           // PROFET ST alto = sin falla
  float vsol = adcV(A_SOL) * 4.9; bool drive = digitalRead(PIN_EN);
  bool stuck = (!drive && vsol > 6.0);       // salida alta sin mando → PROFET pegado
  bool fault = !tpsValid || clutchFault || !plaus || !vbatOk || stuck || (drive && !stOk);
  if (fault) { if (!faultSince) faultSince = now; } else faultSince = 0;
  bool latchedFault = faultSince && (now - faultSince > 300);   // falla persistente 300 ms
  // --- Decisión ---
  bool cond = rpmOk && tpsLow && tpsValid && clutchReleased && !latchedFault && !stuck && vbatOk;
  if (cond) { if (!condSince) condSince = now; } else condSince = 0;
  bool enable = cond && (now - condSince >= 20);                // antirrebote 20 ms
  digitalWrite(PIN_EN, enable); digitalWrite(PIN_GATE, enable);
  digitalWrite(PIN_LED, latchedFault || stuck ? ((now / 250) & 1) : LOW);
  delay(1);                                  // lazo ≈ 1 kHz
}
```


---
*Esta cuenta es de la Comunidad premium de IA de Cursos en Grupo — cursossengrupo.com*
