from pathlib import Path

SRC = Path('Proyecto_trike_v8.ino')
DST = Path('Proyecto_trike_v8_3.ino')

code = SRC.read_text(encoding='utf-8')

# -----------------------------------------------------------------------------
# Header / includes
# -----------------------------------------------------------------------------
code = code.replace(
    'SELECTOR DE MARCHAS VW AUTOSTICK — V8.2.5Intercambio de pin K1(D13) por Led N(D8)',
    'SELECTOR DE MARCHAS VW AUTOSTICK — V8.3 — INTERFAZ DE PRUEBAS SERIE',
    1,
)
code = code.replace(
    'Base: v8.2.4 — Redundancia mejorada',
    'Base: v8.2.5 — Interfaz de pruebas serie para calibración',
    1,
)

for inc in ('#include <string.h>', '#include <stdlib.h>'):
    if inc not in code:
        code = code.replace('#include <EEPROM.h>\n', '#include <EEPROM.h>\n' + inc + '\n', 1)

# -----------------------------------------------------------------------------
# Test globals
# -----------------------------------------------------------------------------
global_anchor = '// =============================================================================\n// PROTOTIPOS\n// ============================================================================='
test_globals = r'''
// =============================================================================
// VARIABLES DE PRUEBAS SERIE
// =============================================================================

enum TipoMovimientoPrueba {
  PRUEBA_MOVIMIENTO_NINGUNO,
  PRUEBA_MOVIMIENTO_ADC_A,
  PRUEBA_MOVIMIENTO_ADC_B,
  PRUEBA_MOVIMIENTO_TIEMPO
};

TipoMovimientoPrueba tipoMovimientoPrueba = PRUEBA_MOVIMIENTO_NINGUNO;
bool movimientoPruebaActivo = false;
uint16_t objetivoADCPrueba = 0;
bool direccionMovimientoPruebaOut = false;
uint32_t inicioMovimientoPrueba = 0;
uint32_t duracionMovimientoPrueba = 0;

bool pruebaGActiva = false;
bool pruebaGDesdeAprendizaje = false;

bool pruebaK1Temporizada = false;
uint32_t finPruebaK1 = 0;

bool pruebaK2Temporizada = false;
uint32_t finPruebaK2 = 0;

char bufferComandoSerial[64];
uint8_t indiceBufferComandoSerial = 0;
'''
if 'VARIABLES DE PRUEBAS SERIE' not in code:
    if global_anchor not in code:
        raise SystemExit('No se encontró el ancla de prototipos')
    code = code.replace(global_anchor, test_globals + '\n' + global_anchor, 1)

# -----------------------------------------------------------------------------
# Test prototypes
# -----------------------------------------------------------------------------
proto_anchor = 'void procesarComandoSerial();\n'
test_prototypes = r'''void procesarComandoSerial();
void imprimirAyudaSerie();
void imprimirPosicionesEEPROM();
bool esEstadoAprendizaje(Estado estado);
void cancelarPruebaMovimiento();
void cancelarManiobraNormalParaPrueba();
void iniciarPruebaADC(uint8_t pista, uint16_t objetivo);
void iniciarPruebaMovimientoTiempo(bool haciaOut, uint32_t duracion);
void gestionarMovimientoPrueba();
void gestionarPruebaK1();
void gestionarPruebaK2();
void iniciarPruebaMarcha(Marcha destino);
void gestionarRetornoDePruebaMarcha();
'''
if 'void imprimirAyudaSerie();' not in code:
    if proto_anchor not in code:
        raise SystemExit('No se encontró el prototipo serie')
    code = code.replace(proto_anchor, test_prototypes, 1)

# -----------------------------------------------------------------------------
# Loop
# -----------------------------------------------------------------------------
loop_start = code.index('void loop() {')
loop_end = code.index('// =============================================================================\n// BOTONES', loop_start)
new_loop = r'''void loop() {
  uint32_t ahora = millis();

  gestionarRetardoRele();
  gestionarPruebaK1();
  gestionarPruebaK2();

  if ((ahora - potDoble.ultimaVerificacion) >= TIEMPO_VERIFICACION_POT) {
    potDoble.ultimaVerificacion = ahora;
    leerPotenciometro();
    verificarPistas();
    potDoble.lecturaEfectiva = obtenerLecturaSegura();
  }

  manejarParpadeoPotAlert();

  // Los botones se muestrean antes del puerto serie. Dentro de un mismo ciclo,
  // una orden serie posterior puede sustituir una orden de botón anterior.
  leerBotones();
  procesarComandoSerial();

  // Una orden UP/DOWN posterior a una prueba de movimiento cancela la prueba.
  if (movimientoPruebaActivo && (btnUp.presionado || btnDown.presionado)) {
    cancelarPruebaMovimiento();
  }

  if (movimientoPruebaActivo) {
    gestionarMovimientoPrueba();
  }

  bool modoAprendizajeActivo = (digitalRead(PIN_BTN_MODO) == LOW);

  // G utiliza la máquina normal de maniobras, incluso cuando el aprendizaje
  // estaba activo. Al terminar correctamente, se vuelve al aprendizaje.
  if (pruebaGActiva) {
    vigilarK2();

    switch (estadoActual) {
      case ESPERANDO_FC_C: estadoEsperandoFCC(); break;
      case MANIOBRA:       ejecutarManiobra(); break;
      case REPOSO:         estadoReposo(); break;
      case ERROR_GRAVE:    estadoErrorGrave(); break;
      default:
        estadoActual = ESPERANDO_FC_C;
        break;
    }

    gestionarRetornoDePruebaMarcha();

    if (!movimientoPruebaActivo && estadoActual == REPOSO && !pruebaGActiva) {
      gestionarLEDsNormal();
    }
    return;
  }

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
    movimientoPruebaActivo = false;
    tipoMovimientoPrueba = PRUEBA_MOVIMIENTO_NINGUNO;
    pruebaGActiva = false;
    pruebaGDesdeAprendizaje = false;
    estadoActual = ARRANQUE;
    tiempoInicio = ahora;
    return;
  }

  if (movimientoPruebaActivo) {
    return;
  }

  vigilarK2();

  switch (estadoActual) {
    case ARRANQUE:                         estadoArranque(); break;
    case REPOSO:                           estadoReposo(); break;
    case ESPERANDO_FC_C:                   estadoEsperandoFCC(); break;
    case MANIOBRA:                         ejecutarManiobra(); break;
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

'''
code = code[:loop_start] + new_loop + code[loop_end:]

# -----------------------------------------------------------------------------
# Automatic manoeuvre starts cancel only a K1 timed test, allowing automatic
# manoeuvres to take K1 back under normal control.
# -----------------------------------------------------------------------------
old_start = 'void iniciarCambioA(Marcha destino) {\n'
if old_start not in code:
    raise SystemExit('No se encontró iniciarCambioA')
code = code.replace(old_start, old_start + '  pruebaK1Temporizada = false;\n', 1)

# -----------------------------------------------------------------------------
# STATUS extension
# -----------------------------------------------------------------------------
status_anchor = '  DBG(F("OUT: ")); DBGLN_VAL(relOutActivo ? F("ON") : F("OFF"));\n'
status_extra = r'''

  DBG(F("Prueba movimiento: "));
  if (!movimientoPruebaActivo) {
    DBGLN(F("OFF"));
  } else if (tipoMovimientoPrueba == PRUEBA_MOVIMIENTO_ADC_A) {
    DBG(F("ADC A -> ")); DBGLN_VAL(objetivoADCPrueba);
  } else if (tipoMovimientoPrueba == PRUEBA_MOVIMIENTO_ADC_B) {
    DBG(F("ADC B -> ")); DBGLN_VAL(objetivoADCPrueba);
  } else {
    DBG(F("MOVE "));
    DBG_VAL(direccionMovimientoPruebaOut ? F("OUT") : F("IN"));
    DBGLN(F(" activo"));
  }

  DBG(F("Prueba G: ")); DBGLN_VAL(pruebaGActiva ? F("ON") : F("OFF"));
  DBG(F("Prueba K1 temporizada: ")); DBGLN_VAL(pruebaK1Temporizada ? F("ON") : F("OFF"));
  DBG(F("Prueba K2 temporizada: ")); DBGLN_VAL(pruebaK2Temporizada ? F("ON") : F("OFF"));
'''
if status_anchor not in code:
    raise SystemExit('No se encontró ancla STATUS')
code = code.replace(status_anchor, status_anchor + status_extra, 1)

# -----------------------------------------------------------------------------
# Replace diagnostic serial parser section with V8.3 implementation.
# -----------------------------------------------------------------------------
serial_start = code.index('void procesarComandoSerial() {')
serial_end = code.index('// =============================================================================\n// APRENDIZAJE', serial_start)
serial_block = r'''void imprimirAyudaSerie() {
#if DEBUG
  DBGLN(F(""));
  DBGLN(F("=== COMANDOS PRUEBA SERIE V8.3 ==="));
  DBGLN(F("HELP        - Mostrar todos los comandos"));
  DBGLN(F("STATUS      - Estado completo del sistema"));
  DBGLN(F("POS         - Posiciones EEPROM A/B"));
  DBGLN(F("RESET / R   - Reset de contadores de diagnostico"));
  DBGLN(F("STOP        - Detener prueba/maniobra y apagar K1/K2/IN/OUT"));
  DBGLN(F("ADC A x     - Mover a ADC x gobernado por pista A"));
  DBGLN(F("ADC B x     - Mover a ADC x gobernado por pista B"));
  DBGLN(F("G R|N|1|2   - Ir a la marcha indicada usando la logica normal"));
  DBGLN(F("MOVE IN x   - Activar IN durante x ms"));
  DBGLN(F("MOVE OUT x  - Activar OUT durante x ms"));
  DBGLN(F("K1 x        - Activar K1 durante x ms"));
  DBGLN(F("K1 ON       - K1 ON indefinido"));
  DBGLN(F("K1 OFF      - K1 OFF"));
  DBGLN(F("K2 x        - Activar K2 durante x ms (max 3000 ms)"));
  DBGLN(F("K2 ON       - K2 ON (max 3000 ms)"));
  DBGLN(F("K2 OFF      - K2 OFF"));
  DBGLN(F("Comandos sin distincion de mayusculas/minusculas."));
  DBGLN(F("Finalizar cada comando con ENTER."));
  DBGLN(F("===================================="));
#endif
}

void imprimirPosicionesEEPROM() {
#if DEBUG
  DBGLN(F(""));
  DBGLN(F("=== POSICIONES EEPROM ==="));
  DBGLN(F("Marcha     A      B"));
  DBG(F("R       ")); DBG_VAL(posADC_A[MARCHA_R]); DBG(F("   ")); DBGLN_VAL(posADC_B[MARCHA_R]);
  DBG(F("N       ")); DBG_VAL(posADC_A[MARCHA_N]); DBG(F("   ")); DBGLN_VAL(posADC_B[MARCHA_N]);
  DBG(F("1       ")); DBG_VAL(posADC_A[MARCHA_1]); DBG(F("   ")); DBGLN_VAL(posADC_B[MARCHA_1]);
  DBG(F("2       ")); DBG_VAL(posADC_A[MARCHA_2]); DBG(F("   ")); DBGLN_VAL(posADC_B[MARCHA_2]);
  DBGLN(F("========================"));
#endif
}

bool esEstadoAprendizaje(Estado estado) {
  return estado == MODO_APRENDIZAJE ||
         estado == APRENDIZAJE_IR_A_2 ||
         estado == APRENDIZAJE_MOVIENDO ||
         estado == APRENDIZAJE_ESPERANDO_FC_C ||
         estado == APRENDIZAJE_ESPERANDO_FC_S ||
         estado == APRENDIZAJE_RECUPERANDO ||
         estado == APRENDIZAJE_CONFIRMANDO;
}

bool parseUnsignedLongExact(const char *texto, uint32_t &valor) {
  if (texto == nullptr || *texto == '\0') return false;

  char *fin = nullptr;
  unsigned long resultado = strtoul(texto, &fin, 10);
  if (*fin != '\0') return false;

  valor = (uint32_t)resultado;
  return true;
}

void cancelarManiobraNormalParaPrueba() {
  apagarActuador();
  desactivarK2();
  timeoutK2 = false;
  ventanaOrdenActiva = false;
  tiempoInicioVentanaOrden = 0;
  marchaDestinoPendiente = marchaDestino;
  marchaDestino = marchaActual;
  estadoSubmaniobra = MANIOBRA_INICIO;
  tiempoInicio = millis();
}

void cancelarPruebaMovimiento() {
  apagarActuador();
  movimientoPruebaActivo = false;
  tipoMovimientoPrueba = PRUEBA_MOVIMIENTO_NINGUNO;
  objetivoADCPrueba = 0;
  direccionMovimientoPruebaOut = false;
  inicioMovimientoPrueba = 0;
  duracionMovimientoPrueba = 0;

  if (esEstadoAprendizaje(estadoActual)) {
    aprendizajeMovimientoAutomatico = false;
    estadoActual = MODO_APRENDIZAJE;
  } else if (estadoActual != ERROR_GRAVE) {
    cancelarManiobraNormalParaPrueba();
    estadoActual = REPOSO;
  }
}

void iniciarPruebaADC(uint8_t pista, uint16_t objetivo) {
  if (pista > 1 || objetivo > 1023) {
    DBGLN(F("ERROR: ADC fuera de rango 0..1023"));
    return;
  }

  cancelarPruebaMovimiento();

  if (esEstadoAprendizaje(estadoActual)) {
    aprendizajeMovimientoAutomatico = false;
    estadoActual = MODO_APRENDIZAJE;
  } else if (estadoActual != ERROR_GRAVE) {
    estadoActual = REPOSO;
  }

  movimientoPruebaActivo = true;
  tipoMovimientoPrueba =
    (pista == 0) ? PRUEBA_MOVIMIENTO_ADC_A : PRUEBA_MOVIMIENTO_ADC_B;
  objetivoADCPrueba = objetivo;
  direccionMovimientoPruebaOut = false;
  inicioMovimientoPrueba = millis();
  duracionMovimientoPrueba = 0;

  DBG(F("PRUEBA ADC "));
  DBG_VAL(pista == 0 ? F("A") : F("B"));
  DBG(F(" OBJ="));
  DBGLN_VAL(objetivo);
}

void iniciarPruebaMovimientoTiempo(bool haciaOut, uint32_t duracion) {
  if (duracion == 0) {
    DBGLN(F("ERROR: duracion debe ser > 0"));
    return;
  }

  cancelarPruebaMovimiento();

  if (esEstadoAprendizaje(estadoActual)) {
    aprendizajeMovimientoAutomatico = false;
    estadoActual = MODO_APRENDIZAJE;
  } else if (estadoActual != ERROR_GRAVE) {
    estadoActual = REPOSO;
  }

  movimientoPruebaActivo = true;
  tipoMovimientoPrueba = PRUEBA_MOVIMIENTO_TIEMPO;
  objetivoADCPrueba = 0;
  direccionMovimientoPruebaOut = haciaOut;
  inicioMovimientoPrueba = millis();
  duracionMovimientoPrueba = duracion;

  DBG(F("PRUEBA MOVE "));
  DBG_VAL(haciaOut ? F("OUT") : F("IN"));
  DBG(F(" MS="));
  DBGLN_VAL(duracion);
}

void gestionarMovimientoPrueba() {
  if (!movimientoPruebaActivo) return;

  if (tipoMovimientoPrueba == PRUEBA_MOVIMIENTO_TIEMPO) {
    if ((millis() - inicioMovimientoPrueba) >= duracionMovimientoPrueba) {
      apagarActuador();
      movimientoPruebaActivo = false;
      tipoMovimientoPrueba = PRUEBA_MOVIMIENTO_NINGUNO;
      direccionMovimientoPruebaOut = false;
      DBGLN(F("PRUEBA MOVE FIN"));
      return;
    }

    // Reintentar la activacion cada ciclo permite completar el tiempo muerto
    // de 150 ms cuando la prueba invierte el sentido del actuador.
    if (direccionMovimientoPruebaOut) {
      activarReleOut();
    } else {
      activarReleIn();
    }
    return;
  }

  uint16_t lectura =
    (tipoMovimientoPrueba == PRUEBA_MOVIMIENTO_ADC_A)
      ? potDoble.lecturaA
      : potDoble.lecturaB;

  int16_t diferencia =
    (int16_t)lectura - (int16_t)objetivoADCPrueba;

  if (diferencia >= -(int16_t)TOLERANCIA_ADC &&
      diferencia <=  (int16_t)TOLERANCIA_ADC) {
    apagarActuador();
    movimientoPruebaActivo = false;
    tipoMovimientoPrueba = PRUEBA_MOVIMIENTO_NINGUNO;

    DBG(F("PRUEBA ADC FIN | P="));
    DBGLN_VAL(lectura);
    return;
  }

  if (lectura < objetivoADCPrueba) {
    activarReleOut();
  } else {
    activarReleIn();
  }
}

void gestionarPruebaK1() {
  if (!pruebaK1Temporizada) return;

  if ((int32_t)(millis() - finPruebaK1) >= 0) {
    pruebaK1Temporizada = false;
    desactivarK1();
    DBGLN(F("PRUEBA K1 FIN"));
  }
}

void gestionarPruebaK2() {
  if (!pruebaK2Temporizada) return;

  if ((int32_t)(millis() - finPruebaK2) >= 0) {
    pruebaK2Temporizada = false;
    desactivarK2();
    timeoutK2 = false;
    DBGLN(F("PRUEBA K2 FIN"));
  }
}

void iniciarPruebaMarcha(Marcha destino) {
  if (estadoActual == ERROR_GRAVE) {
    DBGLN(F("ERROR: G no disponible durante ERROR_GRAVE"));
    return;
  }

  cancelarPruebaMovimiento();
  pruebaK1Temporizada = false;
  pruebaGActiva = true;
  pruebaGDesdeAprendizaje = esEstadoAprendizaje(estadoActual);

  if (pruebaGDesdeAprendizaje) {
    aprendizajeMovimientoAutomatico = false;
    estadoActual = MODO_APRENDIZAJE;
  }

  pruebaK2Temporizada = false;
  marchaDestinoPendiente = marchaDestino;
  ventanaOrdenActiva = false;
  estadoSubmaniobra = MANIOBRA_INICIO;
  timeoutK2 = false;

  if (destino == marchaActual) {
    DBG(F("G ")); DBGLN_VAL(nombreMarcha(destino));
    DBG(F("YA EN ")); DBGLN_VAL(nombreMarcha(destino));
    pruebaGActiva = false;
    if (pruebaGDesdeAprendizaje) {
      estadoActual = MODO_APRENDIZAJE;
    } else {
      estadoActual = REPOSO;
    }
    pruebaGDesdeAprendizaje = false;
    return;
  }

  iniciarCambioA(destino);
}

void gestionarRetornoDePruebaMarcha() {
  if (!pruebaGActiva) return;

  if (estadoActual == ERROR_GRAVE) {
    pruebaGActiva = false;
    pruebaGDesdeAprendizaje = false;
    return;
  }

  if (estadoActual == REPOSO) {
    pruebaGActiva = false;

    if (pruebaGDesdeAprendizaje) {
      estadoActual = MODO_APRENDIZAJE;
    }

    pruebaGDesdeAprendizaje = false;
  }
}

void procesarComandoSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();

    if (c == '\r' || c == '\n') {
      if (indiceBufferComandoSerial == 0) continue;

      bufferComandoSerial[indiceBufferComandoSerial] = '\0';

      for (uint8_t i = 0; i < indiceBufferComandoSerial; i++) {
        if (bufferComandoSerial[i] >= 'a' && bufferComandoSerial[i] <= 'z') {
          bufferComandoSerial[i] -= ('a' - 'A');
        }
      }

      char *cmd = strtok(bufferComandoSerial, " \t");
      char *arg1 = strtok(nullptr, " \t");
      char *arg2 = strtok(nullptr, " \t");
      char *extra = strtok(nullptr, " \t");

      if (cmd == nullptr || extra != nullptr) {
        DBGLN(F("ERROR: COMANDO INVALIDO. USE HELP"));
        indiceBufferComandoSerial = 0;
        continue;
      }

      if (strcmp(cmd, "HELP") == 0) {
        if (arg1 != nullptr) {
          DBGLN(F("ERROR: HELP NO USA PARAMETROS"));
        } else {
          imprimirAyudaSerie();
        }

      } else if (strcmp(cmd, "STATUS") == 0) {
        if (arg1 != nullptr) {
          DBGLN(F("ERROR: STATUS NO USA PARAMETROS"));
        } else {
          imprimirDiagnosticoSistema();
        }

      } else if (strcmp(cmd, "POS") == 0) {
        if (arg1 != nullptr) {
          DBGLN(F("ERROR: POS NO USA PARAMETROS"));
        } else {
          imprimirPosicionesEEPROM();
        }

      } else if (strcmp(cmd, "STOP") == 0) {
        if (arg1 != nullptr) {
          DBGLN(F("ERROR: STOP NO USA PARAMETROS"));
        } else {
          movimientoPruebaActivo = false;
          tipoMovimientoPrueba = PRUEBA_MOVIMIENTO_NINGUNO;
          objetivoADCPrueba = 0;
          direccionMovimientoPruebaOut = false;
          inicioMovimientoPrueba = 0;
          duracionMovimientoPrueba = 0;

          pruebaGActiva = false;
          pruebaGDesdeAprendizaje = false;

          pruebaK1Temporizada = false;
          pruebaK2Temporizada = false;

          apagarTodoReles();
          timeoutK2 = false;
          ventanaOrdenActiva = false;
          tiempoInicioVentanaOrden = 0;
          marchaDestino = marchaActual;
          marchaDestinoPendiente = marchaActual;
          estadoSubmaniobra = MANIOBRA_INICIO;
          aprendizajeMovimientoAutomatico = false;

          if (digitalRead(PIN_BTN_MODO) == LOW) {
            estadoActual = MODO_APRENDIZAJE;
          } else if (estadoActual != ERROR_GRAVE) {
            estadoActual = REPOSO;
          }

          DBGLN(F("STOP OK"));
        }

      } else if (strcmp(cmd, "RESET") == 0 || strcmp(cmd, "R") == 0) {
        if (arg1 != nullptr) {
          DBGLN(F("ERROR: RESET NO USA PARAMETROS"));
        } else {
          potDoble.contadorFalloA = 0;
          potDoble.contadorFalloB = 0;
          potDoble.contadorCongeladoA = 0;
          potDoble.contadorCongeladoB = 0;
          contadorDiscrepanciaPistas = 0;

          leerPotenciometro();
          potDoble.lecturaEfectiva = obtenerLecturaSegura();

          logAccion(F("RESET diagnostico: fallos latched NO borrados"));
        }

      } else if (strcmp(cmd, "ADC") == 0) {
        uint32_t valor;
        if (arg1 == nullptr || arg2 == nullptr ||
            strlen(arg1) != 1 || (arg1[0] != 'A' && arg1[0] != 'B')) {
          DBGLN(F("ERROR: USO ADC A x / ADC B x"));
        } else if (!parseUnsignedLongExact(arg2, valor) || valor > 1023UL) {
          DBGLN(F("ERROR: ADC x DEBE SER 0..1023"));
        } else {
          iniciarPruebaADC(arg1[0] == 'A' ? 0 : 1, (uint16_t)valor);
        }

      } else if (strcmp(cmd, "G") == 0) {
        if (arg1 == nullptr || strlen(arg1) != 1) {
          DBGLN(F("ERROR: USO G R|N|1|2"));
        } else if (arg1[0] == 'R') {
          iniciarPruebaMarcha(MARCHA_R);
        } else if (arg1[0] == 'N') {
          iniciarPruebaMarcha(MARCHA_N);
        } else if (arg1[0] == '1') {
          iniciarPruebaMarcha(MARCHA_1);
        } else if (arg1[0] == '2') {
          iniciarPruebaMarcha(MARCHA_2);
        } else {
          DBGLN(F("ERROR: MARCHA DEBE SER R, N, 1 O 2"));
        }

      } else if (strcmp(cmd, "MOVE") == 0) {
        uint32_t duracion;
        if (arg1 == nullptr || arg2 == nullptr ||
            (strcmp(arg1, "IN") != 0 && strcmp(arg1, "OUT") != 0)) {
          DBGLN(F("ERROR: USO MOVE IN x / MOVE OUT x"));
        } else if (!parseUnsignedLongExact(arg2, duracion) || duracion == 0) {
          DBGLN(F("ERROR: x DEBE SER > 0"));
        } else {
          iniciarPruebaMovimientoTiempo(
            strcmp(arg1, "OUT") == 0, duracion);
        }

      } else if (strcmp(cmd, "K1") == 0) {
        if (arg1 == nullptr) {
          DBGLN(F("ERROR: USO K1 ON|OFF|x"));
        } else if (strcmp(arg1, "ON") == 0) {
          pruebaK1Temporizada = false;
          activarK1();
        } else if (strcmp(arg1, "OFF") == 0) {
          pruebaK1Temporizada = false;
          desactivarK1();
        } else {
          uint32_t duracion;
          if (!parseUnsignedLongExact(arg1, duracion) || duracion == 0) {
            DBGLN(F("ERROR: USO K1 ON|OFF|x"));
          } else {
            pruebaK1Temporizada = true;
            finPruebaK1 = millis() + duracion;
            activarK1();
            DBG(F("PRUEBA K1 MS=")); DBGLN_VAL(duracion);
          }
        }

      } else if (strcmp(cmd, "K2") == 0) {
        if (arg1 == nullptr) {
          DBGLN(F("ERROR: USO K2 ON|OFF|x"));
        } else if (strcmp(arg1, "ON") == 0) {
          pruebaK2Temporizada = false;
          activarK2();
        } else if (strcmp(arg1, "OFF") == 0) {
          pruebaK2Temporizada = false;
          desactivarK2();
          timeoutK2 = false;
        } else {
          uint32_t duracion;
          if (!parseUnsignedLongExact(arg1, duracion) || duracion == 0) {
            DBGLN(F("ERROR: USO K2 ON|OFF|x"));
          } else {
            if (duracion > (uint32_t)TIEMPO_MAX_K2) {
              duracion = TIEMPO_MAX_K2;
            }
            pruebaK2Temporizada = true;
            finPruebaK2 = millis() + duracion;
            timeoutK2 = false;
            activarK2();
            DBG(F("PRUEBA K2 MS=")); DBGLN_VAL(duracion);
          }
        }

      } else {
        DBGLN(F("ERROR: COMANDO DESCONOCIDO. USE HELP"));
      }

      indiceBufferComandoSerial = 0;
      continue;
    }

    if (indiceBufferComandoSerial >= sizeof(bufferComandoSerial) - 1) {
      indiceBufferComandoSerial = 0;
      DBGLN(F("ERROR: COMANDO DEMASIADO LARGO"));
      continue;
    }

    bufferComandoSerial[indiceBufferComandoSerial++] = c;
  }
}

'''
code = code[:serial_start] + serial_block + code[serial_end:]

# -----------------------------------------------------------------------------
# Final sanity checks
# -----------------------------------------------------------------------------
required = [
    'SELECTOR DE MARCHAS VW AUTOSTICK — V8.3',
    'void imprimirAyudaSerie()',
    'void imprimirPosicionesEEPROM()',
    'void procesarComandoSerial()',
    'ADC A x',
    'G R|N|1|2',
    'K1 ON',
    'K2 ON',
    'STATUS',
]
for token in required:
    if token not in code:
        raise SystemExit(f'Falta elemento esperado: {token}')

DST.write_text(code, encoding='utf-8')
print(f'Generado {DST} ({DST.stat().st_size} bytes)')
