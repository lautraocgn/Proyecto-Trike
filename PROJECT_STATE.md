# PROJECT_STATE — Proyecto Trike

**Versión de referencia:** v8.3.2  
**Código principal:** `Proyecto_trike_v8_3.ino`  
**Base funcional:** v8.3.1 — Tiempos K1 N  
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

Las salidas de relés/actuadores se consideran activas en HIGH.

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

Estados actuales:

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

La entrada de aprendizaje se selecciona mediante D11. En las pruebas físicas se ha observado una entrada espuria al modo aprendizaje cuando la entrada D11 queda eléctricamente indefinida. La solución prevista es hardware: resistencia externa de pull-up de 10 kΩ a +5 V y conmutador a GND. No se considera pendiente un cambio de lógica de firmware para este problema.

## Potenciómetro doble

- Pista A: A0.
- Pista B: A3.
- Lectura efectiva para funcionamiento normal seleccionada mediante la lógica de redundancia.
- Se detectan fallos de rango, saltos erráticos, congelación, dirección incorrecta y discrepancia entre pistas.
- Si ambas pistas quedan falladas se entra en `ERROR_GRAVE`.
- Existe rehabilitación de pistas fuera del modo aprendizaje.

### Normalización

Las lecturas de cada pista se transforman a una escala común de 0..3000 siguiendo los puntos físicos `R → 1 → N → 2`.

La v8.3.2 corrige un problema de `uint16_t` al procesar lecturas inferiores al punto aprendido de R o superiores al punto aprendido de 2. Fuera de los extremos se satura la posición normalizada en 0 o 3000 respectivamente, evitando underflow y falsos resultados de discrepancia.

## Relés y temporización

- `K1`: se activa al iniciar una maniobra normal y se desactiva al finalizarla según la lógica existente.
- `K2`: utilizado en las secuencias que requieren cambio de carril hacia R.
- `TIEMPO_MAX_K2 = 3000 ms`.
- `TIMEOUT_MS = 3000 ms`.
- `TIEMPO_MUERTO_INVERSION_MS = 150 ms` entre inversiones IN/OUT.
- `TIEMPO_PAUSA_N = 1000 ms` cuando la secuencia requiere pausa en N.
- La ventana de doble pulsación de 500 ms se mantiene deliberadamente y no debe modificarse como solución a los tiempos de maniobra.

## Correcciones incorporadas en v8.3.2

### Paso obligatorio por N entre 1 y 2

Se estableció que los cambios `1 ↔ 2` deben pasar siempre físicamente por N, independientemente de la lectura ADC instantánea.

En particular, `1 → 2` ya no decide saltarse N basándose en la posición instantánea del potenciómetro: inicia directamente la secuencia `IR_A_N`.

Las pruebas físicas confirmaron que `N → 1 → 2` funciona pasando por N y que la llegada a 2 se completa correctamente mediante FC_S y la posición ADC aprendida.

### N → R directo

Cuando la maniobra parte de N y el destino es R, se evita la pausa de 1 s en N que se utilizaba al entrar en N desde otras marchas. K2 puede activarse inmediatamente una vez iniciada la maniobra, respetando la lógica de FC_S y movimiento final.

Esto elimina el retardo innecesario observado al solicitar R estando ya en N, sin eliminar la pausa de N necesaria en maniobras que realmente atraviesan N.

### Separación visual del monitor serie

Al registrar una nueva orden UP/DOWN se imprime una única línea separadora de `=` antes del mensaje de la orden. Se ha elegido esta solución para mejorar la lectura de las pruebas sin añadir una cantidad significativa de tráfico serie ni consumo de memoria.

## Pruebas físicas realizadas y conclusiones

### Funcionamiento confirmado

En las pruebas de esta etapa se confirmó físicamente:

- Arranque y posicionamiento automático en N.
- N → R completando el movimiento hasta la posición aprendida de R.
- R → N completando el movimiento hasta la posición aprendida de N.
- N → 1 completando el movimiento hasta la posición aprendida de 1.
- 1 → 2 completando el movimiento hasta la posición aprendida de 2.
- La lógica de `1 → 2` pasa por N y no se salta N por una lectura ADC instantánea.
- La corrección de normalización evita el `ERROR_GRAVE` observado anteriormente al trabajar cerca o por debajo del extremo R.

### Anomalías observadas

#### 1. Falsa detección de congelación durante movimiento — PENDIENTE

Se ha demostrado la siguiente cadena de fallo en una prueba anterior:

`OUT activo → ambas pistas ADC dejan de variar → los contadores de congelación alcanzan el umbral → ambas pistas se declaran falladas → ambasPistasFalladas → ERROR_GRAVE`.

La causa inmediata del `ERROR_GRAVE` está demostrada: la lógica actual interpreta la ausencia prolongada de cambio ADC en ambas pistas durante movimiento como congelación simultánea.

Lo que todavía NO está demostrado es por qué las dos pistas dejan de cambiar durante ese movimiento. No debe asumirse todavía que exista un fallo de ambos potenciómetros. Hay que determinar si el origen está en el comportamiento mecánico durante OUT, en la transmisión/actuador, en la lectura eléctrica de los ADC o en la propia condición de detección de congelación.

No se debe simplemente desactivar la detección de congelación sin entender primero esta condición.

#### 2. Incoherencia entre posición física y estado lógico — PENDIENTE

En una prueba se observó que, después de una secuencia anómala durante `2 → 1`, el sistema terminó en:

- Estado lógico: `REPOSO`.
- `marchaActual`: N.
- LED correspondiente: N.
- Lecturas ADC: aproximadamente 863/869, correspondientes a la zona física de 2 según las posiciones aprendidas de esa prueba (`N ≈ 703/709`, `2 ≈ 874/878`).

El firmware actual no detecta esta incoherencia en REPOSO porque la supervisión de potenciómetros comprueba salud de las pistas y discrepancia A/B, pero no valida de forma permanente que la posición física ADC sea coherente con `marchaActual`.

Queda pendiente definir e implementar una supervisión de coherencia entre la posición física medida y la marcha lógica confirmada. Antes de modificar el comportamiento hay que decidir qué acción de seguridad debe tomar el sistema ante una posición física inesperada.

#### 3. Entrada espuria en modo aprendizaje — PENDIENTE DE PRUEBA FÍSICA

Durante una maniobra `2 → 1` se produjo una entrada inesperada en la máquina de aprendizaje. La secuencia registrada mostró el paso por `APRENDIZAJE_IR_A_2`, incompatible con la maniobra normal solicitada.

La hipótesis de trabajo es una lectura LOW espuria en D11 debido a que la entrada estaba eléctricamente indefinida. La corrección prevista es instalar una resistencia externa de 10 kΩ pull-up a +5 V y mantener el interruptor de modo conectando D11 a GND cuando se solicite aprendizaje.

No se realizará una modificación de firmware hasta comprobar el comportamiento con D11 correctamente polarizado.

## Observaciones importantes de las pruebas

- Los cambios de marcha y los valores ADC deben analizarse conjuntamente con los finales de carrera y los relés; una lectura ADC aislada no basta para determinar el estado físico completo.
- En una prueba N → R se observaron lecturas ADC anormalmente altas al comienzo de la maniobra y, en otra, un comportamiento en el que ambas pistas variaron conjuntamente de forma inesperada. No se ha establecido todavía una causa definitiva; queda como evidencia de campo para futuras pruebas, no como diagnóstico confirmado.
- Durante una aproximación a R se observó pérdida temporal de `FC_S R`, seguida de cambio de sentido y nueva búsqueda. La lógica actual reacciona a esa pérdida del final de carrera; debe conservarse como comportamiento a considerar en futuras validaciones.
- El hecho de que A y B presenten valores similares no demuestra por sí solo que la posición física sea correcta: ambas pistas pueden coincidir en una posición incorrecta. Por ello la supervisión de coherencia absoluta con la marcha lógica es una cuestión independiente de la redundancia A/B.

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

## Validación v8.3.2

- El commit de referencia actual es `2bfe1ccc89610f04958694ddbff06106bbce2821`.
- La v8.3.2 ha sido cargada para continuar las pruebas físicas.
- La compilación de versiones anteriores se realizó correctamente con Arduino CLI y core AVR; las pruebas físicas siguen siendo necesarias para validar actuador, relés, finales de carrera y sensores.
- Las conclusiones de esta sección proceden de las pruebas físicas registradas durante la etapa v8.3.1/v8.3.2 y deben distinguirse de hipótesis todavía no verificadas.

## Pendientes inmediatos

1. Investigar y corregir la falsa detección de congelación de ambas pistas durante movimiento con `OUT`.
2. Diseñar y acordar el comportamiento ante incoherencia entre `marchaActual` y posición física ADC.
3. Instalar resistencia externa de 10 kΩ pull-up en D11 y comprobar que desaparece la entrada espuria en aprendizaje.
4. Repetir pruebas físicas de todas las maniobras después de resolver los puntos anteriores.

## Regla de modificación

Antes de modificar el firmware:

1. Leer el código actual de GitHub.
2. Localizar la función y el bloque afectados.
3. Revisar dependencias y efectos temporales.
4. Aplicar validación sistemática cuando se solicite comprobar la lógica.
