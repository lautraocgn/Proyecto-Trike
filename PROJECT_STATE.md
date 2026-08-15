# PROJECT_STATE — Proyecto Trike

**Versión de referencia:** v7.2  
**Código principal:** `Codigo_trike_v7.2.ino`  
**Rama:** `main`

## Regla de fuente de verdad

El código actual de GitHub es la fuente principal para el estado del software. Si `PROJECT_STATE.md` contradice al código, prevalece el código.

Este archivo contiene solo información verificable del Proyecto Trike. No incorporar información de otros proyectos ni datos no confirmados.

## Nota operativa para futuras sesiones

- El nombre del archivo del firmware es la referencia de versión. La versión escrita dentro de la cabecera del `.ino` puede estar desactualizada y no debe utilizarse para determinar la versión vigente.
- Antes de analizar o modificar el firmware, consultar siempre `PROJECT_STATE.md` y después leer el archivo `.ino` actual de GitHub en la rama `main`.
- Un `.ino` antiguo subido a una conversación no sustituye nunca al código de GitHub y no debe utilizarse si GitHub está disponible.
- Si no se puede acceder al código actual de GitHub, detenerse y comunicarlo; no sustituirlo por una versión antigua disponible en la conversación.
- Los archivos de otros proyectos, aunque estén disponibles en la conversación o tengan hardware/nombres similares, quedan fuera del Proyecto Trike salvo asociación explícita.
- No escribir cambios en GitHub sin autorización explícita del usuario.

## Estado actual

- Firmware de referencia: v7.2.
- El código está actualmente en un único archivo `.ino`.
- El firmware utiliza una máquina de estados.
- El sistema utiliza dos pistas de potenciómetro para redundancia de posición.
- El código contiene gestión de aprendizaje/calibración, selección de marchas, relés, finales de carrera, LEDs y diagnóstico mediante puerto serie.

## Microcontrolador

- Arduino Nano / ATmega328P.

## Pinout verificado en v7.2

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

Enumeración actual:

- `MARCHA_R`
- `MARCHA_N`
- `MARCHA_1`
- `MARCHA_2`

## Máquina de estados

Estados definidos en el código v7.2:

- `ARRANQUE`
- `REPOSO`
- `ESPERANDO_FC_C`
- `MOVIENDO`
- `ESPERA_FC_S_RETORNO`
- `EMERGENCIA_A_N`
- `RECUPERANDO_A_N`
- `ERROR_LEVE`
- `ERROR_GRAVE`
- `MODO_APRENDIZAJE`
- `APRENDIZAJE_MOVIENDO`
- `APRENDIZAJE_CONFIRMANDO`

## Potenciómetros

- A0 = pista A.
- A3 = pista B.
- Se utilizan ambas pistas para supervisión redundante de posición.
- El código incorpora filtrado, validación, detección de saltos erráticos y detección de pista congelada.
- Las posiciones de las marchas se almacenan en EEPROM para ambas pistas.

## Parámetros relevantes verificados

- `TIMEOUT_MS = 3000`.
- `DEBOUNCE_MS = 20`.
- `BLOQUEO_MS = 500`.
- `TIEMPO_PULSACION_LARGA_MS = 600`.
- `RANGO_MIN_ADC = 10`.
- `RANGO_MAX_ADC = 1013`.
- `NUM_MUESTRAS_PROMEDIO = 5`.
- `LECTURAS_CONGELADO = 8`.

## Diagnóstico e indicadores

El código dispone de funciones específicas para:

- indicar la marcha mediante LEDs;
- gestionar parpadeos de error y aprendizaje;
- indicar problemas relacionados con el potenciómetro;
- registrar estados, relés, finales de carrera y posiciones mediante puerto serie;
- entrar en `ERROR_GRAVE` ante condiciones graves.

## Calibración / aprendizaje

Existe un modo de aprendizaje controlado mediante los botones `MODO`, `UP`, `DOWN` y `CONF`, con almacenamiento de posiciones para R, N, 1 y 2.

## Versionado de trabajo

- Los cambios y correcciones dentro de una funcionalidad existente se registran mediante commits y mantienen la versión funcional vigente.
- Una nueva versión mayor se utilizará cuando se incorpore una funcionalidad nueva o un cambio importante de arquitectura.
- Los números de versión del archivo y de la cabecera del código deben mantenerse sincronizados en futuras versiones.

## Pendientes

- Revisar y confirmar el estado funcional completo del v7.2 mediante pruebas reales.
- Mantener este archivo actualizado después de cambios importantes de arquitectura, comportamiento, pinout o requisitos.

## Regla de modificación

Antes de modificar el firmware: leer el código actual de GitHub, localizar la función afectada, comprobar sus dependencias y verificar que el cambio no contradice el comportamiento existente. No utilizar memoria de otros proyectos para completar datos que no estén confirmados aquí o en el código actual.
