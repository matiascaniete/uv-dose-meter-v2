#include <EEPROM.h>
#include <Event.h>
#include <Timer.h>
#include <Time.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <VirtualWire.h>
#include <FiveDegreeButton.h>

// Software SPI (slower updates, more flexible pin options):
// pin 7 - Serial clock out (SCLK)
// pin 6 - Serial data out (DIN)
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 3 - LCD reset (RST)
//inicialización del display (Nokia LCD 5110)
//Adafruit_PCD8544 display = Adafruit_PCD8544(7, 6, 5, 4, 3);
Adafruit_PCD8544 display = Adafruit_PCD8544(2, 3, 4, 5, 6);

int joystickPin =   A0; //Pin del joystick
int sensorPin =     A1; //Pin del sensor UV
int buzzerPin =     10; //Pin del buzzer
int ledPin =        13; //Pin del Led indicador
int rxPin =         11; //Pin del receptor RF 433MHz
int lcdLightPin =   7;  //Pin del led del panel LCD

unsigned int nReadings = 0;                     //Numero de lecturas desde el momento de reset
unsigned long int uvIntensity = 0;              //Intensidad actual UV
unsigned long int uvFilteredIntensity = 0;      //Intensidad suavizada UV
unsigned long int cumulatedUVFull = 0;          //Dosis UV total acumulada
unsigned long int cumulatedUV = 0;              //1% de la dosis total acumulada
unsigned int minUV = 1023;                      //Valor minimo de la Intensidad UV desde el momento de reset
unsigned int maxUV = 0;                         //Valor máximo de la Intensidad UV desde el momento de reset
unsigned long int lastRFReading;                //Tiempo de la ultima lectura RF
int rawValue;                                   //Valor crudo de la lectura
unsigned int localBatteryLevel = 0;             //Valor Vcc

char StringReceived[22];                        //Valor crudo de la ultima lectura RF

int rfReading;                                  //Valor numerico de la ultima lectura RF
int rfBatteryLevel;                             //Valor remoto del nivel de bateria
int rfCounter;                                  //Valor del contador de paquetes transmitidos
int rfLocalCounter = 0;                         //Valor del contador local de paquetes, para el control de paquetes perdidos.
int rfSignalQuality = 0;                        //Valor de la calidad de señal en funcion de los paquetes perdidos,

unsigned long int rfMissing = 0;

// ID of the settings block
#define CONFIG_VERSION "vs5"

// Tell it where to store your config data in EEPROM
#define CONFIG_START 32

// Example settings structure
struct StoreStruct {
  // This is for mere detection if they are your settings
  char version[4];
  // The variables of settings
  unsigned int tareValue;                 //"Valor zero" de calibración del sensor UV
  unsigned int multiplierValue;           //Factor de multiplicacion del valor de entrada
  unsigned long int memoryCumUV;          //Dosis límite almacenada en memoria EEPROM
} storage = {
  CONFIG_VERSION,
  206,
  20,
  2000
};

float vi = 0;

int note[7] = {440, 493, 523, 587, 659, 698, 784};
byte buzzStatus = 0;                            //Indica si debe sonar el buzzer cuando la dosis limite es alcanzada
byte rfStatus = 0;                              //Indica si se están recibiendo datos desde el receptor RF

#define DM_TIMES          0       //Mostrar tiempos
#define DM_INTENSITY      1       //Mostrar valores actuales de Intensidad
#define DM_DOSIS          2       //Mostrar Dosis acumulada
#define DM_RF_INFO        3       //Mostrar RF info
#define DM_BT_INFO        4       //Mostrar informacion de baterias
#define DM_CONFIG_INFO    5       //Mostrar valores de configuracion
#define DM_DASHBOARD      6       //Mostrar valores de configuracion
#define MAX_DISPLAY_MODES 7       //Número de modos de visualización.

//Indica el modo de visualizacion actual
int  displayMode = DM_DASHBOARD;

Timer t;                                        //Inicializacion del timer
FiveDegreeButton fdb = FiveDegreeButton(joystickPin);

void (*buttonFunctionPtrs[NUM_KEYS][MAX_DISPLAY_MODES])() = {NULL}; //the array of function pointers for buttons
void (*renderFunctionPtrs[MAX_DISPLAY_MODES])() = {NULL}; //the array of function pointers for display

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete

void setup() {
  display.begin();

  // you can change the contrast around to adapt the display for the best viewing!
  display.setContrast(50);

  setTime(0);

  //Comportamiento de los pines de entrada y salida
  pinMode(joystickPin, INPUT);
  pinMode(sensorPin, INPUT);

  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(lcdLightPin, OUTPUT);

  //Tono de Inicializacion
  tone(buzzerPin, 1000, 100);
  delay(100);
  tone(buzzerPin, 2000, 100);

  digitalWrite(ledPin, HIGH);
  digitalWrite(lcdLightPin, HIGH);

  //Definicion de las interrupciones por Software
  //Hacer una lectura del valor UV 10 veces por segundo
  t.every(100, takeReading);

  //Actualizar la informacion mostrada en pantalla
  t.every(250, render);

  //Sonar el buzzer cada 2 segundos (si se ha superado la Dosis limite)
  t.every(2000, beep);

  t.every(1000, resetRFQualityCounter);

  //Lee el valor en memoria de la configuracion
  loadConfig();

  // Init del receptor RF
  vw_set_rx_pin(rxPin);
  vw_setup(4000);       // Bits per sec
  vw_rx_start();        // Start the receiver PLL running

  // Definicion de los punteros de funciones de manejo de botones de navegacion y renderizacion
  buttonFunctionPtrs[UP_KEY][DM_DOSIS] = &increaseTargetDosis;
  buttonFunctionPtrs[DOWN_KEY][DM_DOSIS] = &decreaseTargetDosis;
  buttonFunctionPtrs[CENTER_KEY][DM_DOSIS] = &saveConfig;

  buttonFunctionPtrs[CENTER_KEY][DM_INTENSITY] = &resetCounter;
  buttonFunctionPtrs[CENTER_KEY][DM_TIMES] = &resetCounter;
  buttonFunctionPtrs[CENTER_KEY][DM_RF_INFO] = &resetRF;
  buttonFunctionPtrs[CENTER_KEY][DM_DASHBOARD] = &resetCounter;

  for (byte i = 0; i < MAX_DISPLAY_MODES; i++) {
    buttonFunctionPtrs[RIGHT_KEY][i] = &increaseMenu;
    buttonFunctionPtrs[LEFT_KEY][i] = &decreaseMenu;
  }

  renderFunctionPtrs[DM_TIMES] = &renderTime;
  renderFunctionPtrs[DM_DOSIS] = &renderDosis;
  renderFunctionPtrs[DM_INTENSITY] = &renderUV;
  renderFunctionPtrs[DM_RF_INFO] = &renderRFInfo;
  renderFunctionPtrs[DM_BT_INFO] = &renderBatteryInfo;
  renderFunctionPtrs[DM_CONFIG_INFO] = &renderConfig;
  renderFunctionPtrs[DM_DASHBOARD] = &renderDashboard;

  Serial.begin(9600);  // Debugging only
  Serial.println("setup");
  Serial.println("Type 'ls' for commands");

  // reserve 200 bytes for the inputString:
  inputString.reserve(200);
}

void increaseMenu() {
  displayMode++;
  if (displayMode > MAX_DISPLAY_MODES - 1) {
    displayMode = 0;
  }
  //Serial.println(displayMode);
}

void decreaseMenu() {
  displayMode--;
  if (displayMode < 0) {
    displayMode = MAX_DISPLAY_MODES - 1;
  }
  //Serial.println(displayMode);
}
void increaseTargetDosis() {
  storage.memoryCumUV += 50;
}

void decreaseTargetDosis() {
  storage.memoryCumUV -= 50;
}

//Reset de variables
void resetCounter() {
  setTime(0);
  uvIntensity = 0;
  cumulatedUVFull = 0;
  nReadings = 0;
  minUV = 1023;
  maxUV = 0;
}

void resetRF() {
  rfMissing = 0;
}

//Obtener el valor almacenado en la memoria EEPROM de la Dosis limite
void loadConfig() {
  // To make sure there are settings, and they are YOURS!
  // If nothing is found it will use the default settings.
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2])
    for (unsigned int t = 0; t < sizeof(storage); t++)
      *((char*)&storage + t) = EEPROM.read(CONFIG_START + t);
}

void saveConfig() {
  for (unsigned int t = 0; t < sizeof(storage); t++)
    EEPROM.write(CONFIG_START + t, *((char*)&storage + t));
}

//Hace la lectura del sensor
void takeReading() {
  //int rawValue = analogRead(sensorPin) - tareValue;             // Valor crudo

  //Si se reciben datos RF entonces priorizar su utilizacion
  if (rfStatus) {
    rawValue = rfReading - storage.tareValue;
    if (rawValue < 0) {
      rawValue = 0;
    }
  }

  float vf = (float) rawValue / (1023 - storage.tareValue); // Valor normalizado
  vf = storage.multiplierValue * vf;                        // Valor multiplicado
  vf = vi + (vf - vi) * 0.1;                        // Valor Filtrado
  uvIntensity = 100 * vf;                           // Valor normalizado a 100
  vi = vf;                                          // Necesario para el Filtrado

  nReadings++;                                      // Contador de lecturas
  cumulatedUVFull = cumulatedUVFull + uvIntensity;  // Dosis acumulada
  cumulatedUV = cumulatedUVFull / 100;              // 1% de la Dosis acumulada para evitar rebalse en el display

  //Calculo del minimo y maximo de la Intensidad actual UV
  if (minUV > uvIntensity) {
    minUV = uvIntensity;
  }

  if (maxUV < uvIntensity) {
    maxUV = uvIntensity;
  }

  localBatteryLevel = readVcc();
}

// utility function for digital clock display: prints preceding colon and leading 0
void printDigits(int digits) {
  if (digits < 10)
    display.print('0');
  display.print(digits);
}

void printTitle(char* title) {
  display.println(title);
  display.println("");
}

void printField(char* label, char* value, char* suffix) {
  display.print(label);
  display.print(value);
  if (suffix != "") {
    display.print(" ");
    display.print(suffix);
  }
  display.println();
}

void printField(char* label, char* value) {
  printField(label, value, "");
}

void printField(char* label, int value, char* suffix) {
  display.print(label);
  display.print(value);
  if (suffix != "") {
    display.print(" ");
    display.print(suffix);
  }
  display.println();
}

void printField(char* label, int value) {
  printField(label, value, "");
}

void printTime(char* label, int h, int m, int s) {
  display.print(label);
  printDigits(h);
  display.print(":");
  printDigits(m);
  display.print(":");
  printDigits(s);
  display.println();
}

//mostrar una barra de progreso en el display, y = altura, percent = porcentaje a mostrar
void renderProgress(byte y, unsigned int percent) {
  if (percent > 100) {
    percent = 100;
  }

  display.drawRect(0, y, 84, 5, BLACK);
  for (int i = 0; i < (80 * percent / 100); i++) {
    if (percent == 100) {
      if ((second() % 2) == 0) {
        display.drawPixel(i + 2, y + 2, BLACK);
      }
    } else {
      display.drawPixel(i + 2, y + 2, BLACK);
    }
  }
}

//mostrar informacion en pantalla
void render() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);

  display.setCursor(84 - 11, 0);
  if (rfStatus) {
    display.print("w");
  }

  if (buzzStatus) {
    display.print("s");
  }

  display.setCursor(0, 0);

  if (renderFunctionPtrs[displayMode]) {
    (renderFunctionPtrs[displayMode])();
  }

  display.display();
}

//Sonar el buzzer si el valor de la dosis limite es superada y si el buzzer está activado
void beep () {
  if (cumulatedUV > storage.memoryCumUV) {
    if (buzzStatus) {
      tone(buzzerPin, 440, 200);
    }
  }
}

void resetRFQualityCounter() {
  rfSignalQuality = rfLocalCounter;
  rfLocalCounter = 0;
  if (millis() - lastRFReading > 1 * 1000) {
    rfMissing++;
  }

}

long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}

void loop() {
  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;

  if (vw_get_message(buf, &buflen)) { // Non-blocking
    // Message with a good checksum received, print it.
    for (int i = 0; i < buflen; i++) {
      StringReceived[i] = char(buf[i]);
    }

    int readStatus = sscanf(StringReceived, "%d,%d,%d,", &rfReading, &rfCounter, &rfBatteryLevel); // Converts a string to an array
    rfLocalCounter++;

    rfStatus = 1;
    lastRFReading = millis();
  }

  memset( StringReceived, 0, sizeof( StringReceived));// This line is for reset the StringReceived

  //Si pasan mas de 5 segundos sin recibir datos RF...
  if (millis() - lastRFReading > 5 * 1000) {
    rfStatus = 0;
  }

  int kp = fdb.update();
  if (kp >= 0) {
    //Enciende la luz trasera al pulsar cualquier boton y se apaga luego de 30 segundos
    digitalWrite(lcdLightPin, HIGH);

    //No poner mas de 10ms de duración, sino crashes
    tone(buzzerPin, note[kp], 10);

    if (buttonFunctionPtrs[kp][displayMode]) {
      buttonFunctionPtrs[kp][displayMode]();
    }

  }

  if (millis() - fdb.lastKeypress > 30 * 1000) {
    digitalWrite(lcdLightPin, LOW);
  }

  if (stringComplete) {
    doTheAction(inputString);
    // clear the string:
    inputString = "";
    stringComplete = false;
  }

  t.update();
}

void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}

void doTheAction(String& action) {
  action.trim();
  
  if (action == "ls") {
    Serial.println("get uv");
    Serial.println("get dosis");
    Serial.println("reset");
  }
  if (action == "get uv") {
    Serial.println(uvIntensity);
  }
  if (action == "get dosis") {
    Serial.println(cumulatedUV);
  }
  if (action == "reset") {
    tone(buzzerPin, 440, 200);
    resetCounter();
    Serial.println("UV Counter reseted");
  }

}

//Mostrar informacion de los tiempos en el display
void renderTime() {
  unsigned long int eta = now() * storage.memoryCumUV / cumulatedUV;
  long int etl = eta - now();

  printTitle("TIMINGS");
  printTime("CURR:", hour(), minute(), second());

  if (etl < 0) {
    char v1[] = "00:00:00";
    char v2[] = "--:--:--";
    printField("LEFT:", ((second() % 2) == 0) ? v1 : v2);
  } else {
    printTime("LEFT:", hour(etl), minute(etl), second(etl));
  }

  printTime("ESTI:", hour(eta), minute(eta), second(eta));
  renderProgress(43, 100 * cumulatedUV / storage.memoryCumUV);
}

//mostrar los valores actuales de Intensidad UV en el display
void renderUV() {
  printTitle("UV-VALUES");
  printField("MIN :", minUV);
  printField("CURR:", uvIntensity);
  printField("MAX :", maxUV);
  renderProgress(43, 100 * uvIntensity / 100);
}

void renderRFInfo() {
  printTitle("RF-INFO");
  printField("SIGNAL Q:", rfSignalQuality);
  printField("MISSED P:", rfMissing);
  printField("UV-VALUE:", rfReading);
}

void renderBatteryInfo() {
  printTitle("BATTERY");
  printField("-TX:", constrain(map(rfBatteryLevel, 2700, 5000, 0, 100), 0 , 100), "%");
  printField("-RX:", constrain(map(localBatteryLevel, 2700, 5000, 0, 100), 0, 100), "%");
}

void renderConfig() {
  printTitle("CONFIG");
  printField("TARE:", storage.tareValue);
  printField("MULTIPLIER:", storage.multiplierValue);
}

void renderDosis() {
  printTitle("DOSIS");
  printField("CURR:", cumulatedUV);
  printField("TARG:", storage.memoryCumUV);
  printField("PERC:", (100 * cumulatedUV / storage.memoryCumUV), "%");
  renderProgress(43, 100 * cumulatedUV / storage.memoryCumUV);
}

void renderDashboard() {
  printTitle("DASHBOARD");
  printField("UV-INTS:", uvIntensity);
  printField("UV-DOSE:", cumulatedUV);
  printTime("TIME:", hour(), minute(), second());
  renderProgress(43, 100 * cumulatedUV / storage.memoryCumUV);
}

