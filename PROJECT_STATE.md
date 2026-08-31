# PROJECT_STATE — Proyecto Trike

**Versión de referencia:** v8.3  
**Código principal:** `Proyecto_trike_v8_3.ino`  
**Base funcional:** V8.2.5  
**Rama:** `main`

## Fuente de verdad

El código actual de GitHub es la fuente principal. Si este archivo contradice al firmware, prevalece el código.

Antes de modificar el firmware:

1. Leer este archivo.
2. Leer el código actual de GitHub.
3. Revisar las funciones y dependencias afectadas.

No usar versiones antiguas, otros proyectos ni memoria no confirmada para completar datos técnicos.

## Microcontrolador

- Arduino Nano / ATmega328P.

## Pinout actual

| Función | Pin |
|---|---:|
| BTN UP | D3 |
| BTN DOWN | D2 |
| Relé movimiento IN | D4 |
| Relé movimiento OUT | D5 |
| K2 | D6 |
| LED R | D7 |
| K1 | D8 |
| LED 1 | D9 |
| LED 2 | D10 |
| BTN MODO | D11 |
| BTN CONF | D12 |
| LED N | D13 |
| Potenciómetro A | A0 |
| FC_S | A1 |
| FC_C | A2 |
| Potenciómetro B | A3 |
| LED aviso potenciómetro | A4 |

## Posiciones

Enumeración de software:

- `MARCHA_R`
- `MARCHA_N`
- `MARCHA_1`
- `MARCHA_2`

Orden físico longitudinal confirmado:

`R ↔ 1 ↔ N ↔ 2`

La escala normalizada de las pistas usa ese orden físico.

## Máquina de estados normal

Estados actuales en el firmware V8.2.5/V8.3:

- `ARRANQUE`
- `REPOSO`
- `ESPERANDO_FC_C`
- `MANIOBRA`
- `ERROR_GRAVE`

Subestados de maniobra:

- `MANIOBRA_INICIO`
- `MANIOBRA_IR_A_N`
- `MANIOBRA_PAUSA_N`
- `MANIOBRA_ESPERAR_FC_S`
- `MANIOBRA_MOVER`

Las maniobras `R`, `N`, `1` y `2` tienen funciones independientes y pueden redirigirse durante el movimiento mediante las órdenes normales UP/DOWN.

## Máquina de aprendizaje

Estados actuales:

- `MODO_APRENDIZAJE`
- `APRENDIZAJE_IR_A_2`
- `APRENDIZAJE_MOVIENDO`
- `APRENDIZAJE_ESPERANDO_FC_C`
- `APRENDIZAJE_ESPERANDO_FC_S`
- `APRENDIZAJE_RECUPERANDO`
- `APRENDIZAJE_CONFIRMANDO`

El modo aprendizaje conserva la lógica específica de calibración y almacenamiento de las posiciones A/B en EEPROM.

## Potenciómetro doble

- Pista A: A0.
- Pista B: A3.
- Lectura efectiva para funcionamiento normal seleccionada mediante la lógica de redundancia.
- Se detectan fallos de rango, saltos erráticos, congelación, dirección incorrecta y discrepancia entre pistas.
- Si ambas pistas quedan falladas se entra en `ERROR_GRAVE`.
- Existe rehabilitación de pistas fuera del modo aprendizaje.

## Relés y temporización

- `K1`: se activa al iniciar una maniobra normal y se desactiva al finalizarla según la lógica existente.
- `K2`: utilizado en las secuencias que requieren cambio de carril hacia R.
- `TIEMPO_MAX_K2 = 3000 ms`.
- `TIMEOUT_MS = 3000 ms`.
- `TIEMPO_MUERTO_INVERSION_MS = 150 ms` entre inversiones IN/OUT.

## Interfaz de pruebas serie V8.3

Puerto serie: 9600 baudios.

Los comandos son **independientes de mayúsculas/minúsculas** y se terminan con ENTER.

### Consultas

- `HELP` — muestra todos los comandos disponibles.
- `STATUS` — diagnóstico completo del sistema.
- `POS` — posiciones A/B almacenadas en EEPROM.

### Movimiento por ADC

- `ADC A x` — mover hacia `x` usando exclusivamente la pista A como referencia de posición.
- `ADC B x` — mover hacia `x` usando exclusivamente la pista B como referencia de posición.
- `x` permitido: `0..1023`.

Estas órdenes son herramientas de prueba: no cambian permanentemente la pista activa de la redundancia.

### Movimiento temporal

- `MOVE IN x` — activar IN durante `x` ms.
- `MOVE OUT x` — activar OUT durante `x` ms.

La inversión respeta el tiempo muerto existente de 150 ms.

### Maniobra directa por marcha

- `G R`
- `G N`
- `G 1`
- `G 2`

`G` utiliza la lógica normal de maniobras y la redundancia A/B. Permite pedir directamente cualquier marcha; la máquina determina si debe pasar por N, esperar FC_S, activar K1/K2, etc.

Cuando `G` se ejecuta desde aprendizaje, se utiliza temporalmente la máquina normal y, si termina correctamente, se vuelve al modo aprendizaje.

### K1

- `K1 x` — activar K1 durante `x` ms.
- `K1 ON` — activar K1 indefinidamente.
- `K1 OFF` — desactivar K1.

Una maniobra automática posterior vuelve a controlar K1 mediante su lógica normal.

### K2

- `K2 x` — activar K2 durante `x` ms, limitado por `TIEMPO_MAX_K2`.
- `K2 ON` — activar K2; se mantiene el timeout normal de 3000 ms.
- `K2 OFF` — desactivar K2.

### Control

- `STOP` — detiene pruebas/maniobra y apaga IN, OUT, K1 y K2. Si el modo aprendizaje sigue seleccionado, deja el sistema en `MODO_APRENDIZAJE`.
- `RESET` / `R` — reinicia los contadores de diagnóstico no latched y actualiza las lecturas de potenciómetros.

## Prioridad de órdenes de movimiento

Para el mismo recurso físico de movimiento longitudinal, una nueva orden sustituye la anterior.

Ejemplo:

`ADC A 500` → movimiento hacia 500  
`UP` → se detiene la prueba ADC y pasa a la orden normal de UP.

Serie y botones pueden utilizarse simultáneamente.

## Comandos durante aprendizaje

Los comandos de prueba serie no se bloquean por estar en aprendizaje.

- `ADC A/B` y `MOVE` toman temporalmente el control del actuador y dejan el aprendizaje en `MODO_APRENDIZAJE` para evitar que la maniobra automática previa recupere el actuador por sorpresa.
- `G` utiliza temporalmente la máquina normal de maniobras y vuelve al aprendizaje al terminar correctamente.
- `K1`, `K2`, `STOP`, `STATUS`, `POS` y `HELP` están disponibles durante aprendizaje.

Durante aprendizaje, la confirmación de una posición sigue almacenando las lecturas actuales A/B para la marcha seleccionada.

## Validación V8.3

- Generación realizada a partir del firmware V8.2.5.
- Compilación realizada correctamente con `arduino:avr:uno` usando Arduino CLI y core AVR.
- Esta compilación verifica sintaxis y compatibilidad de compilación del firmware; no sustituye las pruebas físicas del actuador, relés, finales de carrera y sensores.

## Regla de modificación

Antes de modificar el firmware:

1. Leer el código actual de GitHub.
2. Localizar la función y el bloque afectados.
3. Revisar dependencias y efectos temporales.
4. Aplicar validación sistemática cuando se solicite comprobar la lógica.
