# PROJECT_STATE — Proyecto Trike

**Versión de referencia:** v8.3.4 — Control IN/OUT por PWM  
**Código principal:** `Proyecto_trike_v8_3.ino`  
**Base funcional:** v8.3.3 — Interfaz de pruebas serie  
**Rama:** `main`

> Nota de sincronización: las modificaciones de v8.3.4 descritas en este documento corresponden al firmware de trabajo probado físicamente. El código de GitHub debe quedar sincronizado con esta versión mediante el commit de v8.3.4. Si existe contradicción entre este documento y el código publicado, prevalece el código.

## Fuente de verdad

El código actual de GitHub es la fuente principal. Si este archivo contradice al firmware, prevalece el código.

Antes de modificar el firmware:

1. Leer este archivo.
2. Leer el código actual de GitHub.
3. Revisar las funciones y dependencias afectadas.

No usar versiones antiguas, otros proyectos ni memoria no confirmada para completar datos técnicos.

## Microcontrolador

- Arduino Nano / ATmega328P.

## Pinout actual v8.3.4

| Función | Pin |
|---|---:|
| BTN UP | D3 |
| BTN DOWN | D2 |
| Movimiento IN / PWM | D6 |
| Movimiento OUT / PWM | D5 |
| K2 | D4 |
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

Las salidas de movimiento se consideran activas en HIGH. D6 y D5 se utilizan como salidas PWM para controlar la velocidad del actuador. K2 se ha trasladado a D4.

## Posiciones

Enumeración de software:

- `MARCHA_R`
- `MARCHA_N`
- `MARCHA_1`
- `MARCHA_2`

Orden físico longitudinal confirmado:

`R ↔ 1 ↔ N ↔ 2`

La escala normalizada de las pistas usa ese orden físico.

Posiciones EEPROM utilizadas en las pruebas:

- R: A `334`, B `338`
- N: A `703`, B `709`
- 1: A `461`, B `468`
- 2: A `874`, B `878`

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

## Control de velocidad del actuador — v8.3.4

La salida de movimiento IN y OUT se controla mediante PWM integrado en las funciones `activarReleIn()` y `activarReleOut()`, manteniendo centralizada la lógica existente de inversión y tiempo muerto.

Configuración validada en las pruebas de esta etapa:

- `TOLERANCIA_ADC = 25`.
- Distancia de frenado: `50 ADC`.
- Distancia `>50 ADC`: PWM rápido `255` (100 %).
- Distancia `26..50 ADC`: PWM lento `60` (~24 %).
- Distancia `<=25 ADC`: se considera posición alcanzada y se detiene el actuador.

No se añadió un tercer escalón de velocidad.

El PWM se actualiza mientras el sentido permanece activo, por lo que la transición de velocidad se produce sin perder la gestión centralizada de IN/OUT.

El tiempo muerto entre inversiones se mantiene en `150 ms`.

### Motivo de la estrategia

Las pruebas anteriores con tolerancia de 15 ADC y PWM lento del 50 % produjeron oscilaciones y grandes sobrepasos alrededor del objetivo. Se decidió volver a una tolerancia de 25 ADC y reducir la velocidad de aproximación a aproximadamente 25 %.

Las pruebas con `TOLERANCIA_ADC = 25`, frenado desde 50 ADC y PWM lento `60` mostraron una mejora clara en la aproximación y una reducción importante del sobrepaso respecto a la configuración anterior.

### Limitación de las pruebas

Las pruebas de esta etapa se realizaron con el actuador **desconectado físicamente de la caja de cambios**. Por tanto, la inercia y la dinámica observadas no representan todavía la condición final bajo carga.

Se ha observado una ligera tendencia global a terminar por debajo del objetivo en las maniobras probadas, aunque dentro de la tolerancia. No se considera suficientemente representativa para modificar el control antes de probar el actuador instalado en la caja.

El posible comportamiento bajo carga sigue sin estar verificado. Es razonable esperar que la carga mecánica pueda aumentar la tendencia a quedarse corto, pero esto es una hipótesis que debe comprobarse físicamente.

## Freno eléctrico — idea pendiente

El hardware permite considerar como alternativa futura un frenado eléctrico aplicando positivo a ambas bornas del motor simultáneamente.

La idea queda registrada como posible mecanismo para reducir el recorrido residual después de cortar el movimiento si las pruebas con el actuador instalado demuestran un sobrepaso sistemático.

**No está implementado en v8.3.4.** No debe incorporarse al firmware hasta verificar eléctricamente el comportamiento del puente H bajo esta condición y demostrar mediante pruebas que resulta necesario.

## Relés y temporización

- `K1`: se activa al iniciar una maniobra normal y se desactiva al finalizarla según la lógica existente.
- `K2`: utilizado en las secuencias que requieren cambio de carril hacia R.
- `TIEMPO_MAX_K2 = 3000 ms`.
- `TIMEOUT_MS = 4000 ms` en el firmware de trabajo v8.3.4.
- `TIEMPO_MUERTO_INVERSION_MS = 150 ms` entre inversiones IN/OUT.
- `TIEMPO_PAUSA_N = 1000 ms` cuando la secuencia requiere pausa en N.
- La ventana de doble pulsación de 500 ms se mantiene deliberadamente y no debe modificarse como solución a los tiempos de maniobra.

## Correcciones incorporadas en v8.3.2

### Paso obligatorio por N entre 1 y 2

Se estableció que los cambios `1 ↔ 2` deben pasar siempre físicamente por N, independientemente de la lectura ADC instantánea.

En particular, `1 → 2` ya no decide saltarse N basándose en la posición instantánea del potenciómetro: inicia directamente la secuencia `IR_A_N`.

### N → R directo

Cuando la maniobra parte de N y el destino es R, se evita la pausa de 1 s en N que se utilizaba al entrar en N desde otras marchas. K2 puede activarse inmediatamente una vez iniciada la maniobra, respetando la lógica de FC_S y movimiento final.

### Separación visual del monitor serie

Al registrar una nueva orden UP/DOWN se imprime una única línea separadora de `=` antes del mensaje de la orden.

## Pruebas físicas y conclusiones de v8.3.4

### Control PWM

Se realizaron pruebas de movimiento directo por ADC y maniobras completas utilizando PWM.

Con PWM lento `60` y frenado desde 50 ADC se observaron aproximaciones considerablemente más suaves que con PWM lento `128`.

Ejemplos de posiciones finales observadas en maniobras completas:

- N → 1: `465` frente a objetivo `461` → `+4 ADC`.
- 1 → N: `708` frente a `703` → `+5 ADC`.
- N → R: `330` frente a `334` → `−4 ADC`.
- R → 1: `453` frente a `461` → `−8 ADC`.
- 2 → 1: `460` frente a `461` → `−1 ADC`.
- 1 → N: `709` frente a `703` → `+6 ADC`.

También se observaron casos de aproximadamente `−16/-17 ADC`, todavía dentro de la tolerancia de 25 ADC.

En una prueba ADC hacia `650`, la aproximación finalizó en `659` tras entrar en la zona lenta, mostrando un error de `+9 ADC`.

### Tendencia de posición

En el conjunto de maniobras registrado se observó una ligera tendencia a quedar corto respecto al objetivo, no una tendencia dominante a sobrepasarlo.

Esta conclusión es únicamente válida para las pruebas realizadas con el actuador desacoplado de la caja. No debe extrapolarse todavía a la condición bajo carga.

### Estado actual de la estrategia

La configuración PWM actual se considera **provisionalmente adecuada para continuar las pruebas**.

No modificar de momento tolerancia, distancia de frenado ni PWM lento. La siguiente validación importante debe realizarse con el actuador instalado y conectado mecánicamente a la caja.

## Anomalías observadas

### 1. Falsa detección de congelación durante movimiento — PENDIENTE

Se ha demostrado la siguiente cadena de fallo en una prueba anterior:

`OUT activo → ambas pistas ADC dejan de variar → los contadores de congelación alcanzan el umbral → ambas pistas se declaran falladas → ambasPistasFalladas → ERROR_GRAVE`.

La causa inmediata del `ERROR_GRAVE` está demostrada: la lógica actual interpreta la ausencia prolongada de cambio ADC en ambas pistas durante movimiento como congelación simultánea.

Lo que todavía NO está demostrado es por qué las dos pistas dejan de cambiar durante ese movimiento. No debe asumirse todavía que exista un fallo de ambos potenciómetros. Hay que determinar si el origen está en el comportamiento mecánico durante OUT, en la transmisión/actuador, en la lectura eléctrica de los ADC o en la propia condición de detección de congelación.

No se debe simplemente desactivar la detección de congelación sin entender primero esta condición.

### 2. Incoherencia entre posición física y estado lógico — PENDIENTE

En una prueba se observó que, después de una secuencia anómala durante `2 → 1`, el sistema terminó con estado lógico N mientras las lecturas ADC correspondían a la zona física de 2.

El firmware actual no detecta esta incoherencia en REPOSO porque la supervisión de potenciómetros comprueba salud de las pistas y discrepancia A/B, pero no valida de forma permanente que la posición física ADC sea coherente con `marchaActual`.

Queda pendiente definir e implementar una supervisión de coherencia entre la posición física medida y la marcha lógica confirmada. Antes de modificar el comportamiento hay que decidir qué acción de seguridad debe tomar el sistema ante una posición física inesperada.

### 3. Entrada espuria en modo aprendizaje — PENDIENTE DE PRUEBA FÍSICA

Durante una maniobra `2 → 1` se produjo una entrada inesperada en la máquina de aprendizaje. La hipótesis de trabajo es una lectura LOW espuria en D11 debido a que la entrada estaba eléctricamente indefinida.

La corrección prevista es instalar una resistencia externa de 10 kΩ pull-up a +5 V y mantener el interruptor de modo conectando D11 a GND cuando se solicite aprendizaje.

No se realizará una modificación de firmware hasta comprobar el comportamiento con D11 correctamente polarizado.

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

### K1

- `K1 x` — activar K1 durante `x` ms.
- `K1 ON` — activar K1 indefinidamente.
- `K1 OFF` — desactivar K1.

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

## Pendientes inmediatos

1. Probar v8.3.4 con el actuador instalado y conectado a la caja de cambios.
2. Comparar bajo carga la precisión, sobrepaso, tendencia a quedarse corto y repetibilidad de las maniobras.
3. Mantener el freno eléctrico como posible solución futura únicamente si las pruebas reales demuestran sobrepaso residual significativo.
4. Investigar y corregir la falsa detección de congelación de ambas pistas durante movimiento con `OUT`.
5. Diseñar y acordar el comportamiento ante incoherencia entre `marchaActual` y posición física ADC.
6. Instalar resistencia externa de 10 kΩ pull-up en D11 y comprobar que desaparece la entrada espuria en aprendizaje.

## Regla de modificación

Antes de modificar el firmware:

1. Leer el código actual de GitHub.
2. Localizar la función y el bloque afectados.
3. Revisar dependencias y efectos temporales.
4. Aplicar validación sistemática cuando se solicite comprobar la lógica.
