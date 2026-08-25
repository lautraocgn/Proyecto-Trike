// =============================================================================
// SELECTOR DE MARCHAS VW AUTOSTICK — V8
// Base: commit 338101bf... / v7.3.6.2
// =============================================================================

#include <EEPROM.h>

// =============================================================================
// CONSTANTES
// =============================================================================

const uint16_t DEBOUNCE_MS                  = 20;
const uint16_t VENTANA_DOBLE_PULSACION_MS  = 120;
//const uint16_t BLOQUEO_MS                   = 500;
const uint16_t TIEMPO_PULSACION_LARGA_MS   = 600;
const uint16_t TIMEOUT_MS                   = 3000;
const uint16_t TIEMPO_MUERTO_INVERSION_MS  = 150;
const uint16_t TIEMPO_MAX_K2               = 3000;
const uint16_t TIEMPO_LECTURA_POT          = 20;
const uint16_t TOLERANCIA_ADC              = 25;
const uint16_t PASO_APRENDIZAJE_MS         = 100;

const uint16_t RANGO_MIN_ADC               = 10;
const uint16_t RANGO_MAX_ADC               = 1013;
const uint16_t TIEMPO_VERIFICACION_POT     = 150;
const uint8_t  NUM_MUESTRAS_PROMEDIO       = 5;
const uint16_t UMBRAL_SALTO_ERRATICO       = 120;
const uint16_t UMBRAL_CONGELADO            = 3;
const uint8_t  LECTURAS_CONGELADO          = 8;
const uint8_t  LECTURAS_CONFIRMACION_FALLO = 3;
const uint8_t  LECTURAS_CONFIRMACION_RANGO = 2;
const uint8_t  LECTURAS_CONFIRMACION_DISCREPANCIA = 3;
const uint16_t VALOR_CENTINELA             = 0xFFFF;

const uint16_t PARPADEO_ERROR_ON           = 250;
const uint16_t PARPADEO_ERROR_OFF          = 250;
const uint16_t PARPADEO_APREND_ON          = 150;
const uint16_t PARPADEO_APREND_OFF         = 150;
const uint16_t PARPADEO_APREND_PAUSA       = 600;
const uint16_t PARPADEO_CONFIRM_MS         = 500;
const uint16_t PARPADEO_AVISO_EEPROM_MS    = 5000;
const uint16_t PARPADEO_POT_ALERT_ON       = 500;
const uint16_t PARPADEO_POT_ALERT_OFF      = 500;

const uint8_t EEPROM_FIRMA_ADDR            = 0;
const uint8_t EEPROM_FIRMA_VAL             = 0xA5;
const uint8_t EEPROM_POS_A_BASE            = 2;
const uint8_t EEPROM_POS_B_BASE            = 10;

const uint16_t DEFAULT_POS_A_R             = 334;
const uint16_t DEFAULT_POS_A_N             = 703;
const uint16_t DEFAULT_POS_A_1             = 461;
const uint16_t DEFAULT_POS_A_2             = 874;

const uint16_t DEFAULT_POS_B_R             = 338;
const uint16_t DEFAULT_POS_B_N             = 709;
const uint16_t DEFAULT_POS_B_1             = 468;
const uint16_t DEFAULT_POS_B_2             = 878;

#define DEBUG 1

#if DEBUG
  #define DBG(x)        Serial.print(x)
  #define DBGLN(x)      Serial.println(x)
  #define DBG_VAL(x)    Serial.print(x)
  #define DBGLN_VAL(x)  Serial.println(x)
#else
  #define DBG(x)
  #define DBGLN(x)
  #define DBG_VAL(x)
  #define DBGLN_VAL(x)
#endif

// =============================================================================
// PINOUT
// =============================================================================

const uint8_t PIN_BTN_UP        = 3;
const uint8_t PIN_BTN_DOWN      = 2;
const uint8_t PIN_REL_IN        = 4;
const uint8_t PIN_REL_OUT       = 5;
const uint8_t PIN_K2            = 6;
const uint8_t PIN_LED_R         = 7;
const uint8_t PIN_LED_N         = 8;
const uint8_t PIN_LED_1         = 9;
const uint8_t PIN_LED_2         = 10;
const uint8_t PIN_BTN_MODO      = 11;
const uint8_t PIN_BTN_CONF      = 12;
const uint8_t PIN_K1            = 13;
const uint8_t PIN_POT_A         = A0;
const uint8_t PIN_POT_B         = A3;
const uint8_t PIN_FC_S          = A1;
const uint8_t PIN_FC_C          = A2;
const uint8_t PIN_LED_POT_ALERT = A4;

// =============================================================================
// ENUMERACIONES
// =============================================================================

enum Marcha {
  MARCHA_R = 0,
  MARCHA_N = 1,
  MARCHA_1 = 2,
  MARCHA_2 = 3
};

enum PotFallo {
  POT_FALLO_NINGUNO,
  POT_FALLO_FUERA_RANGO,
  POT_FALLO_SALTO_ERRATICO,
  POT_FALLO_CONGELADA,
  POT_FALLO_DIRECCION,
  POT_FALLO_DISCREPANCIA,
  POT_FALLO_EEPROM
};

enum Estado {
  ARRANQUE,
  REPOSO,
  ESPERANDO_FC_C,
  ESPERANDO_FC_S_PRINCIPAL,
  MOVIENDO,
  ESPERANDO_FC_S_CAMBIO_CARRIL,
  ESPERA_FC_S_RETORNO,
  ERROR_GRAVE,

  MODO_APRENDIZAJE,
  APRENDIZAJE_IR_A_2,
  APRENDIZAJE_MOVIENDO,
  APRENDIZAJE_ESPERANDO_FC_C,
  APRENDIZAJE_ESPERANDO_FC_S,
  APRENDIZAJE_RECUPERANDO,
  APRENDIZAJE_CONFIRMANDO
};

// =============================================================================
// ESTRUCTURAS
// =============================================================================

struct Boton {
  uint8_t pin;
  bool estadoAnterior;
  bool estadoActual;
  bool presionado;
  bool soltado;
  uint32_t tiempoPresion;
  uint32_t ultimoCambio;
  bool bloqueado;
};

struct PotenciometroDoble {
  uint16_t lecturaA;
  uint16_t lecturaB;
  uint16_t lecturaEfectiva;
  bool pistaAFallada;
  bool pistaBFallada;
  uint8_t pistaActiva;
  uint32_t ultimoCambioPista;
  uint32_t ultimaVerificacion;
  uint8_t contadorCongeladoA;
  uint8_t contadorCongeladoB;
  uint8_t contadorFalloA;
  uint8_t contadorFalloB;
  uint16_t lecturaAnteriorA;
  uint16_t lecturaAnteriorB;
  PotFallo motivoFalloA;
  PotFallo motivoFalloB;
  bool primeraLectura;
};

// =============================================================================
// VARIABLES GLOBALES
// =============================================================================

Estado estadoActual = ARRANQUE;

Marcha marchaActual  = MARCHA_N;
Marcha marchaDestino = MARCHA_N;
Marcha marchaOrigen  = MARCHA_N;

bool errorMarcha[4] = {false, false, false, false};
Marcha marchaCancelada = MARCHA_N;
bool cancelacionVisualActiva = false;

uint16_t posADC_A[4];
uint16_t posADC_B[4];

uint32_t tiempoInicio = 0;
uint32_t tiempoAux = 0;

bool relInActivo = false;
bool relOutActivo = false;
uint32_t tiempoInicioK2 = 0;
uint32_t tiempoApagadoRele = 0;
bool esperandoRetardoRele = false;

bool cambioCarrilPendiente = false;
bool fcSConfirmado = false;
bool movimientoEsR = false;
bool movimientoEsN = false;

uint16_t lecturaInicioMovimiento = 0;

Boton btnUp   = {PIN_BTN_UP, true, true, false, false, 0, 0, false};
Boton btnDown = {PIN_BTN_DOWN, true, true, false, false, 0, 0, false};
Boton btnModo = {PIN_BTN_MODO, true, true, false, false, 0, 0, false};
Boton btnConf = {PIN_BTN_CONF, true, true, false, false, 0, 0, false};

bool ordenUpPendiente = false;
bool ordenDownPendiente = false;
uint32_t tiempoOrdenPendiente = 0;

uint32_t tiempoLed = 0;
bool ledEstado = false;
uint8_t faseParpadeo = 0;

bool timeoutK2 = false;

bool avisandoEEPROM = false;
uint32_t tiempoAvisoEEPROM = 0;

PotenciometroDoble potDoble = {
  0, 0, 0,
  false, false,
  0, 0, 0,
  0, 0,
  0, 0,
  0, 0,
  POT_FALLO_NINGUNO,
  POT_FALLO_NINGUNO,
  true
};

bool potAlertState = false;
uint32_t tiempoPotAlert = 0;
bool ambasPistasFalladas = false;
uint8_t contadorDiscrepanciaPistas = 0;

const uint8_t PISTA_NINGUNA = 255;
uint8_t pistaEnRehabilitacion = PISTA_NINGUNA;
uint8_t validacionesRehabilitacion[4] = {0, 0, 0, 0};
Marcha posicionRehabilitacion = MARCHA_N;
bool salioDeUltimaPosicionValidada = true;
bool estabilidadRehabilitacionActiva = false;
uint32_t inicioEstabilidadRehabilitacion = 0;
uint16_t ultimaLecturaEstableRehabilitacion = 0;

// =============================================================================
// VARIABLES DEL MODO APRENDIZAJE
// =============================================================================

Marcha marchaAprendizaje = MARCHA_N;
Marcha marchaAprendizajeOrigen = MARCHA_N;
Marcha marchaAprendizajeDestino = MARCHA_N;
bool posicionAprendidaSesion[4] = {false, false, false, false};
bool aprendizajeMovimientoAutomatico = false;
bool errorAprendizajeActivo = false;
Marcha marchaErrorAprendizaje = MARCHA_N;
bool ledErrorAprendizajeEstado = false;
uint32_t tiempoLedErrorAprendizaje = 0;

// =============================================================================
// PROTOTIPOS
// =============================================================================

void leerBotones();
void actualizarBoton(Boton &btn);
bool upDownSimultaneos();
bool hayNuevaOrden();
void limpiarOrdenesPendientes();
bool obtenerNuevaOrden(Marcha &destino);
Marcha siguienteMarcha(Marcha desde, bool up);
void cancelarYRedirigir(Marcha nuevoDestino);

void activarReleIn();
void activarReleOut();
void apagarActuador();
void gestionarRetardoRele();
void activarK1();
void desactivarK1();
void activarK2();
void desactivarK2();
void vigilarK2();
void apagarTodoReles();

void leerPotenciometro();
uint16_t promediarLecturas(uint8_t pin);
bool validarLectura(uint16_t val);
const __FlashStringHelper* nombreFalloPot(PotFallo fallo);
void registrarFalloPista(uint8_t pista, PotFallo motivo);
void iniciarRehabilitacionPista(uint8_t pista);
void reiniciarRehabilitacionPista();
void comprobarRehabilitacionPista();
bool posicionConfirmadaPorPistaSana(Marcha marcha);
void rehabilitarPista();
uint16_t umbralDiferenciaPistas(Marcha m);
bool verificarPistas();
uint16_t posEfectiva(Marcha m);
uint16_t obtenerLecturaSegura();
uint16_t leerPot();

bool enPosicion(uint16_t pos);
bool fcSCarrilPrincipal();
bool fcSCarrilR();
void iniciarMovimientoPosicion(Marcha destino);
void confirmarMarcha(Marcha marcha);
void registrarErrorMarcha(Marcha marcha);

void gestionarLEDsNormal();
uint8_t pinLED(Marcha m);
void apagarTodosLEDs();
void manejarParpadeoAprendizaje(uint8_t pin);
void manejarParpadeoGrave();
void manejarParpadeoPotAlert();
void manejarIndicadoresAprendizaje();

void printResetCause();
void printDiagnosticoInicial();
void imprimirDiagnosticoSistema();
void logCambioMarcha(Marcha origen, Marcha destino);
void logRele(const __FlashStringHelper* nombre, bool activado);
void logFCCambio(const __FlashStringHelper* mensaje);
void logPosicionAlcanzada(Marcha m);
void logAccion(const __FlashStringHelper* accion);
const __FlashStringHelper* nombreMarcha(Marcha m);
const __FlashStringHelper* nombreEstado(Estado estado);
void procesarComandoSerial();

void estadoArranque();
void estadoReposo();
void estadoEsperandoFCC();
void estadoEsperandoFCSSPrincipal();
void estadoMoviendo();
void estadoEsperandoFCSCambioCarril();
void estadoEsperaFCSRetorno();
void estadoErrorGrave();

void iniciarCambioA(Marcha destino);
void entrarErrorGrave();

// Aprendizaje
void estadoModoAprendizaje();
void estadoAprendizajeIrA2();
void estadoAprendizajeMoviendo();
void estadoAprendizajeEsperandoFCC();
void estadoAprendizajeEsperandoFCS();
void estadoAprendizajeRecuperando();
void estadoAprendizajeConfirmando();
void iniciarSecuenciaAprendizajeNAR();
void iniciarMovimientoAprendizaje(Marcha destino);
void iniciarRecuperacionAprendizaje();
void continuarAprendizajeTrasGuardar();
uint16_t posicionObjetivoAprendizaje();
void guardarPosEnEEPROM(Marcha m, uint16_t valorA, uint16_t valorB);
void cargarPosiciones();

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println(F("TEST SERIAL OK"));

  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_MODO, INPUT_PULLUP);
  pinMode(PIN_BTN_CONF, INPUT_PULLUP);
  pinMode(PIN_FC_S, INPUT_PULLUP);
  pinMode(PIN_FC_C, INPUT_PULLUP);

  pinMode(PIN_REL_IN, OUTPUT); digitalWrite(PIN_REL_IN, LOW);
  pinMode(PIN_REL_OUT, OUTPUT); digitalWrite(PIN_REL_OUT, LOW);
  pinMode(PIN_K1, OUTPUT); digitalWrite(PIN_K1, LOW);
  pinMode(PIN_K2, OUTPUT); digitalWrite(PIN_K2, LOW);

  pinMode(PIN_LED_R, OUTPUT); digitalWrite(PIN_LED_R, LOW);
  pinMode(PIN_LED_N, OUTPUT); digitalWrite(PIN_LED_N, LOW);
  pinMode(PIN_LED_1, OUTPUT); digitalWrite(PIN_LED_1, LOW);
  pinMode(PIN_LED_2, OUTPUT); digitalWrite(PIN_LED_2, LOW);
  pinMode(PIN_LED_POT_ALERT, OUTPUT); digitalWrite(PIN_LED_POT_ALERT, LOW);

  cargarPosiciones();
  leerPotenciometro();
  potDoble.lecturaEfectiva = obtenerLecturaSegura();
  printDiagnosticoInicial();

  tiempoInicio = millis();
  estadoActual = ARRANQUE;
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
  uint32_t ahora = millis();

  leerBotones();
  gestionarRetardoRele();

  if ((ahora - potDoble.ultimaVerificacion) >= TIEMPO_VERIFICACION_POT) {
    potDoble.ultimaVerificacion = ahora;
    leerPotenciometro();
    verificarPistas();
    potDoble.lecturaEfectiva = obtenerLecturaSegura();
  }

  manejarParpadeoPotAlert();
  procesarComandoSerial();

  bool modoAprendizajeActivo = (digitalRead(PIN_BTN_MODO) == LOW);

  if (modoAprendizajeActivo &&
      estadoActual != MODO_APRENDIZAJE &&
      estadoActual != APRENDIZAJE_IR_A_2 &&
      estadoActual != APRENDIZAJE_MOVIENDO &&
      estadoActual != APRENDIZAJE_ESPERANDO_FC_C &&
      estadoActual != APRENDIZAJE_ESPERANDO_FC_S &&
      estadoActual != APRENDIZAJE_RECUPERANDO &&
      estadoActual != APRENDIZAJE_CONFIRMANDO &&
      estadoActual != ERROR_GRAVE) {

    apagarTodoReles();

    for (uint8_t i = 0; i < 4; i++) {
      posicionAprendidaSesion[i] = false;
    }

    marchaAprendizaje = MARCHA_2;
    marchaAprendizajeOrigen = MARCHA_2;
    marchaAprendizajeDestino = MARCHA_2;
    aprendizajeMovimientoAutomatico = false;
    errorAprendizajeActivo = false;
    marchaErrorAprendizaje = MARCHA_N;
    faseParpadeo = 0;
    ledErrorAprendizajeEstado = false;
    tiempoLedErrorAprendizaje = ahora;
    timeoutK2 = false;
    tiempoInicio = ahora;
    estadoActual = APRENDIZAJE_IR_A_2;
    return;
  }

  if (!modoAprendizajeActivo &&
      (estadoActual == MODO_APRENDIZAJE ||
       estadoActual == APRENDIZAJE_IR_A_2 ||
       estadoActual == APRENDIZAJE_MOVIENDO ||
       estadoActual == APRENDIZAJE_ESPERANDO_FC_C ||
       estadoActual == APRENDIZAJE_ESPERANDO_FC_S ||
       estadoActual == APRENDIZAJE_RECUPERANDO ||
       estadoActual == APRENDIZAJE_CONFIRMANDO)) {

    apagarTodoReles();
    timeoutK2 = false;
    estadoActual = ARRANQUE;
    tiempoInicio = ahora;
    return;
  }

  vigilarK2();

  switch (estadoActual) {
    case ARRANQUE:                         estadoArranque(); break;
    case REPOSO:                           estadoReposo(); break;
    case ESPERANDO_FC_C:                   estadoEsperandoFCC(); break;
    case ESPERANDO_FC_S_PRINCIPAL:         estadoEsperandoFCSSPrincipal(); break;
    case MOVIENDO:                         estadoMoviendo(); break;
    case ESPERANDO_FC_S_CAMBIO_CARRIL:    estadoEsperandoFCSCambioCarril(); break;
    case ESPERA_FC_S_RETORNO:              estadoEsperaFCSRetorno(); break;
    case ERROR_GRAVE:                      estadoErrorGrave(); break;

    case MODO_APRENDIZAJE:                 estadoModoAprendizaje(); break;
    case APRENDIZAJE_IR_A_2:               estadoAprendizajeIrA2(); break;
    case APRENDIZAJE_MOVIENDO:             estadoAprendizajeMoviendo(); break;
    case APRENDIZAJE_ESPERANDO_FC_C:       estadoAprendizajeEsperandoFCC(); break;
    case APRENDIZAJE_ESPERANDO_FC_S:       estadoAprendizajeEsperandoFCS(); break;
    case APRENDIZAJE_RECUPERANDO:          estadoAprendizajeRecuperando(); break;
    case APRENDIZAJE_CONFIRMANDO:          estadoAprendizajeConfirmando(); break;
  }

  if (estadoActual != MODO_APRENDIZAJE &&
      estadoActual != APRENDIZAJE_IR_A_2 &&
      estadoActual != APRENDIZAJE_MOVIENDO &&
      estadoActual != APRENDIZAJE_ESPERANDO_FC_C &&
      estadoActual != APRENDIZAJE_ESPERANDO_FC_S &&
      estadoActual != APRENDIZAJE_RECUPERANDO &&
      estadoActual != APRENDIZAJE_CONFIRMANDO) {
    gestionarLEDsNormal();
  }
}

// =============================================================================
// BOTONES
// =============================================================================

void leerBotones() {
  actualizarBoton(btnUp);
  actualizarBoton(btnDown);
  actualizarBoton(btnModo);
  actualizarBoton(btnConf);
}

void actualizarBoton(Boton &btn) {
  uint32_t ahora = millis();
  bool lectura = (digitalRead(btn.pin) == LOW);

  btn.presionado = false;
  btn.soltado = false;

  // Se detecta inmediatamente cualquier cambio físico y se reinicia
  // el tiempo de estabilidad.
  if (lectura != btn.estadoAnterior) {
    btn.estadoAnterior = lectura;
    btn.ultimoCambio = ahora;
  }

  // Solo se acepta como nuevo estado si la lectura se ha mantenido
  // estable durante todo el tiempo de antirrebote.
  if ((ahora - btn.ultimoCambio) >= DEBOUNCE_MS &&
      lectura != btn.estadoActual) {

    btn.estadoActual = lectura;

    if (btn.estadoActual) {
      btn.presionado = true;
      btn.tiempoPresion = ahora;
      btn.bloqueado = false;
    } else {
      btn.soltado = true;
    }
  }
}

bool upDownSimultaneos() {
  return btnUp.estadoActual && btnDown.estadoActual;
}

bool hayNuevaOrden() {
  return btnUp.presionado || btnDown.presionado || upDownSimultaneos();
}

void limpiarOrdenesPendientes() {
  ordenUpPendiente = false;
  ordenDownPendiente = false;
  tiempoOrdenPendiente = 0;
}

Marcha siguienteMarcha(Marcha desde, bool up) {
  if (up) {
    switch (desde) {
      case MARCHA_R: return MARCHA_N;
      case MARCHA_N: return MARCHA_1;
      case MARCHA_1: return MARCHA_2;
      case MARCHA_2: return MARCHA_2;
    }
  } else {
    switch (desde) {
      case MARCHA_R: return MARCHA_R;
      case MARCHA_N: return MARCHA_R;
      case MARCHA_1: return MARCHA_N;
      case MARCHA_2: return MARCHA_1;
    }
  }
  return desde;
}

bool obtenerNuevaOrden(Marcha &destino) {
  if (upDownSimultaneos()) {
    limpiarOrdenesPendientes();
    destino = MARCHA_N;
    return true;
  }

  if (btnUp.presionado) {
    ordenUpPendiente = true;
    ordenDownPendiente = false;
    tiempoOrdenPendiente = millis();
  }

  if (btnDown.presionado) {
    ordenDownPendiente = true;
    ordenUpPendiente = false;
    tiempoOrdenPendiente = millis();
  }

  if (!(ordenUpPendiente || ordenDownPendiente)) {
    return false;
  }

  if (upDownSimultaneos()) {
    limpiarOrdenesPendientes();
    destino = MARCHA_N;
    return true;
  }

  if ((millis() - tiempoOrdenPendiente) < VENTANA_DOBLE_PULSACION_MS) {
    return false;
  }

  bool up = ordenUpPendiente;
  bool down = ordenDownPendiente;
  limpiarOrdenesPendientes();

  if (up && !down) {
    Marcha siguiente = siguienteMarcha(marchaDestino, true);
    if (siguiente == marchaDestino) return false;
    destino = siguiente;
    return true;
  }

  if (down && !up) {
    Marcha siguiente = siguienteMarcha(marchaDestino, false);
    if (siguiente == marchaDestino) return false;
    destino = siguiente;
    return true;
  }

  return false;
}

// =============================================================================
// RELÉS
// =============================================================================

void activarReleIn() {
  if (relInActivo) {
    return;
  }

  if (relOutActivo) {
    digitalWrite(PIN_REL_OUT, LOW);
    relOutActivo = false;
    tiempoApagadoRele = millis();
    esperandoRetardoRele = true;
    logRele(F("OUT"), false);
    return;
  }

  if (esperandoRetardoRele) {
    return;
  }

  digitalWrite(PIN_REL_IN, HIGH);
  relInActivo = true;
  logRele(F("IN"), true);
}

void activarReleOut() {
  if (relOutActivo) {
    return;
  }

  if (relInActivo) {
    digitalWrite(PIN_REL_IN, LOW);
    relInActivo = false;
    tiempoApagadoRele = millis();
    esperandoRetardoRele = true;
    logRele(F("IN"), false);
    return;
  }

  if (esperandoRetardoRele) {
    return;
  }

  digitalWrite(PIN_REL_OUT, HIGH);
  relOutActivo = true;
  logRele(F("OUT"), true);
}

void apagarActuador() {
  bool inEstabaActivo = relInActivo;
  bool outEstabaActivo = relOutActivo;

  digitalWrite(PIN_REL_IN, LOW);
  digitalWrite(PIN_REL_OUT, LOW);

  relInActivo = false;
  relOutActivo = false;
  esperandoRetardoRele = false;

  if (inEstabaActivo) {
    logRele(F("IN"), false);
  }

  if (outEstabaActivo) {
    logRele(F("OUT"), false);
  }
}

void gestionarRetardoRele() {
  if (!esperandoRetardoRele) {
    return;
  }

  if ((millis() - tiempoApagadoRele) >= TIEMPO_MUERTO_INVERSION_MS) {
    esperandoRetardoRele = false;
  }
}

void activarK1() {
  if (digitalRead(PIN_K1) == HIGH) return;
  digitalWrite(PIN_K1, HIGH);
  logRele(F("K1"), true);
}

void desactivarK1() {
  if (digitalRead(PIN_K1) == LOW) return;
  digitalWrite(PIN_K1, LOW);
  logRele(F("K1"), false);
}

void activarK2() {
  if (digitalRead(PIN_K2) == HIGH) return;

  timeoutK2 = false;
  digitalWrite(PIN_K2, HIGH);
  tiempoInicioK2 = millis();
  logRele(F("K2"), true);
}

void desactivarK2() {
  if (digitalRead(PIN_K2) == LOW) {
    tiempoInicioK2 = 0;
    return;
  }

  digitalWrite(PIN_K2, LOW);
  tiempoInicioK2 = 0;
  logRele(F("K2"), false);
}

void vigilarK2() {
  if (digitalRead(PIN_K2) == LOW) {
    tiempoInicioK2 = 0;
    return;
  }

  if ((millis() - tiempoInicioK2) >= TIEMPO_MAX_K2) {
    desactivarK2();
    timeoutK2 = true;
    logAccion(F("TIMEOUT K2"));
  }
}

void apagarTodoReles() {
  apagarActuador();
  desactivarK1();
  desactivarK2();
}

// =============================================================================
// POTENCIÓMETRO
// =============================================================================

uint16_t promediarLecturas(uint8_t pin) {
  uint32_t suma = 0;
  for (uint8_t i = 0; i < NUM_MUESTRAS_PROMEDIO; i++) {
    suma += analogRead(pin);
    delayMicroseconds(50);
  }
  return (uint16_t)(suma / NUM_MUESTRAS_PROMEDIO);
}

void leerPotenciometro() {
  potDoble.lecturaA = promediarLecturas(PIN_POT_A);
  potDoble.lecturaB = promediarLecturas(PIN_POT_B);
}

bool validarLectura(uint16_t val) {
  return val >= RANGO_MIN_ADC && val <= RANGO_MAX_ADC;
}

const __FlashStringHelper* nombreFalloPot(PotFallo fallo) {
  switch (fallo) {
    case POT_FALLO_NINGUNO: return F("NINGUNO");
    case POT_FALLO_FUERA_RANGO: return F("FUERA_DE_RANGO");
    case POT_FALLO_SALTO_ERRATICO: return F("SALTO_ERRATICO");
    case POT_FALLO_CONGELADA: return F("CONGELADA");
    case POT_FALLO_DIRECCION: return F("DIRECCION_INCORRECTA");
    case POT_FALLO_DISCREPANCIA: return F("DISCREPANCIA");
    case POT_FALLO_EEPROM: return F("DESHABILITADA_EEPROM");
  }
  return F("DESCONOCIDO");
}

uint16_t umbralDiferenciaPistas(Marcha m) {
  uint16_t diffBase = (uint16_t)abs((int)posADC_A[m] - (int)posADC_B[m]);
  uint16_t umbral = (uint16_t)(((uint32_t)diffBase * 5UL) / 2UL) + 15U;
  if (umbral < 25) umbral = 25;
  return umbral;
}

uint16_t obtenerLecturaSegura() {
  if (potDoble.pistaActiva == 0 && !potDoble.pistaAFallada) return potDoble.lecturaA;
  if (potDoble.pistaActiva == 1 && !potDoble.pistaBFallada) return potDoble.lecturaB;

  if (!potDoble.pistaAFallada) {
    potDoble.pistaActiva = 0;
    return potDoble.lecturaA;
  }

  if (!potDoble.pistaBFallada) {
    potDoble.pistaActiva = 1;
    return potDoble.lecturaB;
  }

  return 0;
}

uint16_t posEfectiva(Marcha m) {
  if (m > MARCHA_2) return 0;

  if (potDoble.pistaActiva == 0 && !potDoble.pistaAFallada) {
    return posADC_A[m];
  }

  if (potDoble.pistaActiva == 1 && !potDoble.pistaBFallada) {
    return posADC_B[m];
  }

  if (!potDoble.pistaAFallada) return posADC_A[m];
  if (!potDoble.pistaBFallada) return posADC_B[m];

  return 0;
}

uint16_t leerPot() {
  return potDoble.lecturaEfectiva;
}

bool enPosicion(uint16_t pos) {
  int16_t diff = (int16_t)potDoble.lecturaEfectiva - (int16_t)pos;
  return diff >= -(int16_t)TOLERANCIA_ADC &&
         diff <=  (int16_t)TOLERANCIA_ADC;
}

void moverHacia(uint16_t pos) {
  if (ambasPistasFalladas || pos == 0) {
    apagarActuador();
    return;
  }

  if (enPosicion(pos)) {
    apagarActuador();
    return;
  }

  if (potDoble.lecturaEfectiva < pos) {
    activarReleOut();
  } else {
    activarReleIn();
  }
}

bool fcSCarrilPrincipal() {
  return digitalRead(PIN_FC_S) == HIGH;
}

bool fcSCarrilR() {
  return digitalRead(PIN_FC_S) == LOW;
}

void iniciarMovimientoPosicion(Marcha destino) {
  marchaDestino = destino;
  lecturaInicioMovimiento = potDoble.lecturaEfectiva;
  movimientoEsN = (destino == MARCHA_N);
  movimientoEsR = (destino == MARCHA_R);
  tiempoInicio = millis();
  estadoActual = MOVIENDO;
}

void confirmarMarcha(Marcha marcha) {
  apagarActuador();
  desactivarK2();
  desactivarK1();

  marchaActual = marcha;
  marchaDestino = marcha;
  errorMarcha[marcha] = false;

  cancelacionVisualActiva = false;
  movimientoEsN = false;
  movimientoEsR = false;
  cambioCarrilPendiente = false;
  fcSConfirmado = false;
  lecturaInicioMovimiento = 0;

  logPosicionAlcanzada(marcha);
}

void registrarErrorMarcha(Marcha marcha) {
  apagarActuador();
  desactivarK2();
  desactivarK1();

  errorMarcha[marcha] = true;
  cancelacionVisualActiva = false;
  movimientoEsN = false;
  movimientoEsR = false;
  cambioCarrilPendiente = false;
  fcSConfirmado = false;
  lecturaInicioMovimiento = 0;

  logAccion(F("ERROR MARCHA"));

  estadoActual = REPOSO;
}

// =============================================================================
// EEPROM
// =============================================================================

void cargarPosiciones() {
  bool eepromValida = EEPROM.read(EEPROM_FIRMA_ADDR) == EEPROM_FIRMA_VAL;

  if (eepromValida) {
    for (uint8_t i = 0; i < 4; i++) {
      EEPROM.get(EEPROM_POS_A_BASE + i * 2, posADC_A[i]);
      EEPROM.get(EEPROM_POS_B_BASE + i * 2, posADC_B[i]);
    }

    potDoble.pistaAFallada = false;
    potDoble.pistaBFallada = false;
    potDoble.motivoFalloA = POT_FALLO_NINGUNO;
    potDoble.motivoFalloB = POT_FALLO_NINGUNO;

    for (uint8_t i = 0; i < 4; i++) {
      if (posADC_A[i] == VALOR_CENTINELA) {
        potDoble.pistaAFallada = true;
        potDoble.motivoFalloA = POT_FALLO_EEPROM;
      }

      if (posADC_B[i] == VALOR_CENTINELA) {
        potDoble.pistaBFallada = true;
        potDoble.motivoFalloB = POT_FALLO_EEPROM;
      }
    }

    ambasPistasFalladas =
      potDoble.pistaAFallada && potDoble.pistaBFallada;

    if (!potDoble.pistaAFallada) {
      potDoble.pistaActiva = 0;
    } else if (!potDoble.pistaBFallada) {
      potDoble.pistaActiva = 1;
    } else {
      entrarErrorGrave();
      return;
    }

    avisandoEEPROM = false;
    return;
  }

  uint16_t defaultsA[4] = {
    DEFAULT_POS_A_R,
    DEFAULT_POS_A_N,
    DEFAULT_POS_A_1,
    DEFAULT_POS_A_2
  };

  uint16_t defaultsB[4] = {
    DEFAULT_POS_B_R,
    DEFAULT_POS_B_N,
    DEFAULT_POS_B_1,
    DEFAULT_POS_B_2
  };

  for (uint8_t i = 0; i < 4; i++) {
    posADC_A[i] = defaultsA[i];
    posADC_B[i] = defaultsB[i];

    EEPROM.put(EEPROM_POS_A_BASE + i * 2, posADC_A[i]);
    EEPROM.put(EEPROM_POS_B_BASE + i * 2, posADC_B[i]);
  }

  EEPROM.write(EEPROM_FIRMA_ADDR, EEPROM_FIRMA_VAL);
  avisandoEEPROM = true;
  tiempoAvisoEEPROM = millis();
}

void guardarPosEnEEPROM(Marcha m, uint16_t valorA, uint16_t valorB) {
  if (!validarLectura(valorA) || !validarLectura(valorB)) {
    logAccion(F("EEP SAVE RECHAZADO: ADC INVALIDO"));
    return;
  }

  posADC_A[m] = valorA;
  posADC_B[m] = valorB;

  EEPROM.put(EEPROM_POS_A_BASE + m * 2, valorA);
  EEPROM.put(EEPROM_POS_B_BASE + m * 2, valorB);
  EEPROM.write(EEPROM_FIRMA_ADDR, EEPROM_FIRMA_VAL);
}

// =============================================================================
// LEDS
// =============================================================================

uint8_t pinLED(Marcha m) {
  switch (m) {
    case MARCHA_R: return PIN_LED_R;
    case MARCHA_N: return PIN_LED_N;
    case MARCHA_1: return PIN_LED_1;
    case MARCHA_2: return PIN_LED_2;
  }
  return PIN_LED_N;
}

void apagarTodosLEDs() {
  digitalWrite(PIN_LED_R, LOW);
  digitalWrite(PIN_LED_N, LOW);
  digitalWrite(PIN_LED_1, LOW);
  digitalWrite(PIN_LED_2, LOW);
}

void manejarParpadeoGrave() {
  uint32_t ahora = millis();
  uint16_t intervalo = ledEstado
                     ? PARPADEO_ERROR_ON
                     : PARPADEO_ERROR_OFF;

  if ((ahora - tiempoLed) >= intervalo) {
    ledEstado = !ledEstado;

    uint8_t val = ledEstado ? HIGH : LOW;

    digitalWrite(PIN_LED_R, val);
    digitalWrite(PIN_LED_N, val);
    digitalWrite(PIN_LED_1, val);
    digitalWrite(PIN_LED_2, val);

    tiempoLed = ahora;
  }
}

void gestionarLEDsNormal() {
  if (estadoActual == ERROR_GRAVE) {
    manejarParpadeoGrave();
    return;
  }

  uint32_t ahora = millis();

  bool maniobraActiva =
      (estadoActual == ESPERANDO_FC_C ||
       estadoActual == ESPERANDO_FC_S_PRINCIPAL ||
       estadoActual == MOVIENDO ||
       estadoActual == ESPERANDO_FC_S_CAMBIO_CARRIL ||
       estadoActual == ESPERA_FC_S_RETORNO);

  if (maniobraActiva) {
    if ((ahora - tiempoLed) >= 250) {
      ledEstado = !ledEstado;
      tiempoLed = ahora;
    }
  } else {
    ledEstado = false;
  }

  for (uint8_t i = 0; i < 4; i++) {
    Marcha marcha = (Marcha)i;
    bool parpadea = false;

    if (errorMarcha[i]) {
      parpadea = true;
    }

    if (maniobraActiva && marchaDestino == marcha) {
      parpadea = true;
    }

    if (cancelacionVisualActiva && marchaCancelada == marcha) {
      parpadea = true;
    }

    if (parpadea) {
      digitalWrite(pinLED(marcha), ledEstado ? HIGH : LOW);
    } else if (marchaActual == marcha) {
      digitalWrite(pinLED(marcha), HIGH);
    } else {
      digitalWrite(pinLED(marcha), LOW);
    }
  }

  if (cancelacionVisualActiva && !maniobraActiva) {
    cancelacionVisualActiva = false;
  }
}

void manejarParpadeoAprendizaje(uint8_t pin) {
  uint32_t ahora = millis();
  bool encender;
  uint16_t intervalo;

  if (faseParpadeo < 6) {
    encender = (faseParpadeo % 2 == 0);
    intervalo = encender ? PARPADEO_APREND_ON : PARPADEO_APREND_OFF;
  } else {
    encender = false;
    intervalo = PARPADEO_APREND_PAUSA;
  }

  if ((ahora - tiempoLed) >= intervalo) {
    tiempoLed = ahora;
    faseParpadeo++;
    if (faseParpadeo > 6) faseParpadeo = 0;
    digitalWrite(pin, encender ? HIGH : LOW);
  }
}

void manejarParpadeoPotAlert() {
  uint32_t ahora = millis();

  if (potDoble.pistaAFallada && potDoble.pistaBFallada) {
    digitalWrite(PIN_LED_POT_ALERT, HIGH);
    potAlertState = true;
    return;
  }

  if (potDoble.pistaAFallada || potDoble.pistaBFallada) {
    uint16_t intervalo =
      potAlertState ? PARPADEO_POT_ALERT_ON : PARPADEO_POT_ALERT_OFF;

    if ((ahora - tiempoPotAlert) >= intervalo) {
      potAlertState = !potAlertState;
      digitalWrite(PIN_LED_POT_ALERT,
                   potAlertState ? HIGH : LOW);
      tiempoPotAlert = ahora;
    }

    return;
  }

  digitalWrite(PIN_LED_POT_ALERT, LOW);
  potAlertState = false;
}

// =============================================================================
// ESTADOS PRINCIPALES
// =============================================================================

void estadoArranque() {
  uint32_t ahora = millis();

  if (avisandoEEPROM &&
      (ahora - tiempoAvisoEEPROM) >= PARPADEO_AVISO_EEPROM_MS) {
    avisandoEEPROM = false;
    faseParpadeo = 0;
    tiempoLed = ahora;
  }

  if (enPosicion(posEfectiva(MARCHA_N))) {
    apagarActuador();

    if (fcSCarrilPrincipal()) {
      marchaActual = MARCHA_N;
      marchaDestino = MARCHA_N;
      cancelacionVisualActiva = false;
      errorMarcha[MARCHA_N] = false;
      estadoActual = REPOSO;
      logPosicionAlcanzada(MARCHA_N);
      return;
    }

    if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
      entrarErrorGrave();
      return;
    }

    return;
  }

  moverHacia(posEfectiva(MARCHA_N));

  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    entrarErrorGrave();
  }
}

void iniciarCambioA(Marcha destino) {
  marchaOrigen = marchaActual;
  marchaDestino = destino;

  lecturaInicioMovimiento = potDoble.lecturaEfectiva;
  tiempoInicio = millis();

  movimientoEsN = (destino == MARCHA_N);
  movimientoEsR = (destino == MARCHA_R);
  cambioCarrilPendiente = (destino == MARCHA_R);
  fcSConfirmado = false;

  activarK1();
  estadoActual = ESPERANDO_FC_C;

  logCambioMarcha(marchaOrigen, marchaDestino);
}

void cancelarYRedirigir(Marcha nuevoDestino) {
  if (nuevoDestino == marchaDestino) return;

  marchaCancelada = marchaDestino;
  cancelacionVisualActiva = true;

  apagarActuador();
  desactivarK2();

  marchaDestino = nuevoDestino;
  movimientoEsN = (nuevoDestino == MARCHA_N);
  movimientoEsR = (nuevoDestino == MARCHA_R);
  cambioCarrilPendiente = false;
  fcSConfirmado = false;
  lecturaInicioMovimiento = potDoble.lecturaEfectiva;
  tiempoInicio = millis();

  // No se considera error cancelar una maniobra.
  iniciarCambioA(nuevoDestino);
}

void estadoReposo() {
  Marcha nuevoDestino;

  if (!obtenerNuevaOrden(nuevoDestino)) {
    return;
  }

  if (nuevoDestino == marchaActual) {
    limpiarOrdenesPendientes();
    return;
  }

  iniciarCambioA(nuevoDestino);
}

void estadoEsperandoFCC() {
  uint32_t ahora = millis();

  if (digitalRead(PIN_FC_C) == LOW) {
    logFCCambio(F("FC_C OK"));
    tiempoInicio = ahora;

    if (marchaDestino == MARCHA_1 ||
        marchaDestino == MARCHA_2) {

      if (fcSCarrilPrincipal()) {
        iniciarMovimientoPosicion(marchaDestino);
      } else {
        tiempoInicio = ahora;
        estadoActual = ESPERANDO_FC_S_PRINCIPAL;
      }

      return;
    }

    if (marchaDestino == MARCHA_N) {
      iniciarMovimientoPosicion(MARCHA_N);
      return;
    }

    // N -> R: se mueve primero longitudinalmente a N.
    cambioCarrilPendiente = true;
    fcSConfirmado = false;
    iniciarMovimientoPosicion(MARCHA_N);
    return;
  }

  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    logAccion(F("ERROR GRAVE FC_C"));
    entrarErrorGrave();
  }
}

void estadoEsperandoFCSSPrincipal() {
  uint32_t ahora = millis();

  if (fcSCarrilPrincipal()) {
    logFCCambio(F("FC_S PRINCIPAL OK"));
    tiempoInicio = ahora;
    iniciarMovimientoPosicion(marchaDestino);
    return;
  }

  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    logAccion(F("ERROR GRAVE FC_S PRINCIPAL"));
    entrarErrorGrave();
    return;
  }

  Marcha nuevoDestino;
  if (obtenerNuevaOrden(nuevoDestino)) {
    cancelarYRedirigir(nuevoDestino);
  }
}

void estadoMoviendo() {
  uint32_t ahora = millis();

  if (timeoutK2) {
    timeoutK2 = false;
    if (marchaDestino == MARCHA_R) {
      entrarErrorGrave();
    } else {
      registrarErrorMarcha(marchaDestino);
    }
    return;
  }

  // Nueva orden mientras se está moviendo.
  Marcha nuevoDestino;
  if (obtenerNuevaOrden(nuevoDestino)) {
    // Durante N -> R, la única orden válida de cancelación es N.
    if (marchaDestino == MARCHA_R) {
      if (nuevoDestino == MARCHA_N) {
        cancelarYRedirigir(MARCHA_N);
      }
      return;
    }

    cancelarYRedirigir(nuevoDestino);
    return;
  }

  // ---------------------------------------------------------------------------
  // Destino N
  // ---------------------------------------------------------------------------

  if (marchaDestino == MARCHA_N) {

    // Para N el FC_S no condiciona el inicio del movimiento.
    if (enPosicion(posEfectiva(MARCHA_N))) {
      apagarActuador();
      lecturaInicioMovimiento = 0;
      tiempoAux = ahora;
      estadoActual = ESPERA_FC_S_RETORNO;
      return;
    }

    moverHacia(posEfectiva(MARCHA_N));

    if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
      registrarErrorMarcha(MARCHA_N);
    }

    return;
  }

  // ---------------------------------------------------------------------------
  // Destino R: primera fase N -> cambio de carril -> R
  // ---------------------------------------------------------------------------

  if (marchaDestino == MARCHA_R && cambioCarrilPendiente) {

    if (enPosicion(posEfectiva(MARCHA_N))) {
      apagarActuador();
      lecturaInicioMovimiento = 0;
      tiempoInicio = ahora;
      activarK2();
      estadoActual = ESPERANDO_FC_S_CAMBIO_CARRIL;
      return;
    }

    moverHacia(posEfectiva(MARCHA_N));

    if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
      registrarErrorMarcha(MARCHA_R);
    }

    return;
  }

  // ---------------------------------------------------------------------------
  // Destino R: desplazamiento longitudinal dentro del carril R
  // ---------------------------------------------------------------------------

  if (marchaDestino == MARCHA_R) {

    if (!fcSCarrilR()) {
      entrarErrorGrave();
      return;
    }

    if (enPosicion(posEfectiva(MARCHA_R))) {
      apagarActuador();
      confirmarMarcha(MARCHA_R);
      return;
    }

    moverHacia(posEfectiva(MARCHA_R));

    if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
      registrarErrorMarcha(MARCHA_R);
    }

    return;
  }

  // ---------------------------------------------------------------------------
  // Destino 1ª o 2ª
  // ---------------------------------------------------------------------------

  if (!fcSCarrilPrincipal()) {
    entrarErrorGrave();
    return;
  }

  if (enPosicion(posEfectiva(marchaDestino))) {
    apagarActuador();
    confirmarMarcha(marchaDestino);
    return;
  }

  moverHacia(posEfectiva(marchaDestino));

  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    registrarErrorMarcha(marchaDestino);
  }
}

void estadoEsperandoFCSCambioCarril() {
  uint32_t ahora = millis();

  // Durante este estado K2 está activo y todavía estamos físicamente en N.
  Marcha nuevoDestino;
  if (obtenerNuevaOrden(nuevoDestino)) {
    if (nuevoDestino == MARCHA_N) {
      marchaCancelada = marchaDestino;
      cancelacionVisualActiva = true;

      apagarActuador();
      desactivarK2();

      marchaDestino = MARCHA_N;
      movimientoEsN = true;
      movimientoEsR = false;
      cambioCarrilPendiente = false;
      fcSConfirmado = false;
      lecturaInicioMovimiento = potDoble.lecturaEfectiva;
      tiempoInicio = ahora;

      estadoActual = ESPERA_FC_S_RETORNO;
      return;
    }
  }

  if (fcSCarrilR()) {
    logFCCambio(F("FC_S R OK"));
    fcSConfirmado = true;
    cambioCarrilPendiente = false;
    lecturaInicioMovimiento = potDoble.lecturaEfectiva;
    tiempoInicio = ahora;
    iniciarMovimientoPosicion(MARCHA_R);
    return;
  }

  if (timeoutK2) {
    timeoutK2 = false;
    entrarErrorGrave();
    return;
  }

  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    desactivarK2();
    entrarErrorGrave();
  }
}

void estadoEsperaFCSRetorno() {
  uint32_t ahora = millis();

  // Puede venir de cualquier origen. Si todavía no estamos longitudinalmente
  // en N, continuamos hacia N; una vez en N solo esperamos FC_S HIGH.
  if (!enPosicion(posEfectiva(MARCHA_N))) {
    moverHacia(posEfectiva(MARCHA_N));

    if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
      registrarErrorMarcha(MARCHA_N);
    }
    return;
  }

  apagarActuador();

  if (fcSCarrilPrincipal()) {
    confirmarMarcha(MARCHA_N);
    return;
  }

  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    entrarErrorGrave();
  }
}

void entrarErrorGrave() {
  apagarTodoReles();
  timeoutK2 = false;
  movimientoEsN = false;
  movimientoEsR = false;
  cambioCarrilPendiente = false;
  fcSConfirmado = false;
  lecturaInicioMovimiento = 0;
  ledEstado = false;
  tiempoLed = millis();
  estadoActual = ERROR_GRAVE;
  logAccion(F("ERROR GRAVE"));
}

void estadoErrorGrave() {
  manejarParpadeoGrave();
}

// =============================================================================
// DIAGNÓSTICO SERIE
// =============================================================================

const __FlashStringHelper* nombreMarcha(Marcha m) {
  switch (m) {
    case MARCHA_R: return F("R");
    case MARCHA_N: return F("N");
    case MARCHA_1: return F("1");
    case MARCHA_2: return F("2");
  }
  return F("?");
}

const __FlashStringHelper* nombreEstado(Estado estado) {
  switch (estado) {
    case ARRANQUE: return F("ARRANQUE");
    case REPOSO: return F("REPOSO");
    case ESPERANDO_FC_C: return F("ESPERANDO_FC_C");
    case ESPERANDO_FC_S_PRINCIPAL: return F("ESPERANDO_FC_S_PRINCIPAL");
    case MOVIENDO: return F("MOVIENDO");
    case ESPERANDO_FC_S_CAMBIO_CARRIL: return F("ESPERANDO_FC_S_CAMBIO_CARRIL");
    case ESPERA_FC_S_RETORNO: return F("ESPERA_FC_S_RETORNO");
    case ERROR_GRAVE: return F("ERROR_GRAVE");
    case MODO_APRENDIZAJE: return F("MODO_APRENDIZAJE");
    case APRENDIZAJE_IR_A_2: return F("APRENDIZAJE_IR_A_2");
    case APRENDIZAJE_MOVIENDO: return F("APRENDIZAJE_MOVIENDO");
    case APRENDIZAJE_ESPERANDO_FC_C: return F("APRENDIZAJE_ESPERANDO_FC_C");
    case APRENDIZAJE_ESPERANDO_FC_S: return F("APRENDIZAJE_ESPERANDO_FC_S");
    case APRENDIZAJE_RECUPERANDO: return F("APRENDIZAJE_RECUPERANDO");
    case APRENDIZAJE_CONFIRMANDO: return F("APRENDIZAJE_CONFIRMANDO");
  }
  return F("?");
}

void printResetCause() {
#if DEBUG
  uint8_t mcusr = MCUSR;
  MCUSR = 0;

  DBG(F("RST "));
  bool causaEncontrada = false;

  if (mcusr & (1 << PORF)) {
    DBG(F("Power-On "));
    causaEncontrada = true;
  }

  if (mcusr & (1 << EXTRF)) {
    DBG(F("EXT "));
    causaEncontrada = true;
  }

  if (mcusr & (1 << BORF)) {
    DBG(F("BROWN-OUT "));
    causaEncontrada = true;
  }

  if (mcusr & (1 << WDRF)) {
    DBG(F("Watchdog Timer "));
    causaEncontrada = true;
  }

  if (!causaEncontrada) DBG(F("DESCONOCIDO"));
  DBGLN(F(""));
#endif
}

void printDiagnosticoInicial() {
#if DEBUG
  DBGLN(F("VERSION V8"));
  printResetCause();

  DBG(F("EEP R")); DBG_VAL(posADC_A[MARCHA_R]); DBG(F("/")); DBG_VAL(posADC_B[MARCHA_R]);
  DBG(F(" N")); DBG_VAL(posADC_A[MARCHA_N]); DBG(F("/")); DBG_VAL(posADC_B[MARCHA_N]);
  DBG(F(" 1")); DBG_VAL(posADC_A[MARCHA_1]); DBG(F("/")); DBG_VAL(posADC_B[MARCHA_1]);
  DBG(F(" 2")); DBG_VAL(posADC_A[MARCHA_2]); DBG(F("/")); DBGLN_VAL(posADC_B[MARCHA_2]);

  DBG(F("Pot A:")); DBG_VAL(potDoble.lecturaA);
  DBG(F(" B:")); DBG_VAL(potDoble.lecturaB);
  DBG(F(" Act:")); DBGLN_VAL(potDoble.pistaActiva == 0 ? F("A") : F("B"));
#endif
}

void logCambioMarcha(Marcha origen, Marcha destino) {
#if DEBUG
  DBG(F("CAM "));
  DBG_VAL(nombreMarcha(origen));
  DBG(F(">"));
  DBGLN_VAL(nombreMarcha(destino));
#endif
}

void logRele(const __FlashStringHelper* nombre, bool activado) {
#if DEBUG
  DBG_VAL(nombre);
  DBGLN_VAL(activado ? F(" ON") : F(" OFF"));
#endif
}

void logFCCambio(const __FlashStringHelper* mensaje) {
#if DEBUG
  DBGLN_VAL(mensaje);
#endif
}

void logPosicionAlcanzada(Marcha m) {
#if DEBUG
  DBG(F("POS "));
  DBG_VAL(nombreMarcha(m));
  DBGLN(F(" OK"));
  DBG(F("P "));
  DBG_VAL(potDoble.lecturaA);
  DBG(F("/"));
  DBGLN_VAL(potDoble.lecturaB);
#endif
}

void logAccion(const __FlashStringHelper* accion) {
#if DEBUG
  DBGLN_VAL(accion);
#endif
}

void logPot() {
#if DEBUG
  DBG(F("P "));
  DBG_VAL(potDoble.lecturaA);
  DBG(F("/"));
  DBG_VAL(potDoble.lecturaB);
  DBG(F(" Act:"));
  DBGLN_VAL(potDoble.pistaActiva == 0 ? F("A") : F("B"));
#endif
}

void imprimirDiagnosticoSistema() {
#if DEBUG
  DBGLN(F(""));
  DBGLN(F("=== SISTEMA ==="));
  DBG(F("Estado: ")); DBGLN_VAL(nombreEstado(estadoActual));
  DBG(F("Actual: ")); DBGLN_VAL(nombreMarcha(marchaActual));
  DBG(F("Destino: ")); DBGLN_VAL(nombreMarcha(marchaDestino));
  DBG(F("Origen: ")); DBGLN_VAL(nombreMarcha(marchaOrigen));

  DBG(F("FC_C: "));
  DBGLN_VAL(digitalRead(PIN_FC_C) == LOW ? F("LOW/OK") : F("HIGH"));

  DBG(F("FC_S: "));
  DBGLN_VAL(digitalRead(PIN_FC_S) == HIGH ? F("HIGH/PRINCIPAL") : F("LOW/R"));

  DBG(F("K1: ")); DBGLN_VAL(digitalRead(PIN_K1) == HIGH ? F("ON") : F("OFF"));
  DBG(F("K2: ")); DBGLN_VAL(digitalRead(PIN_K2) == HIGH ? F("ON") : F("OFF"));
  DBG(F("IN: ")); DBGLN_VAL(relInActivo ? F("ON") : F("OFF"));
  DBG(F("OUT: ")); DBGLN_VAL(relOutActivo ? F("ON") : F("OFF"));

  DBG(F("Errores R/N/1/2: "));
  DBG_VAL(errorMarcha[MARCHA_R]); DBG(F("/"));
  DBG_VAL(errorMarcha[MARCHA_N]); DBG(F("/"));
  DBG_VAL(errorMarcha[MARCHA_1]); DBG(F("/"));
  DBGLN_VAL(errorMarcha[MARCHA_2]);

  DBGLN(F(""));
  DBGLN(F("=== POTENCIOMETRO ==="));

  DBG(F("A: "));
  if (potDoble.pistaAFallada) {
    DBG(F("FALLADA "));
    DBGLN_VAL(nombreFalloPot(potDoble.motivoFalloA));
  } else {
    DBGLN(F("OK"));
  }

  DBG(F("  ADC: ")); DBGLN_VAL(potDoble.lecturaA);
  DBG(F("  Anterior: ")); DBGLN_VAL(potDoble.lecturaAnteriorA);
  DBG(F("  Fallo cnt: ")); DBGLN_VAL(potDoble.contadorFalloA);
  DBG(F("  Congelada: ")); DBGLN_VAL(potDoble.contadorCongeladoA);

  DBG(F("B: "));
  if (potDoble.pistaBFallada) {
    DBG(F("FALLADA "));
    DBGLN_VAL(nombreFalloPot(potDoble.motivoFalloB));
  } else {
    DBGLN(F("OK"));
  }

  DBG(F("  ADC: ")); DBGLN_VAL(potDoble.lecturaB);
  DBG(F("  Anterior: ")); DBGLN_VAL(potDoble.lecturaAnteriorB);
  DBG(F("  Fallo cnt: ")); DBGLN_VAL(potDoble.contadorFalloB);
  DBG(F("  Congelada: ")); DBGLN_VAL(potDoble.contadorCongeladoB);

  DBG(F("Pista activa: "));
  DBGLN_VAL(potDoble.pistaActiva == 0 ? F("A") : F("B"));

  DBG(F("Estado pot: "));
  if (potDoble.pistaAFallada && potDoble.pistaBFallada) {
    DBGLN(F("AMBAS FALLADAS"));
  } else if (potDoble.pistaAFallada || potDoble.pistaBFallada) {
    DBGLN(F("UNA PISTA FALLADA"));
  } else {
    DBGLN(F("OK"));
  }

  DBG(F("Diferencia A/B: "));
  DBGLN_VAL(abs((int)potDoble.lecturaA - (int)potDoble.lecturaB));

  DBG(F("Umbral A/B: "));
  DBGLN_VAL(umbralDiferenciaPistas(marchaActual));

  DBG(F("Discrepancias confirmadas: "));
  DBGLN_VAL(contadorDiscrepanciaPistas);

  DBGLN(F("================="));
#endif
}

void procesarComandoSerial() {
  if (!Serial.available()) return;

  char c = Serial.read();

  if (c == 'D' || c == 'd') {
    imprimirDiagnosticoSistema();
    return;
  }

  if (c == 'R' || c == 'r') {
    potDoble.contadorFalloA = 0;
    potDoble.contadorFalloB = 0;
    potDoble.contadorCongeladoA = 0;
    potDoble.contadorCongeladoB = 0;
    contadorDiscrepanciaPistas = 0;

    leerPotenciometro();
    potDoble.lecturaEfectiva = obtenerLecturaSegura();

    logAccion(F("RESET diagnostico: fallos latched NO borrados"));
  }
}

// =============================================================================
// APRENDIZAJE
// =============================================================================

void estadoModoAprendizaje() {
  uint32_t ahora = millis();

  manejarParpadeoAprendizaje(pinLED(marchaAprendizaje));

  if (errorAprendizajeActivo) {
    uint16_t intervalo =
      ledErrorAprendizajeEstado ? PARPADEO_ERROR_ON : PARPADEO_ERROR_OFF;

    if ((ahora - tiempoLedErrorAprendizaje) >= intervalo) {
      ledErrorAprendizajeEstado = !ledErrorAprendizajeEstado;
      tiempoLedErrorAprendizaje = ahora;
    }

    digitalWrite(pinLED(marchaErrorAprendizaje),
                 ledErrorAprendizajeEstado ? HIGH : LOW);
  }

  if (btnUp.presionado) {
    aprendizajeMovimientoAutomatico = false;
    tiempoInicio = ahora;
    estadoActual = APRENDIZAJE_MOVIENDO;
    activarReleIn();
    return;
  }

  if (btnDown.presionado) {
    aprendizajeMovimientoAutomatico = false;
    tiempoInicio = ahora;
    estadoActual = APRENDIZAJE_MOVIENDO;
    activarReleOut();
    return;
  }

  if (btnConf.presionado) {
    uint16_t valA = potDoble.lecturaA;
    uint16_t valB = potDoble.lecturaB;

    if (!validarLectura(valA) || !validarLectura(valB)) {
      logAccion(F("EEP SAVE RECHAZADO: ADC INVALIDO"));
      return;
    }

    guardarPosEnEEPROM(marchaAprendizaje, valA, valB);
    posicionAprendidaSesion[marchaAprendizaje] = true;

    apagarTodosLEDs();
    digitalWrite(pinLED(marchaAprendizaje), HIGH);

    tiempoAux = ahora;
    estadoActual = APRENDIZAJE_CONFIRMANDO;
    logAccion(F("EEP SAVE"));
  }
}

void estadoAprendizajeIrA2() {
  uint32_t ahora = millis();

  manejarParpadeoAprendizaje(pinLED(marchaAprendizaje));

  if (enPosicion(posEfectiva(MARCHA_2))) {
    apagarActuador();
    estadoActual = MODO_APRENDIZAJE;
    faseParpadeo = 0;
    tiempoLed = ahora;
    return;
  }

  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    apagarActuador();
    estadoActual = MODO_APRENDIZAJE;
    faseParpadeo = 0;
    tiempoLed = ahora;
    return;
  }

  activarReleOut();
}

void estadoAprendizajeMoviendo() {
  uint32_t ahora = millis();

  if (!aprendizajeMovimientoAutomatico) {
    manejarParpadeoAprendizaje(pinLED(marchaAprendizaje));

    if ((ahora - tiempoInicio) >= PASO_APRENDIZAJE_MS) {
      apagarActuador();
      estadoActual = MODO_APRENDIZAJE;
    }
    return;
  }

  manejarParpadeoAprendizaje(pinLED(marchaAprendizaje));

  if (timeoutK2) {
    timeoutK2 = false;
    apagarActuador();
    iniciarRecuperacionAprendizaje();
    return;
  }

  uint16_t objetivo = posicionObjetivoAprendizaje();

  if (enPosicion(objetivo)) {
    apagarActuador();

    if (marchaAprendizajeDestino == MARCHA_R) {
      desactivarK2();
      desactivarK1();
      marchaAprendizaje = MARCHA_R;
    } else {
      marchaAprendizaje = marchaAprendizajeDestino;
    }

    aprendizajeMovimientoAutomatico = false;
    faseParpadeo = 0;
    tiempoLed = ahora;
    estadoActual = MODO_APRENDIZAJE;
    return;
  }

  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    apagarActuador();

    if (marchaAprendizajeDestino == MARCHA_R) {
      desactivarK2();
      desactivarK1();
    }

    iniciarRecuperacionAprendizaje();
    return;
  }

  moverHacia(objetivo);
}

void estadoAprendizajeEsperandoFCC() {
  uint32_t ahora = millis();

  manejarParpadeoAprendizaje(pinLED(marchaAprendizaje));

  if (digitalRead(PIN_FC_C) == LOW) {
    logFCCambio(F("APR FC_C OK"));
    activarK2();
    tiempoInicio = ahora;
    estadoActual = APRENDIZAJE_ESPERANDO_FC_S;
    return;
  }

  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    apagarActuador();
    desactivarK1();
    iniciarRecuperacionAprendizaje();
  }
}

void estadoAprendizajeEsperandoFCS() {
  uint32_t ahora = millis();

  manejarParpadeoAprendizaje(pinLED(marchaAprendizaje));

  if (timeoutK2) {
    timeoutK2 = false;
    iniciarRecuperacionAprendizaje();
    return;
  }

  if (analogRead(PIN_FC_S) < 100) {
    logFCCambio(F("APR FC_S OK"));
    aprendizajeMovimientoAutomatico = true;
    tiempoInicio = ahora;
    estadoActual = APRENDIZAJE_MOVIENDO;
    return;
  }

  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    desactivarK2();
    iniciarRecuperacionAprendizaje();
  }
}

void estadoAprendizajeRecuperando() {
  uint32_t ahora = millis();

  desactivarK2();
  moverHacia(posEfectiva(marchaAprendizajeOrigen));

  if (enPosicion(posEfectiva(marchaAprendizajeOrigen))) {
    apagarActuador();
    desactivarK1();

    marchaAprendizaje = marchaAprendizajeOrigen;
    marchaErrorAprendizaje = marchaAprendizajeDestino;
    errorAprendizajeActivo = true;
    ledErrorAprendizajeEstado = false;
    tiempoLedErrorAprendizaje = ahora;
    faseParpadeo = 0;
    tiempoLed = ahora;
    aprendizajeMovimientoAutomatico = false;
    estadoActual = MODO_APRENDIZAJE;
    return;
  }

  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    apagarActuador();
    desactivarK1();
    desactivarK2();
    entrarErrorGrave();
  }
}

void estadoAprendizajeConfirmando() {
  uint32_t ahora = millis();

  if ((ahora - tiempoAux) < PARPADEO_CONFIRM_MS) return;

  apagarTodosLEDs();
  faseParpadeo = 0;
  tiempoLed = ahora;
  estadoActual = MODO_APRENDIZAJE;
  continuarAprendizajeTrasGuardar();
}

void iniciarSecuenciaAprendizajeNAR() {
  marchaAprendizajeOrigen = MARCHA_N;
  marchaAprendizajeDestino = MARCHA_R;
  aprendizajeMovimientoAutomatico = false;
  timeoutK2 = false;
  tiempoInicio = millis();
  activarK1();
  estadoActual = APRENDIZAJE_ESPERANDO_FC_C;
}

void iniciarMovimientoAprendizaje(Marcha destino) {
  marchaAprendizajeOrigen = marchaAprendizaje;
  marchaAprendizajeDestino = destino;
  aprendizajeMovimientoAutomatico = true;
  tiempoInicio = millis();
  faseParpadeo = 0;
  tiempoLed = tiempoInicio;
  estadoActual = APRENDIZAJE_MOVIENDO;
  logCambioMarcha(marchaAprendizajeOrigen, marchaAprendizajeDestino);
}

uint16_t posicionObjetivoAprendizaje() {
  if (marchaAprendizajeDestino == MARCHA_R &&
      !posicionAprendidaSesion[MARCHA_R]) {
    return posEfectiva(MARCHA_1);
  }

  return posEfectiva(marchaAprendizajeDestino);
}

void iniciarRecuperacionAprendizaje() {
  apagarActuador();
  desactivarK2();
  timeoutK2 = false;
  aprendizajeMovimientoAutomatico = false;
  tiempoInicio = millis();
  estadoActual = APRENDIZAJE_RECUPERANDO;
}

void continuarAprendizajeTrasGuardar() {
  if (errorAprendizajeActivo) {
    Marcha destino = marchaAprendizajeDestino;

    errorAprendizajeActivo = false;
    ledErrorAprendizajeEstado = false;
    tiempoLedErrorAprendizaje = millis();
    faseParpadeo = 0;
    tiempoLed = millis();

    if (destino == MARCHA_R) {
      iniciarSecuenciaAprendizajeNAR();
    } else {
      iniciarMovimientoAprendizaje(destino);
    }

    return;
  }

  switch (marchaAprendizaje) {
    case MARCHA_2:
      if (posicionAprendidaSesion[MARCHA_1]) {
        iniciarMovimientoAprendizaje(MARCHA_1);
      } else {
        marchaAprendizaje = MARCHA_1;
        faseParpadeo = 0;
        tiempoLed = millis();
      }
      break;

    case MARCHA_1:
      if (posicionAprendidaSesion[MARCHA_N]) {
        iniciarMovimientoAprendizaje(MARCHA_N);
      } else {
        marchaAprendizaje = MARCHA_N;
        faseParpadeo = 0;
        tiempoLed = millis();
      }
      break;

    case MARCHA_N:
      iniciarSecuenciaAprendizajeNAR();
      break;

    case MARCHA_R:
      iniciarMovimientoAprendizaje(MARCHA_2);
      break;
  }
}

// =============================================================================
// REDUNDANCIA DEL POTENCIÓMETRO
// =============================================================================

void registrarFalloPista(uint8_t pista, PotFallo motivo) {
  if (pista == 0) {
    if (potDoble.pistaAFallada) return;
    potDoble.pistaAFallada = true;
    potDoble.motivoFalloA = motivo;
    potDoble.contadorFalloA = 0;
  } else {
    if (potDoble.pistaBFallada) return;
    potDoble.pistaBFallada = true;
    potDoble.motivoFalloB = motivo;
    potDoble.contadorFalloB = 0;
  }

  logPot();
  iniciarRehabilitacionPista(pista);
}

void iniciarRehabilitacionPista(uint8_t pista) {
  if (pista > 1) return;

  if (pistaEnRehabilitacion != PISTA_NINGUNA &&
      pistaEnRehabilitacion != pista) {
    return;
  }

  pistaEnRehabilitacion = pista;

  for (uint8_t i = 0; i < 4; i++) {
    validacionesRehabilitacion[i] = 0;
  }

  posicionRehabilitacion = marchaActual;
  salioDeUltimaPosicionValidada = true;
  estabilidadRehabilitacionActiva = false;
  inicioEstabilidadRehabilitacion = 0;
  ultimaLecturaEstableRehabilitacion = 0;
}

void reiniciarRehabilitacionPista() {
  if (pistaEnRehabilitacion == PISTA_NINGUNA) return;

  for (uint8_t i = 0; i < 4; i++) {
    validacionesRehabilitacion[i] = 0;
  }

  posicionRehabilitacion = marchaActual;
  salioDeUltimaPosicionValidada = true;
  estabilidadRehabilitacionActiva = false;
  inicioEstabilidadRehabilitacion = 0;
  ultimaLecturaEstableRehabilitacion = 0;
}

bool posicionConfirmadaPorPistaSana(Marcha marcha) {
  if (pistaEnRehabilitacion == 0) {
    if (potDoble.pistaBFallada) return false;

    int16_t diferencia =
      (int16_t)potDoble.lecturaB - (int16_t)posADC_B[marcha];

    return diferencia >= -(int16_t)TOLERANCIA_ADC &&
           diferencia <=  (int16_t)TOLERANCIA_ADC;
  }

  if (pistaEnRehabilitacion == 1) {
    if (potDoble.pistaAFallada) return false;

    int16_t diferencia =
      (int16_t)potDoble.lecturaA - (int16_t)posADC_A[marcha];

    return diferencia >= -(int16_t)TOLERANCIA_ADC &&
           diferencia <=  (int16_t)TOLERANCIA_ADC;
  }

  return false;
}

void comprobarRehabilitacionPista() {
  if (pistaEnRehabilitacion == PISTA_NINGUNA) return;
  if (potDoble.pistaAFallada && potDoble.pistaBFallada) return;

  if (relInActivo || relOutActivo) {
    estabilidadRehabilitacionActiva = false;
    return;
  }

  if (estadoActual == MODO_APRENDIZAJE ||
      estadoActual == APRENDIZAJE_IR_A_2 ||
      estadoActual == APRENDIZAJE_MOVIENDO ||
      estadoActual == APRENDIZAJE_ESPERANDO_FC_C ||
      estadoActual == APRENDIZAJE_ESPERANDO_FC_S ||
      estadoActual == APRENDIZAJE_RECUPERANDO ||
      estadoActual == APRENDIZAJE_CONFIRMANDO) {
    estabilidadRehabilitacionActiva = false;
    return;
  }

  uint8_t pista = pistaEnRehabilitacion;

  if ((pista == 0 && !potDoble.pistaAFallada) ||
      (pista == 1 && !potDoble.pistaBFallada)) {
    pistaEnRehabilitacion = PISTA_NINGUNA;
    estabilidadRehabilitacionActiva = false;
    return;
  }

  Marcha marcha = marchaActual;

  if (marcha != posicionRehabilitacion) {
    posicionRehabilitacion = marcha;
    estabilidadRehabilitacionActiva = false;
    salioDeUltimaPosicionValidada = true;
  }

  if (!posicionConfirmadaPorPistaSana(marcha)) {
    estabilidadRehabilitacionActiva = false;
    return;
  }

  uint16_t lecturaPista =
    (pista == 0) ? potDoble.lecturaA : potDoble.lecturaB;

  uint16_t posicionEsperada =
    (pista == 0) ? posADC_A[marcha] : posADC_B[marcha];

  int16_t diferencia =
    (int16_t)lecturaPista - (int16_t)posicionEsperada;

  bool lecturaCorrecta =
    diferencia >= -(int16_t)TOLERANCIA_ADC &&
    diferencia <=  (int16_t)TOLERANCIA_ADC;

  if (!lecturaCorrecta) {
    reiniciarRehabilitacionPista();
    return;
  }

  if (!estabilidadRehabilitacionActiva) {
    estabilidadRehabilitacionActiva = true;
    inicioEstabilidadRehabilitacion = millis();
    ultimaLecturaEstableRehabilitacion = lecturaPista;
    return;
  }

  if (abs((int)lecturaPista -
          (int)ultimaLecturaEstableRehabilitacion) > 8) {
    reiniciarRehabilitacionPista();
    return;
  }

  ultimaLecturaEstableRehabilitacion = lecturaPista;

  if ((millis() - inicioEstabilidadRehabilitacion) < 2000UL) return;

  if (!salioDeUltimaPosicionValidada) {
    estabilidadRehabilitacionActiva = false;
    return;
  }

  if (validacionesRehabilitacion[marcha] < 2) {
    validacionesRehabilitacion[marcha]++;
  }

  salioDeUltimaPosicionValidada = false;
  estabilidadRehabilitacionActiva = false;

  bool completa = true;

  for (uint8_t i = 0; i < 4; i++) {
    if (validacionesRehabilitacion[i] < 2) {
      completa = false;
      break;
    }
  }

  if (completa) rehabilitarPista();
}

void rehabilitarPista() {
  if (pistaEnRehabilitacion == PISTA_NINGUNA) return;

  uint8_t pista = pistaEnRehabilitacion;

  if (pista == 0) {
    potDoble.pistaAFallada = false;
    potDoble.motivoFalloA = POT_FALLO_NINGUNO;
    potDoble.contadorFalloA = 0;
    potDoble.contadorCongeladoA = 0;
    potDoble.pistaActiva = 0;
  } else {
    potDoble.pistaBFallada = false;
    potDoble.motivoFalloB = POT_FALLO_NINGUNO;
    potDoble.contadorFalloB = 0;
    potDoble.contadorCongeladoB = 0;
    potDoble.pistaActiva = 1;
  }

  pistaEnRehabilitacion = PISTA_NINGUNA;
  estabilidadRehabilitacionActiva = false;

  for (uint8_t i = 0; i < 4; i++) {
    validacionesRehabilitacion[i] = 0;
  }

  ambasPistasFalladas =
    potDoble.pistaAFallada && potDoble.pistaBFallada;
}

bool verificarPistas() {
  uint16_t valA = potDoble.lecturaA;
  uint16_t valB = potDoble.lecturaB;

  bool primeraLectura = potDoble.primeraLectura;
  bool moviendo = relInActivo || relOutActivo;

  int16_t deltaA = 0;
  int16_t deltaB = 0;

  if (!primeraLectura) {
    deltaA = (int16_t)valA -
             (int16_t)potDoble.lecturaAnteriorA;

    deltaB = (int16_t)valB -
             (int16_t)potDoble.lecturaAnteriorB;
  }

  bool fueraRangoA = !validarLectura(valA);
  bool fueraRangoB = !validarLectura(valB);
  bool saltoA = false;
  bool saltoB = false;
  bool direccionIncorrectaA = false;
  bool direccionIncorrectaB = false;
  bool congeladaA = false;
  bool congeladaB = false;

  if (!primeraLectura) {
    saltoA = abs((int)deltaA) > UMBRAL_SALTO_ERRATICO;
    saltoB = abs((int)deltaB) > UMBRAL_SALTO_ERRATICO;

    if (moviendo) {
      if (abs((int)deltaA) < UMBRAL_CONGELADO) {
        potDoble.contadorCongeladoA++;
      } else {
        potDoble.contadorCongeladoA = 0;
      }

      if (abs((int)deltaB) < UMBRAL_CONGELADO) {
        potDoble.contadorCongeladoB++;
      } else {
        potDoble.contadorCongeladoB = 0;
      }

      congeladaA =
        potDoble.contadorCongeladoA >= LECTURAS_CONGELADO;

      congeladaB =
        potDoble.contadorCongeladoB >= LECTURAS_CONGELADO;

      if (relInActivo) {
        if (deltaA > 5) direccionIncorrectaA = true;
        if (deltaB > 5) direccionIncorrectaB = true;
      } else if (relOutActivo) {
        if (deltaA < -5) direccionIncorrectaA = true;
        if (deltaB < -5) direccionIncorrectaB = true;
      }
    } else {
      potDoble.contadorCongeladoA = 0;
      potDoble.contadorCongeladoB = 0;
    }
  }

  bool falloActualA =
    fueraRangoA || saltoA || congeladaA || direccionIncorrectaA;

  bool falloActualB =
    fueraRangoB || saltoB || congeladaB || direccionIncorrectaB;

  PotFallo motivoA = POT_FALLO_NINGUNO;
  PotFallo motivoB = POT_FALLO_NINGUNO;

  if (fueraRangoA) motivoA = POT_FALLO_FUERA_RANGO;
  else if (saltoA) motivoA = POT_FALLO_SALTO_ERRATICO;
  else if (congeladaA) motivoA = POT_FALLO_CONGELADA;
  else if (direccionIncorrectaA) motivoA = POT_FALLO_DIRECCION;

  if (fueraRangoB) motivoB = POT_FALLO_FUERA_RANGO;
  else if (saltoB) motivoB = POT_FALLO_SALTO_ERRATICO;
  else if (congeladaB) motivoB = POT_FALLO_CONGELADA;
  else if (direccionIncorrectaB) motivoB = POT_FALLO_DIRECCION;

  if (!primeraLectura &&
      !fueraRangoA &&
      !fueraRangoB &&
      !potDoble.pistaAFallada &&
      !potDoble.pistaBFallada) {

    int diff = abs((int)valA - (int)valB);
    uint16_t umbral =
      umbralDiferenciaPistas(marchaActual);

    if (diff > umbral) {
      bool culpableA =
        saltoA || direccionIncorrectaA || congeladaA;

      bool culpableB =
        saltoB || direccionIncorrectaB || congeladaB;

      if (culpableA && !culpableB) {
        falloActualA = true;
        motivoA =
          saltoA ? POT_FALLO_SALTO_ERRATICO :
          direccionIncorrectaA ? POT_FALLO_DIRECCION :
          POT_FALLO_CONGELADA;
        potDoble.contadorFalloB = 0;

      } else if (culpableB && !culpableA) {
        falloActualB = true;
        motivoB =
          saltoB ? POT_FALLO_SALTO_ERRATICO :
          direccionIncorrectaB ? POT_FALLO_DIRECCION :
          POT_FALLO_CONGELADA;
        potDoble.contadorFalloA = 0;

      } else if (!culpableA && !culpableB) {
        contadorDiscrepanciaPistas++;

        if (contadorDiscrepanciaPistas >=
            LECTURAS_CONFIRMACION_DISCREPANCIA) {

          potDoble.pistaAFallada = true;
          potDoble.pistaBFallada = true;
          potDoble.motivoFalloA = POT_FALLO_DISCREPANCIA;
          potDoble.motivoFalloB = POT_FALLO_DISCREPANCIA;
          ambasPistasFalladas = true;

          entrarErrorGrave();
        }
      } else {
        falloActualA = true;
        falloActualB = true;

        if (motivoA == POT_FALLO_NINGUNO) {
          motivoA = POT_FALLO_DISCREPANCIA;
        }

        if (motivoB == POT_FALLO_NINGUNO) {
          motivoB = POT_FALLO_DISCREPANCIA;
        }
      }
    } else {
      contadorDiscrepanciaPistas = 0;
    }
  }

  if (!potDoble.pistaAFallada && falloActualA) {
    potDoble.contadorFalloA++;

    uint8_t limite =
      fueraRangoA
      ? LECTURAS_CONFIRMACION_RANGO
      : LECTURAS_CONFIRMACION_FALLO;

    if (congeladaA) limite = 1;

    if (potDoble.contadorFalloA >= limite) {
      registrarFalloPista(0, motivoA);
    }
  } else if (!falloActualA) {
    potDoble.contadorFalloA = 0;
  }

  if (!potDoble.pistaBFallada && falloActualB) {
    potDoble.contadorFalloB++;

    uint8_t limite =
      fueraRangoB
      ? LECTURAS_CONFIRMACION_RANGO
      : LECTURAS_CONFIRMACION_FALLO;

    if (congeladaB) limite = 1;

    if (potDoble.contadorFalloB >= limite) {
      registrarFalloPista(1, motivoB);
    }
  } else if (!falloActualB) {
    potDoble.contadorFalloB = 0;
  }

  potDoble.lecturaAnteriorA = valA;
  potDoble.lecturaAnteriorB = valB;
  potDoble.primeraLectura = false;

  ambasPistasFalladas =
    potDoble.pistaAFallada && potDoble.pistaBFallada;

  if (ambasPistasFalladas) {
    if (estadoActual != ERROR_GRAVE) {
      entrarErrorGrave();
    }
    return false;
  }

  if (potDoble.pistaActiva == 0 &&
      potDoble.pistaAFallada &&
      !potDoble.pistaBFallada) {

    potDoble.pistaActiva = 1;
    potDoble.ultimoCambioPista = millis();

  } else if (potDoble.pistaActiva == 1 &&
             potDoble.pistaBFallada &&
             !potDoble.pistaAFallada) {

    potDoble.pistaActiva = 0;
    potDoble.ultimoCambioPista = millis();
  }

  comprobarRehabilitacionPista();
  return true;
}
