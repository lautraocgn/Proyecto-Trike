// =============================================================================
// SELECTOR DE MARCHAS VW AUTOSTICK — CON REDUNDANCIA DE POTENCIÓMETRO
// Versión 4.2 — Código completo y estable
// =============================================================================

#include <EEPROM.h>

// =============================================================================
// CONSTANTES CONFIGURABLES
// =============================================================================

const uint16_t DEBOUNCE_MS               = 20;
const uint16_t BLOQUEO_MS                = 500;
const uint16_t TIEMPO_PULSACION_LARGA_MS = 600;
const uint16_t TIMEOUT_MS                = 3000;
const uint16_t RETARDO_RELE_MS           = 50;
const uint16_t TIEMPO_LECTURA_POT        = 20;
const uint16_t TOLERANCIA_ADC            = 25;
const uint16_t PASO_APRENDIZAJE_MS       = 100;

// --- Redundancia de potenciómetro ---
const uint16_t RANGO_MIN_ADC             = 10;
const uint16_t RANGO_MAX_ADC             = 1013;
const uint16_t TIEMPO_VERIFICACION_POT   = 150;    // <--- CAMBIADO: 100 -> 200ms
const uint8_t  NUM_MUESTRAS_PROMEDIO     = 5;      // <--- CAMBIADO: 3 -> 5 (más filtrado)
const uint16_t UMBRAL_SALTO_ERRATICO     = 120;    // <--- CAMBIADO: 100 -> 150
const uint16_t UMBRAL_CONGELADO          = 3;
const uint8_t  LECTURAS_CONGELADO        = 8;      // <--- CAMBIADO: 5 -> 8
const uint16_t VALOR_CENTINELA           = 0xFFFF;

// --- Patrones de parpadeo ---
const uint16_t PARPADEO_ERROR_ON         = 250;
const uint16_t PARPADEO_ERROR_OFF        = 250;
const uint16_t PARPADEO_APREND_ON        = 150;
const uint16_t PARPADEO_APREND_OFF       = 150;
const uint16_t PARPADEO_APREND_PAUSA     = 600;
const uint16_t PARPADEO_CONFIRM_MS       = 500;
const uint16_t PARPADEO_AVISO_EEPROM_MS  = 5000;
const uint16_t PARPADEO_POT_ALERT_ON     = 500;
const uint16_t PARPADEO_POT_ALERT_OFF    = 500;

// --- EEPROM ---
const uint8_t  EEPROM_FIRMA_ADDR   = 0;
const uint8_t  EEPROM_FIRMA_VAL    = 0xA5;
const uint8_t  EEPROM_POS_A_BASE   = 2;
const uint8_t  EEPROM_POS_B_BASE   = 10;

// --- Valores por defecto (diferenciados) ---
const uint16_t DEFAULT_POS_A_R     = 334;
const uint16_t DEFAULT_POS_A_N     = 703;
const uint16_t DEFAULT_POS_A_1     = 461;
const uint16_t DEFAULT_POS_A_2     = 874;

const uint16_t DEFAULT_POS_B_R     = 338;
const uint16_t DEFAULT_POS_B_N     = 709;
const uint16_t DEFAULT_POS_B_1     = 468;
const uint16_t DEFAULT_POS_B_2     = 878;

// =============================================================================
// DEBUG
// =============================================================================

#define DEBUG 1   // Cambiar a 0 para producción

#if DEBUG
  #define DBG(x)        Serial.print(F(x))
  #define DBGLN(x)      Serial.println(F(x))
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

enum Estado {
  ARRANQUE,
  REPOSO,
  ESPERANDO_FC_C,
  MOVIENDO,
  ESPERA_FC_S_RETORNO,
  EMERGENCIA_A_N,
  RECUPERANDO_A_N,
  ERROR_LEVE,
  ERROR_GRAVE,
  MODO_APRENDIZAJE,
  APRENDIZAJE_MOVIENDO,
  APRENDIZAJE_CONFIRMANDO
};

// =============================================================================
// ESTRUCTURAS
// =============================================================================

struct Boton {
  uint8_t  pin;
  bool     estadoAnterior;
  bool     estadoActual;
  bool     presionado;
  bool     soltado;
  uint32_t tiempoPresion;
  uint32_t ultimoCambio;
  bool     bloqueado;
};

struct PotenciometroDoble {
  uint16_t lecturaA;
  uint16_t lecturaB;
  uint16_t lecturaEfectiva;
  bool     pistaAFallada;
  bool     pistaBFallada;
  uint8_t  pistaActiva;
  uint32_t ultimoCambioPista;
  uint32_t ultimaVerificacion;
  uint8_t  contadorCongeladoA;
  uint8_t  contadorCongeladoB;
  uint16_t lecturaAnteriorA;
  uint16_t lecturaAnteriorB;
  bool     primeraLectura;
};

// =============================================================================
// VARIABLES GLOBALES
// =============================================================================

Estado estadoActual     = ARRANQUE;
Marcha marchaActual     = MARCHA_N;
Marcha marchaDestino    = MARCHA_N;
Marcha marchaOrigen     = MARCHA_N;
Marcha marchaError      = MARCHA_N;

uint16_t posADC_A[4];
uint16_t posADC_B[4];

uint32_t tiempoInicio   = 0;
uint32_t tiempoAux      = 0;

bool relInActivo  = false;
bool relOutActivo = false;
uint32_t tiempoApagadoRele = 0;
bool esperandoRetardoRele  = false;
bool relPendienteIn        = false;
bool relPendienteOut       = false;
bool cambioCarrilPendiente = false;

Boton btnUp   = {PIN_BTN_UP,   true, true, false, false, 0, 0, false};
Boton btnDown = {PIN_BTN_DOWN, true, true, false, false, 0, 0, false};
Boton btnModo = {PIN_BTN_MODO, true, true, false, false, 0, 0, false};
Boton btnConf = {PIN_BTN_CONF, true, true, false, false, 0, 0, false};

bool     bloqueoCambio     = false;
uint32_t tiempoBloqueo     = 0;

uint32_t tiempoLed         = 0;
bool     ledEstado         = false;
uint8_t  contadorDestellos = 0;
uint8_t  faseParpadeo      = 0;

Marcha marchaAprendizaje   = MARCHA_N;

bool   avisandoEEPROM      = false;
uint32_t tiempoAvisoEEPROM = 0;

Estado ultimoEstadoLog     = ARRANQUE;
uint32_t ultimoLogPot      = 0;

PotenciometroDoble potDoble = {
  0, 0, 0,
  false, false,
  0, 0, 0,
  0, 0, 0, 0,
  true
};

bool potAlertState = false;
uint32_t tiempoPotAlert = 0;
bool ambasPistasFalladas = false;

// =============================================================================
// PROTOTIPOS
// =============================================================================

void leerBotones();
void actualizarBoton(Boton &btn);
bool upDownSimultaneos();
void activarReleIn();
void activarReleOut();
void apagarActuador();
void gestionarRetardoRele();
void activarK1();
void desactivarK1();
void activarK2();
void desactivarK2();
void apagarTodoReles();

void leerPotenciometro();
uint16_t promediarLecturas(uint8_t pin);
bool validarLectura(uint16_t val);
uint16_t umbralDiferenciaPistas(Marcha m);
bool verificarPistas();
uint16_t posEfectiva(Marcha m);
uint16_t obtenerLecturaSegura();
uint16_t leerPot();

bool enPosicion(uint16_t pos);
void moverHacia(uint16_t pos);

void guardarPosEnEEPROM(Marcha m, uint16_t valorA, uint16_t valorB);
void cargarPosiciones();

uint8_t pinLED(Marcha m);
void apagarTodosLEDs();
void encenderLED(Marcha m);
void manejarParpadeoSimple(uint8_t pin);
void manejarParpadeoAprendizaje(uint8_t pin);
void manejarParpadeoGrave();
void manejarParpadeoPotAlert();

void printResetCause();
void printDiagnosticoInicial();
void logCambioMarcha(Marcha origen, Marcha destino);
void logRele(const char* nombre, bool activado);
void logFCCambio(const char* mensaje);
void logPosicionAlcanzada(Marcha m);

void estadoArranque();
void estadoReposo();
void estadoEsperandoFCC();
void estadoMoviendo();
void estadoEsperaFCSRetorno();
void estadoEmergenciaN();
void estadoRecuperandoN();
void estadoErrorLeve();
void estadoErrorGrave();
void estadoModoAprendizaje();
void estadoAprendizajeMoviendo();
void estadoAprendizajeConfirmando();

void iniciarCambioA(Marcha destino);
void entrarErrorGrave();
const char* nombreMarcha(Marcha m);
const char* nombreEstado(Estado e);
//void logEstado();
void logPot();
void logAccion(const char* accion);
void procesarComandoSerial();

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(9600);

  pinMode(PIN_BTN_UP,   INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_MODO, INPUT_PULLUP);
  pinMode(PIN_BTN_CONF, INPUT_PULLUP);
  pinMode(PIN_FC_S,     INPUT_PULLUP);
  pinMode(PIN_FC_C,     INPUT_PULLUP);

  pinMode(PIN_REL_IN,  OUTPUT); digitalWrite(PIN_REL_IN,  LOW);
  pinMode(PIN_REL_OUT, OUTPUT); digitalWrite(PIN_REL_OUT, LOW);
  pinMode(PIN_K1,      OUTPUT); digitalWrite(PIN_K1,      LOW);
  pinMode(PIN_K2,      OUTPUT); digitalWrite(PIN_K2,      LOW);
  pinMode(PIN_LED_R,   OUTPUT); digitalWrite(PIN_LED_R,   LOW);
  pinMode(PIN_LED_N,   OUTPUT); digitalWrite(PIN_LED_N,   LOW);
  pinMode(PIN_LED_1,   OUTPUT); digitalWrite(PIN_LED_1,   LOW);
  pinMode(PIN_LED_2,   OUTPUT); digitalWrite(PIN_LED_2,   LOW);
  pinMode(PIN_LED_POT_ALERT, OUTPUT); digitalWrite(PIN_LED_POT_ALERT, LOW);

  cargarPosiciones();
  leerPotenciometro();
  potDoble.lecturaEfectiva = obtenerLecturaSegura();

  printDiagnosticoInicial();

  estadoActual = ARRANQUE;
  tiempoInicio = millis();
  DBGLN("Arrancando hacia N...");
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

//  logEstado();
  logPot();
  manejarParpadeoPotAlert();
  procesarComandoSerial();

  bool modoAprendizajeActivo = (digitalRead(PIN_BTN_MODO) == LOW);

  if (modoAprendizajeActivo &&
      estadoActual != MODO_APRENDIZAJE &&
      estadoActual != APRENDIZAJE_MOVIENDO &&
      estadoActual != APRENDIZAJE_CONFIRMANDO &&
      estadoActual != ERROR_GRAVE) {
    logAccion("MODO APRENDIZAJE");
    apagarTodoReles();
    estadoActual      = MODO_APRENDIZAJE;
    marchaAprendizaje = MARCHA_N;
    faseParpadeo      = 0;
    contadorDestellos = 0;
    tiempoLed         = ahora;
    if (!enPosicion(posEfectiva(MARCHA_N))) {
      moverHacia(posEfectiva(MARCHA_N));
    }
    return;
  }

  if (!modoAprendizajeActivo &&
      (estadoActual == MODO_APRENDIZAJE ||
       estadoActual == APRENDIZAJE_MOVIENDO ||
       estadoActual == APRENDIZAJE_CONFIRMANDO)) {
    logAccion("Saliendo de MODO APRENDIZAJE -> ARRANQUE");
    apagarTodoReles();
    estadoActual = ARRANQUE;
    tiempoInicio = ahora;
    return;
  }

  switch (estadoActual) {
    case ARRANQUE:                estadoArranque();              break;
    case REPOSO:                  estadoReposo();                break;
    case ESPERANDO_FC_C:          estadoEsperandoFCC();          break;
    case MOVIENDO:                estadoMoviendo();              break;
    case ESPERA_FC_S_RETORNO:     estadoEsperaFCSRetorno();      break;
    case EMERGENCIA_A_N:          estadoEmergenciaN();           break;
    case RECUPERANDO_A_N:         estadoRecuperandoN();          break;
    case ERROR_LEVE:              estadoErrorLeve();             break;
    case ERROR_GRAVE:             estadoErrorGrave();            break;
    case MODO_APRENDIZAJE:        estadoModoAprendizaje();       break;
    case APRENDIZAJE_MOVIENDO:    estadoAprendizajeMoviendo();   break;
    case APRENDIZAJE_CONFIRMANDO: estadoAprendizajeConfirmando();break;
  }
}

// =============================================================================
// POSICIÓN EFECTIVA
// =============================================================================

uint16_t posEfectiva(Marcha m) {
  return (potDoble.pistaActiva == 0) ? posADC_A[m] : posADC_B[m];
}

// =============================================================================
// DIAGNÓSTICO
// =============================================================================

void printResetCause() {
#if DEBUG
  uint8_t mcusr = MCUSR;
  MCUSR = 0;
  DBG("Reset: ");
  if (mcusr & (1 << PORF)) DBG("Power-On ");
  if (mcusr & (1 << EXTRF)) DBG("EXT ");
  if (mcusr & (1 << WDRF)) DBG("Watchdog ");
  DBGLN("");
#endif
}

void printDiagnosticoInicial() {
#if DEBUG
  DBGLN("V7.2");
  printResetCause();
  
  // EEPROM en formato compacto: R=334/338 N=703/709 1=461/468 2=874/878
  DBG("EEPROM: R=");
  DBG_VAL(posADC_A[MARCHA_R]);
  DBG("/");
  DBG_VAL(posADC_B[MARCHA_R]);
  DBG(" N=");
  DBG_VAL(posADC_A[MARCHA_N]);
  DBG("/");
  DBG_VAL(posADC_B[MARCHA_N]);
  DBG(" 1=");
  DBG_VAL(posADC_A[MARCHA_1]);
  DBG("/");
  DBG_VAL(posADC_B[MARCHA_1]);
  DBG(" 2=");
  DBG_VAL(posADC_A[MARCHA_2]);
  DBG("/");
  DBGLN_VAL(posADC_B[MARCHA_2]);
  
  // Potenciómetros iniciales
  DBG("Pot  A:");
  DBG_VAL(potDoble.lecturaA);
  DBG("   B:");
  DBG_VAL(potDoble.lecturaB);
  DBG("   Act:");
  DBGLN_VAL(potDoble.pistaActiva == 0 ? "A" : "B");
  
  DBGLN("");
#endif
}

void logCambioMarcha(Marcha origen, Marcha destino) {
#if DEBUG
  DBG("[");
  DBG_VAL(millis());
  DBG("ms] CAMBIO: ");
  DBG_VAL(nombreMarcha(origen));
  DBG(" -> ");
  DBG_VAL(nombreMarcha(destino));
  DBG("  Pot: ");
  DBGLN_VAL(potDoble.lecturaEfectiva);
#endif
}

void logRele(const char* nombre, bool activado) {
#if DEBUG
  DBG_VAL(nombre);
  DBGLN_VAL(activado ? " ON" : " OFF");
#endif
}

void logFCCambio(const char* mensaje) {
#if DEBUG
  DBGLN_VAL(mensaje);
#endif
}

void logPosicionAlcanzada(Marcha m) {
#if DEBUG
  DBG("POS ");
  DBG_VAL(nombreMarcha(m));
  DBGLN(" OK");
#endif
}

// =============================================================================
// MONITOR SERIE
// =============================================================================

const char* nombreMarcha(Marcha m) {
  switch (m) {
    case MARCHA_R: return "R";
    case MARCHA_N: return "N";
    case MARCHA_1: return "1";
    case MARCHA_2: return "2";
  }
  return "?";
}

const char* nombreEstado(Estado e) {
  switch (e) {
    case ARRANQUE:                return "ARRANQUE";
    case REPOSO:                  return "REPOSO";
    case ESPERANDO_FC_C:          return "ESPERANDO_FC_C";
    case MOVIENDO:                return "MOVIENDO";
    case ESPERA_FC_S_RETORNO:     return "ESPERA_FC_S_RETORNO";
    case EMERGENCIA_A_N:          return "EMERGENCIA_A_N";
    case RECUPERANDO_A_N:         return "RECUPERANDO_A_N";
    case ERROR_LEVE:              return "ERROR_LEVE";
    case ERROR_GRAVE:             return "ERROR_GRAVE";
    case MODO_APRENDIZAJE:        return "MODO_APRENDIZAJE";
    case APRENDIZAJE_MOVIENDO:    return "APRENDIZAJE_MOVIENDO";
    case APRENDIZAJE_CONFIRMANDO: return "APRENDIZAJE_CONFIRMANDO";
  }
  return "?";
}

/*
void logEstado() {
#if DEBUG
  if (estadoActual != ultimoEstadoLog) {
    DBG("[");
    DBG_VAL(millis());
    DBG("ms] Estado: ");
    DBG_VAL(nombreEstado(ultimoEstadoLog));
    DBG(" -> ");
    DBGLN_VAL(nombreEstado(estadoActual));
    ultimoEstadoLog = estadoActual;
  }
#endif
}
*/

void logPot() {
#if DEBUG
  static uint16_t ultimaLecturaLog = 0;
  static bool primerLog = true;
  
  uint16_t actual = potDoble.lecturaEfectiva;
  bool cambioSignificativo = (primerLog || abs((int)actual - (int)ultimaLecturaLog) > 8);
  
  if (cambioSignificativo) {
    primerLog = false;
    ultimaLecturaLog = actual;
    
    // Si es la primera vez o hay cambio de pista, mostrar formato completo
    if (primerLog) {
      DBG("Pot  A:");
      DBG_VAL(potDoble.lecturaA);
      DBG("   B:");
      DBG_VAL(potDoble.lecturaB);
      DBG("   Act:");
      DBGLN_VAL(potDoble.pistaActiva == 0 ? "A" : "B");
    } else {
      // Formato compacto durante el movimiento
      DBG("P ");
      DBG_VAL(potDoble.lecturaA);
      DBG("/");
      DBG_VAL(potDoble.lecturaB);
      DBG(" Act:");
      DBGLN_VAL(potDoble.pistaActiva == 0 ? "A" : "B");
    }
  }
#endif
}

void logAccion(const char* accion) {
#if DEBUG
  DBGLN_VAL(accion);
#endif
}

// =============================================================================
// COMANDO SERIAL
// =============================================================================

void procesarComandoSerial() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'R' || c == 'r') {
      logAccion("Reset de fallos de potenciómetro por comando serial");
      potDoble.pistaAFallada = false;
      potDoble.pistaBFallada = false;
      potDoble.contadorCongeladoA = 0;
      potDoble.contadorCongeladoB = 0;
      ambasPistasFalladas = false;
      digitalWrite(PIN_LED_POT_ALERT, LOW);
      potAlertState = false;
      leerPotenciometro();
      verificarPistas();
      potDoble.lecturaEfectiva = obtenerLecturaSegura();
      DBGLN("Fallos reseteados.");
    }
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
  uint32_t ahora   = millis();
  bool     lectura = (digitalRead(btn.pin) == LOW);

  btn.presionado = false;
  btn.soltado    = false;

  if (lectura != btn.estadoAnterior) {
    if ((ahora - btn.ultimoCambio) >= DEBOUNCE_MS) {
      btn.estadoAnterior = lectura;
      btn.ultimoCambio   = ahora;
      if (lectura) {
        btn.tiempoPresion = ahora;
        btn.bloqueado     = false;
      } else {
        btn.soltado   = true;
        btn.bloqueado = false;
      }
    }
  }

  btn.estadoActual = lectura;

  if (btn.soltado && !btn.bloqueado) {
    btn.presionado = true;
    btn.bloqueado  = true;
  }
}

bool upDownSimultaneos() {
  return (btnUp.estadoActual && btnDown.estadoActual);
}

// =============================================================================
// RELÉS
// =============================================================================

void activarReleIn() {
  if (relInActivo) return;
  if (relOutActivo) {
    digitalWrite(PIN_REL_OUT, LOW);
    relOutActivo         = false;
    tiempoApagadoRele    = millis();
    esperandoRetardoRele = true;
    relPendienteIn       = true;
    relPendienteOut      = false;
    return;
  }
  digitalWrite(PIN_REL_IN, HIGH);
  relInActivo = true;
  logRele("IN", true);
}

void activarReleOut() {
  if (relOutActivo) return;
  if (relInActivo) {
    digitalWrite(PIN_REL_IN, LOW);
    relInActivo          = false;
    tiempoApagadoRele    = millis();
    esperandoRetardoRele = true;
    relPendienteOut      = true;
    relPendienteIn       = false;
    return;
  }
  digitalWrite(PIN_REL_OUT, HIGH);
  relOutActivo = true;
  logRele("REL_OUT", true);
}

void apagarActuador() {
  bool habia = relInActivo || relOutActivo;
  digitalWrite(PIN_REL_IN,  LOW);
  digitalWrite(PIN_REL_OUT, LOW);
  relInActivo          = false;
  relOutActivo         = false;
  esperandoRetardoRele = false;
  relPendienteIn       = false;
  relPendienteOut      = false;
  if (habia) logAccion("Actuador detenido");
}

void gestionarRetardoRele() {
  if (!esperandoRetardoRele) return;
  if ((millis() - tiempoApagadoRele) >= RETARDO_RELE_MS) {
    esperandoRetardoRele = false;
    if (relPendienteIn) {
      relPendienteIn = false;
      digitalWrite(PIN_REL_IN, HIGH);
      relInActivo = true;
      logRele("REL_IN", true);
    } else if (relPendienteOut) {
      relPendienteOut = false;
      digitalWrite(PIN_REL_OUT, HIGH);
      relOutActivo = true;
      logRele("OUT", true);
    }
  }
}

void activarK1() {
  digitalWrite(PIN_K1, HIGH);
  logRele("K1", true);
}

void desactivarK1() {
  digitalWrite(PIN_K1, LOW);
  logRele("K1", false);
}

void activarK2() {
  digitalWrite(PIN_K2, HIGH);
  logRele("K2", true);
}

void desactivarK2() {
  digitalWrite(PIN_K2, LOW);
  logRele("K2", false);
}

void apagarTodoReles() {
  apagarActuador();
  digitalWrite(PIN_K1, LOW);
  digitalWrite(PIN_K2, LOW);
  logAccion("Todos los reles apagados");
}

// =============================================================================
// POTENCIÓMETRO REDUNDANTE
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
  return (val >= RANGO_MIN_ADC && val <= RANGO_MAX_ADC);
}

uint16_t umbralDiferenciaPistas(Marcha m) {
  int16_t diffBase = abs((int)posADC_A[m] - (int)posADC_B[m]);
  // Con diferencias reales de 4-7, el umbral debe ser ajustado
  uint16_t umbral = (uint16_t)((float)diffBase * 2.5) + 15;  // Factor 2.5, margen 15
  if (umbral < 25) umbral = 25;  // Mínimo absoluto
  return umbral;
}

bool verificarPistas() {
  uint16_t valA = potDoble.lecturaA;
  uint16_t valB = potDoble.lecturaB;
  bool validaA = validarLectura(valA);
  bool validaB = validarLectura(valB);
  
  bool diffOk = true;
  if (validaA && validaB) {
    int diff = abs((int)valA - (int)valB);
    uint16_t umbral = umbralDiferenciaPistas(marchaActual);
    if (diff > umbral) {
      diffOk = false;
      #if DEBUG
      DBG("Diferencia pistas excede umbral: diff=");
      DBG_VAL(diff);
      DBG(" umbral=");
      DBGLN_VAL(umbral);
      #endif
    }
  }

  bool moviendo = relInActivo || relOutActivo;
  if (moviendo) {
    if (abs((int)valA - (int)potDoble.lecturaAnteriorA) < UMBRAL_CONGELADO) {
      potDoble.contadorCongeladoA++;
    } else {
      potDoble.contadorCongeladoA = 0;
    }
    if (potDoble.contadorCongeladoA >= LECTURAS_CONGELADO) validaA = false;
    
    if (abs((int)valB - (int)potDoble.lecturaAnteriorB) < UMBRAL_CONGELADO) {
      potDoble.contadorCongeladoB++;
    } else {
      potDoble.contadorCongeladoB = 0;
    }
    if (potDoble.contadorCongeladoB >= LECTURAS_CONGELADO) validaB = false;
  } else {
    potDoble.contadorCongeladoA = 0;
    potDoble.contadorCongeladoB = 0;
  }

  if (!potDoble.primeraLectura) {
    if (abs((int)valA - (int)potDoble.lecturaAnteriorA) > UMBRAL_SALTO_ERRATICO) validaA = false;
    if (abs((int)valB - (int)potDoble.lecturaAnteriorB) > UMBRAL_SALTO_ERRATICO) validaB = false;
  }
  potDoble.primeraLectura = false;

  if (moviendo) {
    if (relInActivo) {
      if (valA > potDoble.lecturaAnteriorA + 5) validaA = false;
      if (valB > potDoble.lecturaAnteriorB + 5) validaB = false;
    } else if (relOutActivo) {
      if (valA < potDoble.lecturaAnteriorA - 5) validaA = false;
      if (valB < potDoble.lecturaAnteriorB - 5) validaB = false;
    }
  }

  potDoble.lecturaAnteriorA = valA;
  potDoble.lecturaAnteriorB = valB;

  if (!validaA || !diffOk) {
    if (!potDoble.pistaAFallada) {
      potDoble.pistaAFallada = true;
      logAccion("!!! PISTA A FALLADA !!!");
    }
  } else {
    potDoble.pistaAFallada = false;
  }

  if (!validaB || !diffOk) {
    if (!potDoble.pistaBFallada) {
      potDoble.pistaBFallada = true;
      logAccion("!!! PISTA B FALLADA !!!");
    }
  } else {
    potDoble.pistaBFallada = false;
  }

  ambasPistasFalladas = potDoble.pistaAFallada && potDoble.pistaBFallada;
  if (ambasPistasFalladas) {
    logAccion("*** CRITICO: AMBAS PISTAS FALLARON ***");
    if (estadoActual != ERROR_GRAVE) {
      entrarErrorGrave();
    }
  }

  if (potDoble.pistaActiva == 0 && potDoble.pistaAFallada) {
    if (!potDoble.pistaBFallada) {
      potDoble.pistaActiva = 1;
      potDoble.ultimoCambioPista = millis();
      logAccion("Conmutando a PISTA B");
    }
  } else if (potDoble.pistaActiva == 1 && potDoble.pistaBFallada) {
    if (!potDoble.pistaAFallada) {
      potDoble.pistaActiva = 0;
      potDoble.ultimoCambioPista = millis();
      logAccion("Conmutando a PISTA A");
    }
  }

  return !ambasPistasFalladas;
}

uint16_t obtenerLecturaSegura() {
  if (potDoble.pistaActiva == 0 && !potDoble.pistaAFallada) {
    return potDoble.lecturaA;
  }
  if (potDoble.pistaActiva == 1 && !potDoble.pistaBFallada) {
    return potDoble.lecturaB;
  }

  if (!potDoble.pistaAFallada) {
    potDoble.pistaActiva = 0;
    return potDoble.lecturaA;
  }
  if (!potDoble.pistaBFallada) {
    potDoble.pistaActiva = 1;
    return potDoble.lecturaB;
  }

  if (potDoble.lecturaEfectiva > 0 && potDoble.lecturaEfectiva < 1024) {
    return potDoble.lecturaEfectiva;
  }
  
  logAccion("*** Usando valor N por defecto ***");
  return posEfectiva(MARCHA_N);
}

uint16_t leerPot() {
  return potDoble.lecturaEfectiva;
}

// =============================================================================
// CONTROL DE POSICIÓN
// =============================================================================

bool enPosicion(uint16_t pos) {
  int16_t diff = (int16_t)potDoble.lecturaEfectiva - (int16_t)pos;
  return (diff >= -(int16_t)TOLERANCIA_ADC && diff <= (int16_t)TOLERANCIA_ADC);
}

void moverHacia(uint16_t pos) {
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

// =============================================================================
// EEPROM
// =============================================================================

void cargarPosiciones() {
  bool eepromValida = (EEPROM.read(EEPROM_FIRMA_ADDR) == EEPROM_FIRMA_VAL);
  
  if (eepromValida) {
    for (uint8_t i = 0; i < 4; i++) {
      EEPROM.get(EEPROM_POS_A_BASE + i * 2, posADC_A[i]);
      EEPROM.get(EEPROM_POS_B_BASE + i * 2, posADC_B[i]);
    }
    
    for (uint8_t i = 0; i < 4; i++) {
      if (posADC_A[i] == VALOR_CENTINELA) potDoble.pistaAFallada = true;
      if (posADC_B[i] == VALOR_CENTINELA) potDoble.pistaBFallada = true;
    }
    
    if (!potDoble.pistaAFallada) {
      potDoble.pistaActiva = 0;
    } else if (!potDoble.pistaBFallada) {
      potDoble.pistaActiva = 1;
    } else {
      logAccion("ERROR: Ambas pistas corruptas en EEPROM");
      entrarErrorGrave();
      return;
    }
    
    avisandoEEPROM = false;
  } else {
    uint16_t defaultsA[4] = {DEFAULT_POS_A_R, DEFAULT_POS_A_N, DEFAULT_POS_A_1, DEFAULT_POS_A_2};
    uint16_t defaultsB[4] = {DEFAULT_POS_B_R, DEFAULT_POS_B_N, DEFAULT_POS_B_1, DEFAULT_POS_B_2};
    
    for (uint8_t i = 0; i < 4; i++) {
      posADC_A[i] = defaultsA[i];
      posADC_B[i] = defaultsB[i];
      EEPROM.put(EEPROM_POS_A_BASE + i * 2, posADC_A[i]);
      EEPROM.put(EEPROM_POS_B_BASE + i * 2, posADC_B[i]);
    }
    EEPROM.write(EEPROM_FIRMA_ADDR, EEPROM_FIRMA_VAL);
    
    avisandoEEPROM    = true;
    tiempoAvisoEEPROM = millis();
  }
}

void guardarPosEnEEPROM(Marcha m, uint16_t valorA, uint16_t valorB) {
  posADC_A[m] = valorA;
  posADC_B[m] = valorB;
  
  EEPROM.put(EEPROM_POS_A_BASE + m * 2, valorA);
  EEPROM.put(EEPROM_POS_B_BASE + m * 2, valorB);
  EEPROM.write(EEPROM_FIRMA_ADDR, EEPROM_FIRMA_VAL);
  
  if (potDoble.pistaAFallada) {
    EEPROM.put(EEPROM_POS_A_BASE + m * 2, (uint16_t)VALOR_CENTINELA);
    posADC_A[m] = VALOR_CENTINELA;
  }
  if (potDoble.pistaBFallada) {
    EEPROM.put(EEPROM_POS_B_BASE + m * 2, (uint16_t)VALOR_CENTINELA);
    posADC_B[m] = VALOR_CENTINELA;
  }
}

// =============================================================================
// LEDs
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

void encenderLED(Marcha m) {
  apagarTodosLEDs();
  digitalWrite(pinLED(m), HIGH);
}

void manejarParpadeoSimple(uint8_t pin) {
  uint32_t ahora    = millis();
  uint16_t intervalo = ledEstado ? PARPADEO_ERROR_ON : PARPADEO_ERROR_OFF;
  if ((ahora - tiempoLed) >= intervalo) {
    ledEstado = !ledEstado;
    digitalWrite(pin, ledEstado ? HIGH : LOW);
    tiempoLed = ahora;
  }
}

void manejarParpadeoAprendizaje(uint8_t pin) {
  uint32_t ahora = millis();
  bool     encender;
  uint16_t intervalo;

  if (faseParpadeo < 6) {
    encender  = (faseParpadeo % 2 == 0);
    intervalo = encender ? PARPADEO_APREND_ON : PARPADEO_APREND_OFF;
  } else {
    encender  = false;
    intervalo = PARPADEO_APREND_PAUSA;
  }

  if ((ahora - tiempoLed) >= intervalo) {
    tiempoLed = ahora;
    faseParpadeo++;
    if (faseParpadeo > 6) faseParpadeo = 0;
    digitalWrite(pin, encender ? HIGH : LOW);
  }
}

void manejarParpadeoGrave() {
  uint32_t ahora    = millis();
  uint16_t intervalo = ledEstado ? PARPADEO_ERROR_ON : PARPADEO_ERROR_OFF;
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

void manejarParpadeoPotAlert() {
  uint32_t ahora = millis();
  
  if (ambasPistasFalladas) {
    digitalWrite(PIN_LED_POT_ALERT, HIGH);
    return;
  }
  
  if (potDoble.pistaAFallada || potDoble.pistaBFallada) {
    uint16_t intervalo = potAlertState ? PARPADEO_POT_ALERT_ON : PARPADEO_POT_ALERT_OFF;
    if ((ahora - tiempoPotAlert) >= intervalo) {
      potAlertState = !potAlertState;
      digitalWrite(PIN_LED_POT_ALERT, potAlertState ? HIGH : LOW);
      tiempoPotAlert = ahora;
    }
  } else {
    digitalWrite(PIN_LED_POT_ALERT, LOW);
    potAlertState = false;
  }
}
// =============================================================================
// ESTADOS
// =============================================================================

void estadoArranque() {
  uint32_t ahora = millis();

  if (avisandoEEPROM) {
    manejarParpadeoAprendizaje(PIN_LED_N);
    if ((ahora - tiempoAvisoEEPROM) >= PARPADEO_AVISO_EEPROM_MS) {
      avisandoEEPROM = false;
      apagarTodosLEDs();
      faseParpadeo = 0;
    }
  }

  if (enPosicion(posEfectiva(MARCHA_N))) {
    apagarActuador();
    marchaActual = MARCHA_N;
    encenderLED(MARCHA_N);
    bloqueoCambio = false;
    estadoActual = REPOSO;
    logPosicionAlcanzada(MARCHA_N);
//    logEstado();
    return;
  }

  moverHacia(posEfectiva(MARCHA_N));

  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    logAccion("TIMEOUT arranque");
    entrarErrorGrave();
  }
}

void estadoReposo() {
  uint32_t ahora = millis();

  if (bloqueoCambio && (ahora - tiempoBloqueo) >= BLOQUEO_MS)
    bloqueoCambio = false;

  if (bloqueoCambio)
    return;

  if (upDownSimultaneos() && marchaActual != MARCHA_N) {
    logAccion("EMERGENCIA: UP+DOWN -> N");
    marchaOrigen  = marchaActual;
    marchaDestino = MARCHA_N;
    tiempoInicio = ahora;
    activarK1();
    estadoActual = EMERGENCIA_A_N;
    return;
  }

  Marcha sig = marchaActual;

  if (btnUp.presionado) {
    switch (marchaActual) {
      case MARCHA_N: sig = MARCHA_1; break;
      case MARCHA_1: sig = MARCHA_2; break;
      case MARCHA_R: sig = MARCHA_N; break;
      case MARCHA_2:
      default:
        manejarParpadeoSimple(pinLED(marchaActual));
        return;
    }
    iniciarCambioA(sig);
    return;
  }

  if (btnDown.presionado) {
    switch (marchaActual) {
      case MARCHA_N: sig = MARCHA_R; break;
      case MARCHA_1: sig = MARCHA_N; break;
      case MARCHA_2: sig = MARCHA_1; break;
      case MARCHA_R:
      default:
        manejarParpadeoSimple(pinLED(marchaActual));
        return;
    }
    iniciarCambioA(sig);
    return;
  }
}

void iniciarCambioA(Marcha destino) {
  marchaOrigen  = marchaActual;
  marchaDestino = destino;
  tiempoInicio = millis();
  cambioCarrilPendiente = false;
  logCambioMarcha(marchaOrigen, marchaDestino);
  activarK1();
  estadoActual = ESPERANDO_FC_C;
//  logEstado();
}

void estadoEsperandoFCC() {
  if (digitalRead(PIN_FC_C) == HIGH) {
    if (millis() - tiempoInicio > TIMEOUT_MS) {
      logAccion("Timeout esperando FC_C");
      marchaDestino = MARCHA_N;
      tiempoInicio = millis();
      estadoActual = RECUPERANDO_A_N;
//      logEstado();
    }
    return;
  }

  logFCCambio("FC_C OK");
  tiempoInicio = millis();

  if (marchaDestino == MARCHA_R) {
    activarK2();
    cambioCarrilPendiente = true;
    moverHacia(posEfectiva(MARCHA_N));
  } else {
    cambioCarrilPendiente = false;
    moverHacia(posEfectiva(marchaDestino));
  }

  estadoActual = MOVIENDO;
//  logEstado();
}

void estadoMoviendo() {
  uint32_t ahora = millis();

  if (upDownSimultaneos()) {
    logAccion("EMERGENCIA durante MOVIENDO");
    apagarActuador();
    desactivarK2();
    marchaDestino = MARCHA_N;
    tiempoInicio = ahora;
    estadoActual = EMERGENCIA_A_N;
    return;
  }

  if (cambioCarrilPendiente) {
    moverHacia(posEfectiva(MARCHA_N));
    if (analogRead(PIN_FC_S) < 100) {
      logFCCambio("FC_S OK");
      cambioCarrilPendiente = false;
      tiempoInicio = ahora;
      moverHacia(posEfectiva(MARCHA_R));
    }
    if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
      logAccion("TIMEOUT esperando FC_S");
      apagarActuador();
      desactivarK2();
      marchaError = MARCHA_R;
      tiempoInicio = ahora;
      estadoActual = RECUPERANDO_A_N;
    }
    return;
  }

  moverHacia(posEfectiva(marchaDestino));

  if (enPosicion(posEfectiva(marchaDestino))) {
    apagarActuador();
    if (marchaActual == MARCHA_R && marchaDestino == MARCHA_N) {
      tiempoAux = ahora;
      estadoActual = ESPERA_FC_S_RETORNO;
//      logEstado();
      return;
    }
    desactivarK2();
    desactivarK1();
    marchaActual = marchaDestino;
    encenderLED(marchaActual);
    logPosicionAlcanzada(marchaActual);
    bloqueoCambio = true;
    tiempoBloqueo = ahora;
    estadoActual = REPOSO;
//    logEstado();
    return;
  }

  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    logAccion("TIMEOUT en MOVIENDO");
    apagarActuador();
    desactivarK2();
    marchaError = marchaDestino;
    tiempoInicio = ahora;
    estadoActual = RECUPERANDO_A_N;
//    logEstado();
  }
}

void estadoEsperaFCSRetorno() {
  uint32_t ahora = millis();

  if (analogRead(PIN_FC_S) > 900) {
    logFCCambio("FC_S retorno correcto");
    desactivarK1();
    marchaActual = MARCHA_N;
    encenderLED(MARCHA_N);
    bloqueoCambio = true;
    tiempoBloqueo = ahora;
    estadoActual = REPOSO;
    logPosicionAlcanzada(MARCHA_N);
//    logEstado();
    return;
  }

  if ((ahora - tiempoAux) >= 1000) {
    logAccion("ERROR: FC_S no retorno");
    entrarErrorGrave();
  }
}

void estadoEmergenciaN() {
  uint32_t ahora = millis();
  if (enPosicion(posEfectiva(MARCHA_N))) {
    apagarActuador();
    desactivarK1();
    desactivarK2();
    marchaActual  = MARCHA_N;
    encenderLED(MARCHA_N);
    bloqueoCambio = false;
    estadoActual  = REPOSO;
    logPosicionAlcanzada(MARCHA_N);
//    logEstado();
    logAccion("Emergencia completada");
    return;
  }
  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    logAccion("TIMEOUT en EMERGENCIA -> ERROR GRAVE");
    entrarErrorGrave();
    return;
  }
  moverHacia(posEfectiva(MARCHA_N));
}

void estadoRecuperandoN() {
  uint32_t ahora = millis();
  moverHacia(posEfectiva(MARCHA_N));

  if (enPosicion(posEfectiva(MARCHA_N))) {
    apagarActuador();
    desactivarK1();
    desactivarK2();
    marchaActual = MARCHA_N;
    apagarTodosLEDs();
    encenderLED(MARCHA_N);
    ledEstado = false;
    tiempoLed = ahora;
    estadoActual = ERROR_LEVE;
    logPosicionAlcanzada(MARCHA_N);
    logAccion("Recuperado en N - ERROR LEVE");
//    logEstado();
    return;
  }

  if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
    logAccion("TIMEOUT REC N");
    apagarActuador();
    desactivarK1();
    desactivarK2();
    entrarErrorGrave();
  }
}

void estadoErrorLeve() {
  uint32_t ahora = millis();

  manejarParpadeoSimple(pinLED(marchaError));

  if (upDownSimultaneos()) {
    logAccion("ERROR_LEVE reconocido con UP+DOWN");
    
    apagarTodosLEDs();
    
    if (marchaActual != MARCHA_N) {
      logAccion("UP+DOWN en ERROR_LEVE: yendo a N");
      marchaOrigen = marchaActual;
      marchaDestino = MARCHA_N;
      tiempoInicio = ahora;
      apagarActuador();
      activarK1();
      estadoActual = EMERGENCIA_A_N;
//      logEstado();
      return;
    } else {
      encenderLED(marchaActual);
      estadoActual = REPOSO;
      logAccion("ERROR_LEVE borrado - en N");
//      logEstado();
      return;
    }
  }

  if (bloqueoCambio && (ahora - tiempoBloqueo) >= BLOQUEO_MS)
    bloqueoCambio = false;
  if (bloqueoCambio) return;

  Marcha sig = marchaActual;

  if (btnUp.presionado) {
    switch (marchaActual) {
      case MARCHA_N: sig = MARCHA_1; break;
      case MARCHA_1: sig = MARCHA_2; break;
      case MARCHA_R: sig = MARCHA_N; break;
      case MARCHA_2:
      default: return;
    }
    iniciarCambioA(sig);
    return;
  }

  if (btnDown.presionado) {
    switch (marchaActual) {
      case MARCHA_N: sig = MARCHA_R; break;
      case MARCHA_1: sig = MARCHA_N; break;
      case MARCHA_2: sig = MARCHA_1; break;
      case MARCHA_R:
      default: return;
    }
    iniciarCambioA(sig);
    return;
  }
}

void estadoErrorGrave() {
  manejarParpadeoGrave();
}

void entrarErrorGrave() {
  apagarTodoReles();
  apagarTodosLEDs();
  digitalWrite(PIN_LED_POT_ALERT, HIGH);
  ledEstado    = false;
  tiempoLed    = millis();
  estadoActual = ERROR_GRAVE;
  ambasPistasFalladas = true;
  logAccion("ERROR GRAVE");
}

// =============================================================================
// MODO APRENDIZAJE
// =============================================================================

void estadoModoAprendizaje() {
  uint32_t ahora = millis();

  apagarTodosLEDs();
  manejarParpadeoAprendizaje(pinLED(marchaAprendizaje));

  if (btnUp.estadoActual && !btnUp.bloqueado &&
      (ahora - btnUp.tiempoPresion) >= TIEMPO_PULSACION_LARGA_MS) {
    btnUp.bloqueado = true;
    Marcha anterior = marchaAprendizaje;
    if      (marchaAprendizaje == MARCHA_2) marchaAprendizaje = MARCHA_1;
    else if (marchaAprendizaje == MARCHA_1) marchaAprendizaje = MARCHA_N;
    else if (marchaAprendizaje == MARCHA_N) marchaAprendizaje = MARCHA_R;
    if (marchaAprendizaje != anterior) {
      logAccion("Navegando a marcha");
      faseParpadeo  = 0;
      tiempoLed     = ahora;
      tiempoInicio  = ahora;
      estadoActual  = APRENDIZAJE_MOVIENDO;
      marchaDestino = marchaAprendizaje;
      moverHacia(posEfectiva(marchaAprendizaje));
    }
    return;
  }

  if (btnDown.estadoActual && !btnDown.bloqueado &&
      (ahora - btnDown.tiempoPresion) >= TIEMPO_PULSACION_LARGA_MS) {
    btnDown.bloqueado = true;
    Marcha anterior = marchaAprendizaje;
    if      (marchaAprendizaje == MARCHA_R) marchaAprendizaje = MARCHA_N;
    else if (marchaAprendizaje == MARCHA_N) marchaAprendizaje = MARCHA_1;
    else if (marchaAprendizaje == MARCHA_1) marchaAprendizaje = MARCHA_2;
    if (marchaAprendizaje != anterior) {
      logAccion("Navegando a marcha");
      faseParpadeo  = 0;
      tiempoLed     = ahora;
      tiempoInicio  = ahora;
      estadoActual  = APRENDIZAJE_MOVIENDO;
      marchaDestino = marchaAprendizaje;
      moverHacia(posEfectiva(marchaAprendizaje));
    }
    return;
  }

  if (btnUp.presionado) {
    logAccion("Aprendizaje: paso IN");
    tiempoInicio  = ahora;
    estadoActual  = APRENDIZAJE_MOVIENDO;
    marchaDestino = MARCHA_R;
    activarReleIn();
    return;
  }

  if (btnDown.presionado) {
    logAccion("Aprendizaje: paso OUT");
    tiempoInicio  = ahora;
    estadoActual  = APRENDIZAJE_MOVIENDO;
    marchaDestino = MARCHA_2;
    activarReleOut();
    return;
  }

  if (btnConf.presionado) {
    uint16_t valA = potDoble.lecturaA;
    uint16_t valB = potDoble.lecturaB;
    if (potDoble.pistaAFallada) {
      valA = VALOR_CENTINELA;
    }
    if (potDoble.pistaBFallada) {
      valB = VALOR_CENTINELA;
    }
    guardarPosEnEEPROM(marchaAprendizaje, valA, valB);
    apagarTodosLEDs();
    digitalWrite(pinLED(marchaAprendizaje), HIGH);
    tiempoAux    = ahora;
    estadoActual = APRENDIZAJE_CONFIRMANDO;
    logAccion("Posicion guardada en EEPROM");
  }
}

void estadoAprendizajeConfirmando() {
  uint32_t ahora = millis();
  if ((ahora - tiempoAux) >= PARPADEO_CONFIRM_MS) {
    apagarTodosLEDs();
    faseParpadeo = 0;
    tiempoLed    = ahora;
    estadoActual = MODO_APRENDIZAJE;
  }
}

void estadoAprendizajeMoviendo() {
  uint32_t ahora = millis();

  apagarTodosLEDs();
  manejarParpadeoAprendizaje(pinLED(marchaAprendizaje));

  bool esNavegacion = (marchaDestino == marchaAprendizaje);

  if (esNavegacion) {
    if (enPosicion(posEfectiva(marchaAprendizaje))) {
      apagarActuador();
      logPosicionAlcanzada(marchaAprendizaje);
      estadoActual = MODO_APRENDIZAJE;
      return;
    }
    if ((ahora - tiempoInicio) >= TIMEOUT_MS) {
      apagarActuador();
      logAccion("Aprendizaje: timeout navegacion");
      estadoActual = MODO_APRENDIZAJE;
      return;
    }
    moverHacia(posEfectiva(marchaAprendizaje));
  } else {
    if ((ahora - tiempoInicio) >= PASO_APRENDIZAJE_MS) {
      apagarActuador();
      logAccion("Aprendizaje: paso fin");
      estadoActual = MODO_APRENDIZAJE;
    }
  }
}
