# PROJECT_STATE — Proyecto Trike

**Versión de referencia:** v8.1.4  
**Código principal:** `Proyecto_trike_v8.ino`  
**Rama:** `main`

## Fuente de verdad

El código actual de GitHub es la fuente principal del estado del software. Si este archivo contradice al código, prevalece el código.

Antes de analizar o modificar el firmware:

1. Leer este archivo.
2. Leer el código actual de GitHub en `main`.
3. Localizar las funciones y dependencias afectadas.

No utilizar versiones antiguas, archivos de otros proyectos ni memoria no confirmada para tomar decisiones técnicas.

No escribir cambios en GitHub sin autorización explícita del usuario.

## Estado actual

- Firmware V8.1.4.
- Código principal en un único archivo `.ino`.
- Arquitectura basada en máquina de estados no bloqueante para la lógica normal.
- Control de cuatro posiciones: R, N, 1 y 2.
- Potenciómetro doble redundante para control de posición.
- Posiciones de ambas pistas almacenadas en EEPROM.
- Gestión de relés, finales de carrera, LEDs, diagnóstico serie y modo de aprendizaje.
- La V8 elimina la recuperación automática normal hacia N tras un fallo de posicionamiento: el sistema se detiene, registra error en la marcha objetivo y permite nuevas órdenes cuando la seguridad lo permite.

## Microcontrolador

- Arduino Nano / ATmega328P.

## Pinout

| Función | Pin |
|---|---:|
| BTN UP | D3 |
| BTN DOWN | D2 |
| Relé movimiento IN | D4 |
| Relé movimiento OUT | D5 |
| K2 | D6 |
| LED R | D7 |
| LED N | D8 |
| LED 1 | D9 |
| LED 2 | D10 |
| BTN MODO | D11 |
| BTN CONF | D12 |
| K1 | D13 |
| Potenciómetro A | A0 |
| FC_S | A1 |
| FC_C | A2 |
| Potenciómetro B | A3 |
| LED aviso potenciómetro | A4 |

## Marchas

Enumeración:

- `MARCHA_R`
- `MARCHA_N`
- `MARCHA_1`
- `MARCHA_2`

Orden físico longitudinal confirmado:

`R ↔ N ↔ 1 ↔ 2`

No se realizan saltos directos físicamente imposibles.

## Máquina de estados normal

Estados actuales:

- `ARRANQUE`
- `REPOSO`
- `ESPERANDO_FC_C`
- `ESPERANDO_FC_S_PRINCIPAL`
- `MOVIENDO`
- `ESPERANDO_FC_S_CAMBIO_CARRIL`
- `ESPERA_FC_S_RETORNO`
- `ERROR_GRAVE`

También existen estados independientes del modo de aprendizaje:

- `MODO_APRENDIZAJE`
- `APRENDIZAJE_IR_A_2`
- `APRENDIZAJE_MOVIENDO`
- `APRENDIZAJE_ESPERANDO_FC_C`
- `APRENDIZAJE_ESPERANDO_FC_S`
- `APRENDIZAJE_RECUPERANDO`
- `APRENDIZAJE_CONFIRMANDO`

## Lógica normal de cambios

### N → 1 / N → 2

1. Activar K1.
2. Esperar `FC_C`.
3. Comprobar `FC_S HIGH` antes del movimiento longitudinal.
4. Si no está confirmado, esperar en `ESPERANDO_FC_S_PRINCIPAL`.
5. Mover hacia la posición objetivo.
6. Confirmar la marcha al alcanzar la posición.

Si `FC_C` o `FC_S HIGH` no se confirman dentro del timeout correspondiente, se entra en `ERROR_GRAVE`.

### 1 / 2 → N

1. Activar K1.
2. Esperar `FC_C`.
3. Mover longitudinalmente hacia N.
4. Confirmar N directamente al alcanzar su posición.

No se espera `FC_S` para confirmar el retorno desde 1 o 2, porque ya se parte del carril principal.

### N → R

1. Activar K1.
2. Esperar `FC_C`.
3. Mover hacia N si todavía no se está en N.
4. Al llegar a N, detener el actuador.
5. Activar K2.
6. Esperar que `FC_S` pase a LOW, confirmando carril R.
7. Mantener K2 activo durante esta espera.
8. Mover longitudinalmente hacia R.
9. Confirmar R y desactivar K1/K2.

Un timeout de K2 o la imposibilidad de confirmar el cambio de carril hacia R se consideran errores graves.

### R → N

1. Activar K1.
2. Esperar `FC_C`.
3. Mover longitudinalmente hacia N.
4. Al alcanzar la posición de N, detener el actuador.
5. Entrar en `ESPERA_FC_S_RETORNO`.
6. Reiniciar `tiempoInicio` al entrar en este estado.
7. Esperar `FC_S HIGH`, confirmando el retorno al carril principal.
8. Confirmar N.

Solo la maniobra `R → N` utiliza actualmente `ESPERA_FC_S_RETORNO` como parte de la confirmación normal de N.

## Cancelación y redirección

Durante una maniobra pueden recibirse nuevas órdenes.

Reglas relevantes:

- La lógica de órdenes utiliza `marchaDestino` para calcular el siguiente destino.
- `UP + DOWN` establece N como objetivo.
- Cancelar una maniobra no genera por sí mismo un error de marcha.
- Durante `N → R`, la cancelación permitida es volver a N.
- Durante `R → N`, no se permite redirigir hasta confirmar N.
- Las demás maniobras pueden cancelarse y redirigirse según la lógica de transiciones físicas.
- Al cancelar se detiene el actuador, se desactiva K2 y se reinicia la maniobra hacia el nuevo objetivo.

## Relés y temporización

- `K1`: se activa al iniciar una maniobra normal y se desactiva al confirmar marcha, registrar error de marcha o entrar en error grave.
- `K2`: se utiliza para el cambio de carril hacia R.
- `TIEMPO_MAX_K2 = 3000 ms`.
- `TIMEOUT_MS = 3000 ms`.
- `TIEMPO_MUERTO_INVERSION_MS = 150 ms`.
- Las inversiones entre IN y OUT pasan por un tiempo muerto.
- `vigilarK2()` genera `timeoutK2` si K2 permanece activo más de su tiempo máximo.

## Potenciómetro doble

- Pista A: A0.
- Pista B: A3.
- Se promedian 5 muestras por lectura.
- Se supervisan ambas pistas individualmente.
- El sistema puede detectar, entre otros:
  - lectura fuera de rango;
  - salto errático;
  - pista congelada;
  - dirección incorrecta;
  - discrepancia entre pistas;
  - pista deshabilitada por EEPROM.
- Existe selección de pista activa y lógica de rehabilitación.
- Si ambas pistas fallan, no se puede garantizar la posición y se aplica la lógica de seguridad correspondiente.
- `posEfectiva()` selecciona la posición almacenada correspondiente a la pista válida/activa.
- `lecturaEfectiva` es la referencia usada para el control de posición.

## Posiciones por defecto

Pista A:

- R: 334
- N: 703
- 1: 461
- 2: 874

Pista B:

- R: 338
- N: 709
- 1: 468
- 2: 878

Las posiciones reales pueden ser sustituidas mediante el modo de aprendizaje y almacenadas en EEPROM.

## Parámetros relevantes

- `DEBOUNCE_MS = 20`
- `VENTANA_DOBLE_PULSACION_MS = 120`
- `TIEMPO_PULSACION_LARGA_MS = 600`
- `TIMEOUT_MS = 3000`
- `TIEMPO_MUERTO_INVERSION_MS = 150`
- `TIEMPO_MAX_K2 = 3000`
- `TIEMPO_LECTURA_POT = 20`
- `TOLERANCIA_ADC = 25`
- `TIEMPO_VERIFICACION_POT = 25`
- `NUM_MUESTRAS_PROMEDIO = 5`
- `RANGO_MIN_ADC = 10`
- `RANGO_MAX_ADC = 1013`

## Errores de marcha

Existe un error independiente para:

- R
- N
- 1
- 2

Un fallo normal al alcanzar una marcha:

1. detiene IN/OUT;
2. desactiva K1/K2;
3. registra error en la marcha objetivo;
4. no realiza recuperación automática hacia N;
5. vuelve a `REPOSO`.

El error de una marcha se borra al confirmar posteriormente esa misma marcha o al reiniciar.

## Error grave

`ERROR_GRAVE`:

- detiene todos los relés;
- desactiva K1 y K2;
- limpia banderas internas de maniobra;
- hace parpadear los cuatro LEDs;
- requiere reinicio.

Entre las condiciones actuales se encuentran:

- timeout o ausencia de confirmación imprescindible de `FC_C`;
- imposibilidad de confirmar el carril principal antes de N → 1/2;
- timeout de K2 durante N → R;
- otras condiciones que impidan garantizar una posición o maniobra segura.

## Finales de carrera

- `FC_C` se considera confirmado cuando está LOW.
- `FC_S HIGH` indica carril principal 2-N-1.
- `FC_S LOW` indica carril R.

## LEDs

- LED fijo: marcha actualmente confirmada.
- LED parpadeando: marcha objetivo durante maniobra, marcha con error almacenado o indicación temporal de cancelación.
- Pueden parpadear varios LEDs simultáneamente por errores acumulados.

## Diagnóstico serie

- Puerto serie a 9600 baudios.
- `DEBUG` está actualmente activo.
- El comando `D` muestra diagnóstico del sistema.
- Se registran, entre otros:
  - versión;
  - posiciones EEPROM;
  - lecturas de potenciómetros;
  - estado;
  - marcha actual, destino y origen;
  - FC_C y FC_S;
  - K1, K2, IN y OUT;
  - errores de marcha.

## Modo de aprendizaje

Se mantiene un modo de aprendizaje/calibración separado de la lógica normal.

Permite aprender y guardar las posiciones de:

- R
- N
- 1
- 2

La revisión exhaustiva pendiente debe analizar el modo normal y el modo de aprendizaje por separado.

## Estado de validación

La lógica actual V8.1.4 incorpora las últimas correcciones de transición:

1. `MOVIENDO` solo entra en `ESPERA_FC_S_RETORNO` para `R → N`.
2. `tiempoInicio` se reinicia al entrar en `ESPERA_FC_S_RETORNO`.
3. N → 1/2 verifica `FC_S HIGH` antes del movimiento longitudinal.
4. 1/2 → N confirma N directamente por posición, sin espera innecesaria de retorno de `FC_S`.
5. N → R espera la transición de `FC_S` a LOW después de activar K2.

Pendiente principal: realizar una validación sistemática y exhaustiva de la lógica normal V8.1.4, incluyendo estados, transiciones, órdenes simultáneas, cancelaciones, redirecciones, sensores, relés, timeouts y reinicios, antes de considerar la lógica completamente validada en pruebas físicas.

## Regla de modificación

Antes de modificar el firmware:

1. Leer el código actual de GitHub.
2. Localizar la función y el bloque afectados.
3. Revisar dependencias y efectos temporales.
4. No utilizar memoria ni versiones antiguas para completar datos no confirmados.
5. Aplicar validación sistemática cuando se solicite revisar o validar la lógica.
