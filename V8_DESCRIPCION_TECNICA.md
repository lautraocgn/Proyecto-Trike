# Selector de marchas VW Autostick — V8.3.1

## 1. Identificación y objetivo

La V8.3.1 es la versión del firmware basada en la V8.3 y orientada al control normal del selector, diagnóstico por puerto serie y pruebas del actuador.

La lógica normal prioriza una maniobra sencilla y segura: si una marcha solicitada no puede confirmarse correctamente, el sistema detiene el movimiento, registra el error en la marcha objetivo y no realiza una recuperación automática hacia N.

Principio principal:

> Si una maniobra no confirma correctamente la marcha solicitada, se detiene, se registra el error en la marcha objetivo y el usuario puede solicitar posteriormente otra maniobra permitida.

Los errores considerados graves continúan bloqueando el funcionamiento y requieren reinicio del Arduino.

Esta versión incorpora además dos ajustes temporales en la maniobra normal:

- Tras alcanzar y confirmar una marcha, **K1 permanece activado durante 1000 ms** antes de apagarse.
- La pausa mecánica en N pasa de **500 ms a 1000 ms**.

La retención de K1 posterior a la posición no bloquea una nueva orden: si durante esos 1000 ms se recibe una nueva orden UP/DOWN, la espera se cancela inmediatamente y la nueva maniobra continúa con K1 ya activado.

## 2. Hardware y microcontrolador

- Microcontrolador: **Arduino Nano / ATmega328P**.
- Entradas de botones mediante `INPUT_PULLUP`.
- Finales de carrera mediante `INPUT_PULLUP`.
- Potenciómetro doble con dos pistas independientes.
- Dos relés para el movimiento longitudinal IN/OUT.
- K1 y K2 para el accionamiento de la mecánica de selección/carril.
- Cuatro LEDs de indicación de marcha.
- LED independiente de aviso del potenciómetro.

## 3. Pinout

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

Las salidas de relé y K1/K2 se activan con nivel **HIGH**.

## 4. Marchas y orden físico

Marchas controladas:

- R
- N
- 1ª
- 2ª

Enumeración de software:

- `MARCHA_R = 0`
- `MARCHA_N = 1`
- `MARCHA_1 = 2`
- `MARCHA_2 = 3`

El orden físico longitudinal de la caja es:

`R → 1 → N → 2`

La escala normalizada de las dos pistas del potenciómetro utiliza exactamente ese orden físico. Esto es independiente del orden de la enumeración software.

Las transiciones permitidas por las órdenes normales son:

- R ↔ N
- N ↔ 1ª
- 1ª ↔ 2ª

No se generan saltos directos entre posiciones físicamente imposibles. Las funciones internas pueden pasar por N cuando la posición actual lo requiere para realizar la maniobra de forma segura.

## 5. Máquina de estados normal

Estados principales:

- `ARRANQUE`
- `REPOSO`
- `ESPERANDO_FC_C`
- `MANIOBRA`
- `ERROR_GRAVE`

Subestados de `MANIOBRA`:

- `MANIOBRA_INICIO`
- `MANIOBRA_IR_A_N`
- `MANIOBRA_PAUSA_N`
- `MANIOBRA_ESPERAR_FC_S`
- `MANIOBRA_MOVER`

Las maniobras R, N, 1 y 2 se implementan en funciones independientes:

- `maniobraR()`
- `maniobraN()`
- `maniobra1()`
- `maniobra2()`

### ARRANQUE

En `setup()` se inicializan las entradas/salidas, se cargan las posiciones almacenadas en EEPROM, se realiza una lectura inicial del potenciómetro y se imprime el diagnóstico inicial.

El estado lógico inicial es `ARRANQUE`.

### REPOSO

No hay una maniobra normal activa. Se aceptan nuevas órdenes UP/DOWN.

En este estado también se gestiona la retención temporal de K1 posterior a una posición confirmada. Mientras la retención está activa, K1 permanece físicamente ON hasta que transcurren 1000 ms, salvo que una nueva orden inicie antes otra maniobra.

### ESPERANDO_FC_C

Espera la confirmación necesaria de `FC_C` antes de continuar con las maniobras que la requieren.

La ausencia de una confirmación imprescindible de `FC_C` puede producir un error grave según la maniobra concreta.

### MANIOBRA

Ejecuta la secuencia correspondiente a `marchaDestino`.

Antes de cada movimiento longitudinal se comprueban las condiciones de carril que correspondan. La posición objetivo se obtiene mediante `posEfectiva()` y la lectura efectiva del potenciómetro.

### ERROR_GRAVE

Detiene la operación y apaga los accionamientos. Los cuatro LEDs de marcha parpadean y el sistema permanece bloqueado hasta reiniciar el Arduino.

## 6. Flujo general de una maniobra

Una maniobra normal sigue este principio:

1. Se recibe una orden UP o DOWN.
2. Se determina la marcha destino.
3. Se inicia la maniobra y se activa K1.
4. Se comprueban las condiciones de finales de carrera necesarias.
5. Se mueve el actuador mediante IN/OUT hacia la posición memorizada.
6. El potenciómetro determina si se ha alcanzado la posición objetivo.
7. Cuando la posición es correcta, la marcha se confirma.
8. Se detiene el movimiento longitudinal y se desactiva K2 cuando corresponde.
9. K1 permanece activado durante 1000 ms después de la confirmación.
10. Transcurrido ese tiempo, K1 se desactiva si no ha comenzado otra maniobra.

Si durante la maniobra aparece un fallo normal de posicionamiento o un timeout de movimiento:

- se detiene el actuador;
- se registra `errorMarcha[marchaDestino]`;
- no se recupera automáticamente hacia N;
- el sistema vuelve a `REPOSO` si no existe un error grave.

## 7. Retención de K1 después de alcanzar una marcha

Cuando `confirmarMarcha()` confirma correctamente una posición:

- se apaga IN/OUT;
- se desactiva K2;
- se actualiza `marchaActual`;
- se actualiza `marchaDestino`;
- se borra el error de esa marcha;
- se pasa a `REPOSO`;
- se inicia una retención temporal de K1 de **1000 ms**.

La retención se implementa mediante:

- `retencionK1PostPosicionActiva`
- `inicioRetencionK1PostPosicion`
- `gestionarRetencionK1PostPosicion()`

No se introduce un estado adicional en la máquina de maniobras.

### Nueva orden durante la retención

Si el usuario pulsa UP/DOWN durante los 1000 ms:

1. `REPOSO` obtiene la nueva orden.
2. `iniciarCambioA()` cancela la retención temporal.
3. K1 no se apaga.
4. Se inicia la nueva maniobra con K1 ya activado.

Por tanto, la retención posterior no introduce una espera adicional entre maniobras cuando el usuario solicita inmediatamente otro cambio.

Si no llega ninguna orden, al cumplirse 1000 ms se ejecuta `desactivarK1()`.

## 8. Pausa en N

Cuando una maniobra necesita pasar por N antes de continuar, se utiliza el subestado `MANIOBRA_PAUSA_N`.

El tiempo actual es:

`TIEMPO_PAUSA_N = 1000 ms`

Durante esta pausa:

- el actuador longitudinal permanece apagado;
- se espera el tiempo completo antes de continuar la secuencia;
- al finalizar se ejecuta `continuarDesdePausaN()`.

La pausa se utiliza en las maniobras hacia R, 1ª y 2ª cuando deben pasar por N.

## 9. Maniobra hacia N

`maniobraN()` es la secuencia más directa:

1. Comprueba si la lectura efectiva está dentro de la tolerancia de N.
2. Si lo está, confirma N.
3. Si no lo está, mueve el actuador hacia la posición memorizada de N.
4. Si transcurren 3000 ms sin alcanzar N, registra error en N.

## 10. Maniobra hacia R

La maniobra hacia R utiliza los subestados de cambio de carril:

### `MANIOBRA_INICIO`

- Si `FC_S` ya indica carril R, se inicia directamente el movimiento final hacia R.
- Si no, se inicia el desplazamiento hacia N.

### `MANIOBRA_IR_A_N`

- Se mueve hacia N.
- Al alcanzar N se apaga el actuador y comienza la pausa de 1000 ms.
- Si se agota el timeout de 3000 ms antes de alcanzar N, se registra error en R.

### `MANIOBRA_PAUSA_N`

Espera `TIEMPO_PAUSA_N = 1000 ms`.

Después:

1. se activa K2;
2. se comprueba `FC_S` para el carril R;
3. si ya está en R, se inicia el movimiento final;
4. si no, se espera `FC_S`.

### `MANIOBRA_ESPERAR_FC_S`

Espera `FC_S` indicando carril R. Si no llega en 3000 ms, entra en `ERROR_GRAVE`.

### `MANIOBRA_MOVER`

Con `FC_S` confirmado como carril R:

- si el objetivo cambia, se vuelve a la lógica de inicio;
- si se pierde el carril R, se detiene el actuador y se reinicia la secuencia de maniobra;
- si se alcanza la posición memorizada de R, se confirma R;
- si no, se mueve hacia R;
- si pasan 3000 ms sin alcanzar R, se registra error en R.

## 11. Maniobras hacia 1ª y 2ª

Las maniobras hacia 1ª y 2ª comparten la misma estructura general.

### Inicio

Primero se exige que `FC_S` indique el carril principal.

Si el potenciómetro indica que el actuador se encuentra dentro del tramo que hace necesario pasar por N, se ejecuta `iniciarIrAN()` y se realiza la secuencia de paso por N.

En caso contrario se inicia directamente el movimiento final hacia la marcha objetivo.

### Paso por N

Si se alcanza N:

1. se apaga el actuador;
2. comienza `MANIOBRA_PAUSA_N`;
3. se esperan 1000 ms;
4. `continuarDesdePausaN()` comprueba nuevamente el carril principal;
5. si el carril es correcto, se inicia el movimiento final;
6. si no, se espera `FC_S`.

### Espera de FC_S

Para 1ª y 2ª se espera `FC_S` en carril principal. Si no se obtiene en 3000 ms se entra en `ERROR_GRAVE`.

### Movimiento final

Con el carril principal confirmado:

- si el objetivo cambia, la maniobra se redirige;
- si se pierde `FC_S`, se detiene el actuador y se vuelve a la lógica de inicio;
- si se alcanza la posición memorizada, se confirma la marcha;
- si pasan 3000 ms sin alcanzarla, se registra error en la marcha objetivo.

## 12. FC_S

`FC_S` identifica el carril mecánico:

- `HIGH` = carril principal;
- `LOW` = carril R.

Para movimientos longitudinales hacia 1ª y 2ª, el carril principal debe estar confirmado antes de iniciar el movimiento final.

Para R, el carril R debe estar confirmado antes del movimiento final hacia R.

Si durante un movimiento final se pierde la condición de carril esperada, el actuador se detiene y la maniobra vuelve a su lógica de preparación correspondiente.

## 13. FC_C

`FC_C` se utiliza para confirmar una condición mecánica necesaria antes de continuar con determinadas secuencias de cambio de carril.

La ausencia de una confirmación imprescindible de `FC_C` se considera una condición grave en las rutas donde dicha confirmación es necesaria.

## 14. K1 y K2

### K1

K1 se activa al iniciar una maniobra normal mediante `iniciarCambioA()`.

Su comportamiento normal es:

- ON durante la maniobra;
- permanece ON 1000 ms después de confirmar la posición;
- OFF al finalizar esa retención;
- si llega una nueva orden durante la retención, no se apaga y la nueva maniobra continúa con K1 ya ON.

El modo de pruebas serie también puede controlar K1 independientemente mediante sus comandos específicos.

### K2

K2 se utiliza para las secuencias que necesitan cambiar al carril R.

Supervisión temporal:

`TIEMPO_MAX_K2 = 3000 ms`

Si K2 permanece activo más de ese tiempo, se produce `TIMEOUT K2`.

La respuesta posterior depende de la maniobra:

- durante una maniobra hacia R, el timeout provoca `ERROR_GRAVE`;
- en otras maniobras, el timeout registra error en la marcha destino.

## 15. Relés IN/OUT

Los relés longitudinales son:

- `PIN_REL_IN = D4`
- `PIN_REL_OUT = D5`

Ambos se activan con HIGH.

Nunca se permite mantener IN y OUT activados simultáneamente. Si se solicita invertir el sentido mientras el contrario está activo:

1. se apagan ambos relés;
2. se inicia un tiempo muerto de inversión;
3. se esperan 150 ms;
4. posteriormente se permite activar el nuevo sentido.

`TIEMPO_MUERTO_INVERSION_MS = 150 ms`.

## 16. Redirección de maniobra

Una maniobra normal puede recibir una nueva orden UP/DOWN antes de alcanzar el objetivo anterior.

La lógica mantiene un objetivo pendiente durante una ventana de doble pulsación de:

`VENTANA_DOBLE_PULSACION_MS = 500 ms`

Las pulsaciones recibidas dentro de esa ventana se acumulan sobre el destino pendiente.

Cuando la ventana termina se establece el nuevo objetivo y la maniobra se redirige.

La nueva orden no provoca una recuperación automática a N por el mero hecho de cambiar el objetivo. La maniobra se adapta a la nueva posición y a las condiciones mecánicas actuales.

## 17. UP + DOWN

La pulsación simultánea de UP y DOWN tiene prioridad y establece N como nuevo objetivo.

La orden:

- cancela la selección de destino pendiente;
- establece N como destino;
- no borra errores de marcha.

## 18. Botones

Los botones utilizan `INPUT_PULLUP`.

La lectura física se interpreta como:

- `LOW` = botón pulsado;
- `HIGH` = botón no pulsado.

La orden se genera en el flanco lógico de pulsación después del antirrebote.

Parámetros actuales:

- `DEBOUNCE_MS = 20 ms`;
- ventana de doble pulsación: 500 ms;
- pulsación larga: 600 ms.

El código mantiene el bloqueo temporal asociado a la gestión de botones.

## 19. Potenciómetro doble

El sistema utiliza dos pistas redundantes:

- pista A: A0;
- pista B: A3.

Cada lectura se obtiene mediante promedio de 5 muestras.

Parámetros principales:

- rango válido: ADC 10..1013;
- promedio: 5 muestras;
- periodo de verificación: 25 ms;
- tolerancia de posición: 25 ADC;
- umbral de salto errático: 120 ADC;
- detección de congelación: 3 ADC de variación máxima y 8 lecturas consecutivas;
- confirmación de fallo de pista: 3 lecturas;
- confirmación de rango válido: 2 lecturas;
- confirmación de discrepancia: 3 lecturas.

Se detectan, entre otros:

- fuera de rango;
- salto errático;
- pista congelada;
- dirección incorrecta;
- discrepancia entre pistas;
- pista deshabilitada por EEPROM.

Existe lógica de selección de pista activa, validación individual y rehabilitación de pistas.

Si ambas pistas quedan falladas y no existe una lectura segura que permita garantizar la posición, el sistema puede entrar en `ERROR_GRAVE`.

## 20. Escala normalizada A/B

Para comparar las dos pistas pese a sus diferentes valores ADC, cada pista se transforma a una escala común de 0..3000 mediante interpolación por tramos.

El orden físico usado es:

`R → 1 → N → 2`

Cada pista utiliza sus propios cuatro puntos memorizados para construir la escala.

La discrepancia A/B se evalúa sobre esta escala normalizada, no directamente sobre la diferencia ADC bruta.

La posición efectiva utilizada por el control normal se obtiene mediante `posEfectiva()` y la lógica de redundancia disponible.

## 21. Posiciones EEPROM

Se almacenan las posiciones de las cuatro marchas para ambas pistas:

- R: A/B;
- N: A/B;
- 1ª: A/B;
- 2ª: A/B.

La firma de EEPROM utilizada es:

`0xA5`

Direcciones base:

- pista A: dirección 2;
- pista B: dirección 10.

Valores por defecto definidos actualmente en el firmware:

| Marcha | A | B |
|---|---:|---:|
| R | 334 | 338 |
| N | 703 | 709 |
| 1ª | 461 | 468 |
| 2ª | 874 | 878 |

Estos valores son los valores por defecto definidos por código; durante el modo aprendizaje pueden sustituirse por las posiciones aprendidas y guardadas en EEPROM.

## 22. Errores por marcha

Existe un indicador independiente para cada marcha:

- `errorMarcha[R]`
- `errorMarcha[N]`
- `errorMarcha[1]`
- `errorMarcha[2]`

Los errores se acumulan entre maniobras.

Un error de marcha se borra cuando:

1. esa misma marcha se alcanza y confirma correctamente; o
2. se reinicia el Arduino.

Un fallo normal de posicionamiento no bloquea por sí solo todo el selector.

Ejemplo:

- 1ª confirmada → LED 1 fijo.
- 2ª con error pendiente → LED 2 parpadeando.

Por tanto, los indicadores de marcha y los errores pueden coexistir.

## 23. Errores graves

Las condiciones graves detienen y bloquean el selector.

Entre ellas se encuentran las condiciones que impiden garantizar de forma segura la posición o el carril, por ejemplo:

- fallo grave de finales de carrera;
- ausencia de una confirmación imprescindible de `FC_C`;
- timeout de K2 en una ruta que requiere su funcionamiento seguro;
- fallo grave del potenciómetro;
- ambas pistas del potenciómetro falladas sin una referencia segura disponible;
- incoherencias críticas de hardware o posición;
- cualquier otra condición que impida garantizar una operación segura.

Ante `ERROR_GRAVE`:

- se apaga IN;
- se apaga OUT;
- se apaga K1;
- se apaga K2;
- se cancela la retención temporal de K1;
- se detiene la máquina normal;
- los cuatro LEDs de marcha parpadean;
- es necesario reiniciar el Arduino.

## 24. Indicadores LED

LEDs de marcha:

- R → D7;
- N → D13;
- 1ª → D9;
- 2ª → D10.

### LED fijo

Indica la marcha actualmente confirmada.

### LED parpadeando durante una maniobra

Indica la marcha objetivo o una posición implicada por la transición actual, según la lógica de indicadores de la maniobra.

### LED parpadeando al finalizar

Indica un error pendiente de esa marcha.

Pueden existir varios LEDs parpadeando simultáneamente debido a errores acumulados.

LED de aviso del potenciómetro:

- A4.

Se utiliza para indicar el estado de alerta asociado a las pistas del potenciómetro.

## 25. Modo de aprendizaje

El modo aprendizaje se mantiene separado de la máquina normal.

Estados:

- `MODO_APRENDIZAJE`
- `APRENDIZAJE_IR_A_2`
- `APRENDIZAJE_MOVIENDO`
- `APRENDIZAJE_ESPERANDO_FC_C`
- `APRENDIZAJE_ESPERANDO_FC_S`
- `APRENDIZAJE_RECUPERANDO`
- `APRENDIZAJE_CONFIRMANDO`

El modo aprendizaje permite registrar las posiciones de:

- R
- N
- 1ª
- 2ª

Para cada posición se almacenan las lecturas de las pistas A y B.

Al entrar en aprendizaje se desconectan los relés y se inicia la secuencia específica de calibración. Al salir del modo aprendizaje se apagan los accionamientos y se vuelve a `ARRANQUE`.

## 26. Interfaz de pruebas por puerto serie

Puerto serie:

`9600 baudios`

Los comandos no distinguen mayúsculas/minúsculas y se finalizan con ENTER.

### Consultas

- `HELP` — muestra la ayuda completa.
- `STATUS` — muestra el estado completo del sistema, relés, K1/K2, errores y diagnóstico de potenciómetros.
- `POS` — muestra las posiciones A/B almacenadas en EEPROM.

### Movimiento por ADC

- `ADC A x` — mueve hacia el valor ADC `x` utilizando exclusivamente la pista A como referencia.
- `ADC B x` — mueve hacia el valor ADC `x` utilizando exclusivamente la pista B como referencia.

`x` debe estar entre 0 y 1023.

Estas pruebas no cambian permanentemente la pista activa de la redundancia.

### Movimiento temporal

- `MOVE IN x` — activa IN durante `x` ms.
- `MOVE OUT x` — activa OUT durante `x` ms.

La inversión respeta el tiempo muerto de 150 ms.

### Maniobra directa por marcha

- `G R`
- `G N`
- `G 1`
- `G 2`

`G` utiliza la máquina normal de maniobras y permite pedir directamente una marcha.

### K1

- `K1 x` — activa K1 durante `x` ms.
- `K1 ON` — activa K1 indefinidamente.
- `K1 OFF` — desactiva K1.

### K2

- `K2 x` — activa K2 durante `x` ms, con supervisión máxima de 3000 ms.
- `K2 ON` — activa K2 y mantiene su supervisión temporal normal.
- `K2 OFF` — desactiva K2.

### Control

- `STOP` — detiene una prueba o maniobra y apaga IN, OUT, K1 y K2.
- `RESET` / `R` — reinicia los contadores de diagnóstico no latched y actualiza las lecturas de potenciómetro.

Las órdenes de prueba serie pueden utilizarse durante aprendizaje. Los comandos de movimiento de prueba toman temporalmente el control del actuador para evitar que una maniobra automática previa vuelva a activarlo inesperadamente.

## 27. Prioridad de órdenes

El sistema permite combinar botones y puerto serie.

Dentro de un mismo ciclo del `loop()`:

1. se leen los botones;
2. se procesa el puerto serie;
3. una orden serie posterior puede sustituir una orden de botón anterior.

Una orden UP/DOWN durante una prueba de movimiento cancela la prueba y devuelve el control a la lógica normal.

Una nueva orden normal durante una maniobra puede redirigir el objetivo. La retención K1 posterior a una posición confirmada también se cancela inmediatamente cuando se inicia una nueva maniobra.

## 28. Temporizaciones principales

| Parámetro | Valor |
|---|---:|
| Antirrebote | 20 ms |
| Ventana doble pulsación | 500 ms |
| Pulsación larga | 600 ms |
| Timeout de movimiento | 3000 ms |
| Tiempo muerto inversión IN/OUT | 150 ms |
| Tiempo máximo K2 | 3000 ms |
| Verificación potenciómetro | 25 ms |
| Paso de aprendizaje | 100 ms |
| Pausa en N | **1000 ms** |
| Retención K1 tras posición confirmada | **1000 ms** |
| Tolerancia de posición | 25 ADC |

## 29. Diagnóstico serie

Con `DEBUG` activo, el firmware registra eventos de interés como:

- cambio de marcha solicitado;
- marcha alcanzada;
- activación/desactivación de relés;
- cambios de `FC_C` y `FC_S` relevantes;
- timeout de K2;
- errores de marcha;
- entrada en error grave;
- lecturas A/B del potenciómetro;
- pista activa;
- discrepancia normalizada A/B;
- estado general del sistema.

`STATUS` muestra además:

- estado actual;
- marcha actual;
- destino;
- origen;
- FC_C;
- FC_S;
- K1;
- K2;
- IN/OUT;
- pruebas activas;
- errores R/N/1/2;
- estado detallado de las dos pistas del potenciómetro.

## 30. Arquitectura de pruebas

El firmware contiene herramientas de prueba separadas de la lógica normal:

- movimiento a ADC A/B;
- movimiento por tiempo IN/OUT;
- prueba temporizada de K1;
- prueba temporizada de K2;
- maniobra directa por marcha mediante `G`.

Las pruebas permiten verificar el actuador, los relés, K1/K2, la lectura del potenciómetro y la secuencia normal sin tener que modificar la máquina principal.

## 31. Comportamiento tras reinicio

El arranque vuelve a inicializar las salidas en estado seguro:

- IN OFF;
- OUT OFF;
- K1 OFF;
- K2 OFF;
- LEDs OFF inicialmente.

Después se cargan las posiciones EEPROM y se realiza la lectura inicial de los potenciómetros.

Los errores de marcha son variables de RAM, por lo que no son persistentes a través de un reinicio.

## 32. Validación y límites

La versión actual ha sido compilada correctamente para la plataforma AVR utilizada durante el desarrollo. La compilación valida sintaxis, tipos y compatibilidad de las funciones con el entorno de Arduino utilizado.

La compilación **no sustituye las pruebas físicas** del selector.

Queda por verificar físicamente, entre otros aspectos:

- funcionamiento real de K1 y K2 con la electrónica instalada;
- sentido real IN/OUT;
- tiempos mecánicos del actuador;
- comportamiento de FC_C y FC_S;
- posiciones reales aprendidas;
- comportamiento del potenciómetro doble bajo movimiento;
- efecto mecánico de la retención de K1 durante 1000 ms;
- efecto de la pausa de 1000 ms en N;
- redirecciones durante movimiento;
- recuperación después de pérdida o incoherencia de sensores.

Por tanto, el comportamiento descrito como lógica de firmware está definido por el código, mientras que el comportamiento mecánico real debe validarse en la moto.

## 33. Observación sobre identificadores de versión

El encabezado del archivo identifica el firmware como **V8.3.1 — Tiempos K1 N**.

El texto de diagnóstico inicial `printDiagnosticoInicial()` todavía imprime `VERSION V8.2.5`. Ese texto es un identificador de diagnóstico heredado y no coincide con el encabezado de la versión actual; no modifica la lógica de funcionamiento.

## 34. Diferencia fundamental respecto a versiones anteriores

La diferencia arquitectónica principal de la V8 respecto a la lógica anterior es:

> Un fallo al alcanzar una marcha ya no provoca, como respuesta normal, una maniobra automática de recuperación hacia N.

La V8/V8.3.1 prioriza:

1. detener;
2. informar;
3. conservar el error;
4. permitir una nueva orden cuando la seguridad lo permita.

Además, la V8.3.1 incorpora los ajustes temporales de esta versión:

- **1000 ms de retención de K1 después de confirmar una marcha**;
- **1000 ms de pausa en N**;
- cancelación inmediata de la retención de K1 si llega una nueva orden durante esos 1000 ms.

La recuperación automática hacia N podrá reconsiderarse en una versión futura después de validar el comportamiento mecánico real.
