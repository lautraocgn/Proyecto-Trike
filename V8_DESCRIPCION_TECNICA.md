# Selector de marchas VW Autostick — V8

## Objetivo de la V8

La V8 simplifica deliberadamente la lógica de control: ante un fallo normal al alcanzar una marcha, el sistema **detiene el movimiento y no vuelve automáticamente a N**.

Principio principal:

> Si una maniobra no confirma correctamente la marcha solicitada, se detiene, se registra el error en la marcha objetivo y el usuario puede solicitar posteriormente otra maniobra permitida.

Los **errores graves** continúan bloqueando el funcionamiento.

## Marchas y transiciones

Marchas controladas:

- R
- N
- 1ª
- 2ª

Las transiciones respetan la mecánica:

- R ↔ N
- N ↔ 1ª
- 1ª ↔ 2ª

No se realizan saltos directos entre posiciones físicamente imposibles.

## Estados principales

La máquina de estados normal utiliza:

- `ARRANQUE`
- `REPOSO`
- `ESPERANDO_FC_C`
- `ESPERANDO_FC_S_PRINCIPAL`
- `MOVIENDO`
- `ESPERANDO_FC_S_CAMBIO_CARRIL`
- `ESPERA_FC_S_RETORNO`
- `ERROR_GRAVE`

También se mantienen los estados del modo de aprendizaje.

### ARRANQUE

Inicializa el sistema, carga las posiciones memorizadas, lee el potenciómetro y determina las condiciones iniciales necesarias.

### REPOSO

No hay maniobra activa. Puede mostrar una marcha confirmada, errores pendientes y aceptar una nueva orden permitida.

### ESPERANDO_FC_C

Espera una confirmación necesaria de `FC_C`. La falta de una confirmación imprescindible de este final de carrera se considera error grave.

### ESPERANDO_FC_S_PRINCIPAL

Espera la confirmación del carril principal antes de iniciar un movimiento longitudinal que requiera esa condición.

### MOVIENDO

El sistema se desplaza hacia `marchaDestino`. La llegada se determina mediante la posición efectiva del potenciómetro y las condiciones de finales de carrera aplicables.

### ESPERANDO_FC_S_CAMBIO_CARRIL y ESPERA_FC_S_RETORNO

Estados utilizados para las confirmaciones de `FC_S` asociadas a las maniobras de cambio y retorno de carril.

### ERROR_GRAVE

Detiene la operación. Los cuatro LEDs de marcha parpadean y se requiere reiniciar el Arduino.

## Maniobra normal

Una maniobra sigue este principio:

1. Se recibe una marcha objetivo.
2. Se comprueban las condiciones de seguridad necesarias.
3. Se acciona la mecánica correspondiente.
4. El potenciómetro determina la llegada a la posición memorizada.
5. Los finales de carrera confirman el carril cuando corresponde.
6. Si todo es correcto, la marcha queda confirmada.
7. Si falla una condición o se agota el tiempo:
   - se detiene el movimiento;
   - se registra error en la marcha objetivo;
   - no se vuelve automáticamente a N;
   - puede aceptarse otra orden si no existe error grave.

## Errores por marcha

Existe un error independiente para:

- R
- N
- 1ª
- 2ª

Los errores se acumulan.

Un error solo se borra cuando:

1. posteriormente se alcanza correctamente esa misma marcha; o
2. se reinicia el Arduino.

Un fallo normal de posicionamiento no bloquea necesariamente el selector.

## Confirmación de marcha

Al confirmar correctamente una marcha:

- pasa a ser `marchaActual`;
- se borra su error almacenado;
- su LED queda fijo;
- se elimina la indicación temporal de maniobra correspondiente.

Los errores de las demás marchas permanecen.

## Cancelación y cambio de objetivo

Durante una maniobra puede solicitarse un nuevo objetivo permitido.

Ejemplo: 1ª → 2ª y se solicita el retorno antes de alcanzar 2ª.

El sistema:

1. cancela el objetivo anterior;
2. establece el nuevo objetivo;
3. comprueba inmediatamente si ya está dentro de tolerancia;
4. si no lo está, se mueve hacia la posición memorizada del nuevo objetivo.

Los LEDs conservan información de la maniobra y de errores previos.

## UP + DOWN

La pulsación simultánea de UP y DOWN:

- cancela la maniobra activa;
- establece N como nuevo objetivo.

No borra errores.

## Lógica de LEDs

### LED fijo

La marcha está correctamente alcanzada y confirmada.

### LED parpadeando durante una maniobra

La marcha es un objetivo activo o una posición implicada en una transición/cancelación.

### LED parpadeando al finalizar

Existe un error pendiente en esa marcha.

Pueden existir varios LEDs parpadeando simultáneamente.

Ejemplo:

- 1ª confirmada: LED 1 fijo.
- Error previo de 2ª: LED 2 parpadeando.

Por tanto, un LED fijo puede indicar la posición confirmada mientras otros LEDs muestran errores acumulados.

## Ejemplo de cancelación visual

Situación:

- 1ª confirmada → LED 1 fijo.
- Se solicita 2ª → LED 2 parpadea.
- Se cancela y el objetivo vuelve a 1ª.

Durante la transición pueden parpadear los LEDs implicados. Al finalizar:

- si 1ª se confirma, LED 1 queda fijo;
- el LED de 2ª se apaga si no tenía un error previo;
- si 2ª ya tenía un error almacenado, continúa parpadeando;
- si 1ª no se confirma, LED 1 queda parpadeando como error pendiente.

## FC_C

`FC_C` confirma condiciones necesarias relacionadas con el cambio de carril.

Si una maniobra requiere su confirmación para continuar y no la obtiene, se considera un error grave porque no se puede garantizar el uso correcto de la caja.

## FC_S

`FC_S` confirma el carril correspondiente.

Para los movimientos longitudinales que requieren el carril principal, la confirmación debe existir **antes de iniciar el movimiento longitudinal**. No es aceptable mover primero y comprobar después que el carril era correcto.

Esto protege frente a desplazamientos hacia posiciones incoherentes causados por un fallo mecánico o movimiento involuntario del carril.

## N → R

Secuencia mecánica definida:

1. K1 ON.
2. Esperar `FC_C`.
3. Mover hacia N.
4. Activar K2 en el momento definido por la secuencia.
5. Esperar la condición correspondiente de `FC_S`.
6. Continuar hacia R.
7. Confirmar la posición memorizada de R.
8. K2 OFF y K1 OFF.

El cambio mecánico de carril puede producirse antes de alcanzar exactamente la posición de N debido al sistema mecánico con muelle.

## R → N

No es necesario exigir inicialmente que `FC_S` indique ya el carril principal.

R fue confirmada durante la maniobra anterior. El movimiento puede dirigirse longitudinalmente hacia N y la condición relevante es que al alcanzar N se confirme el estado correcto de `FC_S`.

Si N no se confirma:

- se detiene la maniobra;
- se registra error en N;
- no se vuelve automáticamente a N porque esa recuperación automática ya no forma parte de la lógica normal de V8.

## Errores graves

Se mantienen como errores graves las condiciones que impiden garantizar una operación segura, incluyendo:

- incoherencia peligrosa o fallo crítico de finales de carrera;
- fallo grave del potenciómetro sin pista válida;
- fallo de hardware que impida garantizar la posición;
- activación imposible de K1/K2;
- ausencia de una confirmación imprescindible de `FC_C`;
- cualquier condición equivalente que impida conocer de forma segura posición o carril.

Ante un error grave:

- se detiene el movimiento;
- se desactivan los accionamientos necesarios;
- se entra en `ERROR_GRAVE`;
- parpadean los cuatro LEDs;
- se requiere reiniciar el Arduino.

## Botones

Los botones usan `INPUT_PULLUP`.

La pulsación se detecta en el flanco:

**HIGH → LOW**

Por tanto, la orden se detecta al pulsar el botón, no al soltarlo.

Se mantienen las protecciones de antirrebote y bloqueo temporal implementadas en el código.

## Potenciómetro doble

La V8 conserva el sistema de doble pista:

- lectura de ambas pistas;
- validación individual;
- detección de fallos;
- selección de pista activa;
- lectura efectiva;
- posiciones memorizadas en EEPROM.

Si ninguna pista válida permite garantizar la posición, el sistema puede entrar en error grave.

`posEfectiva(Marcha)` proporciona la posición memorizada de la marcha utilizando la pista efectiva/segura disponible.

## Puerto serie y diagnóstico

La comunicación serie se mantiene a 9600 baudios.

El comando:

`D`

continúa proporcionando el diagnóstico del sistema y del potenciómetro.

Con `DEBUG` activo también se mantienen los mensajes de depuración definidos por el código.

## Modo de aprendizaje

Se conserva el modo de aprendizaje y el almacenamiento en EEPROM de las posiciones:

- R
- N
- 1ª
- 2ª

Estas posiciones se utilizan posteriormente como objetivos del control por potenciómetro.

## Diferencia fundamental respecto a versiones anteriores

La diferencia arquitectónica principal de la V8 es:

> Un fallo al alcanzar una marcha ya no provoca, como respuesta normal, una maniobra automática de recuperación hacia N.

La V8 prioriza:

1. detener;
2. informar;
3. conservar el error;
4. permitir una nueva orden cuando la seguridad lo permita.

La recuperación automática podrá reconsiderarse en una versión futura después de validar el comportamiento mecánico real.
