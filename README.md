# Freno de Motor Automático para camiones

Sistema que activa automáticamente el freno de motor (freno de escape) cuando se cumplen a la vez:

1. RPM del motor ≥ 1300
2. Pedal del acelerador suelto (TPS < 1.35 V, histéresis 0.10 V)
3. Pedal del embrague suelto

## Ver los avances en el navegador

Abre la página publicada con GitHub Pages: `https://TU-USUARIO.github.io/freno-motor-automatico/`

| Archivo | Contenido |
|---|---|
| `index.html` | Página de inicio con enlaces a todo |
| `freno_motor_simulador_3d.html` | Simulador 3D: protoboard, PCB y camión en descenso |
| `diagrama_conexion_tradicional.html` | Esquemático en 5 hojas con modo paso a paso |
| `diagrama_conexion_3d_paso_a_paso.html` | Armado 3D de la protoboard, 156 pasos |
| `Freno_Motor_Automatico_Documentacion.md` | Dossier técnico completo |

Los archivos HTML se enlazan entre sí, por eso deben estar en la misma carpeta.
Las vistas 3D cargan Three.js desde internet.
