#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>


#define enfriar 4
#define calentar 2 
#define on_enfriar digitalWrite(enfriar, HIGH)
#define off_enfriar digitalWrite(enfriar, LOW)
#define on_calentar digitalWrite(calentar, HIGH)
#define off_calentar digitalWrite(calentar, LOW)

// -------- LCD I2C 16x2 --------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------- LM35 --------
// Vout al GPIO 34 (solo entrada), +Vs a 5 V (VIN), GND común
const uint8_t LM35_PIN = 34;

float leerTemperatura() {
  long suma = 0;
  for (int i = 0; i < 16; i++) {
    suma += analogReadMilliVolts(LM35_PIN);
    delay(2);
  }
  float mV = suma / 16.0;
  return mV / 10.0;   // 10 mV por °C
}

// -------- KEYPAD 4x4 --------
const byte FILAS = 4;
const byte COLUMNAS = 4;

char teclas[FILAS][COLUMNAS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte pinesFilas[FILAS] = {13, 14, 16, 17};
byte pinesColumnas[COLUMNAS] = {18, 19, 23, 25};

Keypad teclado = Keypad(makeKeymap(teclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

// -------- LIMITES Y EDICION --------
float HL = 40;            // límite alto: enfría si temp >= HL
float LL = 30;            // límite bajo: calienta si temp <= LL
char editando = 'A';      // 'A' = editando HL, 'B' = editando LL
String entradaHL = "";
String entradaLL = "";
const size_t MAX_CARACTERES = 4;

String& entradaActual() {
  return (editando == 'C') ? entradaHL : entradaLL;
}

// -------- MUESTREO --------
unsigned long ultimoMuestreo = 0;
const unsigned long INTERVALO_SENSOR_MS = 1000;
float temperaturaC = NAN;

void controlarSalidas() {
  if (temperaturaC >= HL) {
    on_enfriar;
    off_calentar;
  }

  if (temperaturaC <= LL) {
    on_calentar;
    off_enfriar;
  }
  // if (isnan(temperaturaC)) {          // sin lectura: todo apagado por seguridad
  //   off_enfriar;
  //   off_calentar;
  // } else if (temperaturaC >= HL) {
  //   on_enfriar;
  //   off_calentar;
  // } else if (temperaturaC <= LL) {
  //   off_enfriar;
  //   on_calentar;
  // } else {
  //   off_enfriar;
  //   off_calentar;
  // }
}

void actualizarLCD() {
  // Línea 1: temperatura
  lcd.setCursor(0, 0);
  if (isnan(temperaturaC)) {
    lcd.print("Temp: --.- C    ");
  } else {
    char tempTexto[17];
    snprintf(tempTexto, sizeof(tempTexto), "Temp: %5.1f C", temperaturaC);
    lcd.print(tempTexto);
    int longitud = strlen(tempTexto);
    while (longitud++ < 16) lcd.print(' ');
  }

  // Línea 2: HL (izquierda) y LL (derecha); ">" marca cuál se edita
  String textoHL = (editando == 'C' && entradaHL.length() > 0) ? entradaHL : String((int)HL);
  String textoLL = (editando == 'B' && entradaLL.length() > 0) ? entradaLL : String((int)LL);

  lcd.setCursor(0, 1);
  lcd.print(editando == 'C' ? "HL>" : "HL:");
  lcd.print(textoHL);
  for (size_t i = textoHL.length(); i < 5; i++) lcd.print(' ');

  lcd.setCursor(8, 1);
  lcd.print(editando == 'B' ? "LL>" : "LL:");
  lcd.print(textoLL);
  for (size_t i = textoLL.length(); i < 5; i++) lcd.print(' ');
}

void confirmarEntrada() {
  String& e = entradaActual();

  if (e.length() == 0) {
    lcd.setCursor(0, 1);
    lcd.print("Entrada vacia   ");
    delay(800);
  } else {
    float nuevo = e.toFloat();
    bool valido = (editando == 'C') ? (nuevo > LL) : (nuevo < HL);

    lcd.setCursor(0, 1);
    if (valido) {
      if (editando == 'C') HL = nuevo; else LL = nuevo;
      lcd.print("Guardado        ");
      Serial.printf("HL=%.1f  LL=%.1f\n", HL, LL);
    } else {
      lcd.print("HL debe ser>LL  ");
    }
    delay(800);
  }

  e = "";
  controlarSalidas();
  actualizarLCD();
}

void setup() {
  Serial.begin(115200);

  pinMode(enfriar, OUTPUT);
  pinMode(calentar, OUTPUT);
  off_enfriar;
  off_calentar;

  analogSetPinAttenuation(LM35_PIN, ADC_11db);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Keypad + LM35");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");
  delay(1500);

  temperaturaC = leerTemperatura();

  controlarSalidas();
  actualizarLCD();
}

void loop() {
  // Leer temperatura cada segundo sin bloquear el teclado
  unsigned long ahora = millis();
  if (ahora - ultimoMuestreo >= INTERVALO_SENSOR_MS) {
    ultimoMuestreo = ahora;

    temperaturaC = leerTemperatura();
    Serial.printf("Temperatura: %.1f C\n", temperaturaC);

    controlarSalidas();
    actualizarLCD();
  }

  // Teclado
  char tecla = teclado.getKey();
  if (tecla == NO_KEY) return;

  Serial.print("Tecla: ");
  Serial.println(tecla);

  if (tecla == 'C') {                    // editar HL
    editando = 'C';
    actualizarLCD();
  } else if (tecla == 'B') {             // editar LL
    editando = 'B';
    actualizarLCD();
  } else if (tecla == '*') {             // borrar lo que se está escribiendo
    entradaActual() = "";
    actualizarLCD();
  } else if (tecla == '#') {             // confirmar
    confirmarEntrada();
  } else if (tecla >= '0' && tecla <= '9') {
    if (entradaActual().length() < MAX_CARACTERES) {
      entradaActual() += tecla;
      actualizarLCD();
    }
  }
  // C y D no hacen nada
}