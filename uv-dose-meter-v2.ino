#include <EEPROM.h>
#include <ButtonV2.h>
#include <Event.h>
#include <Timer.h>
#include <Time.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <VirtualWire.h>

// Software SPI (slower updates, more flexible pin options):
// pin 7 - Serial clock out (SCLK)
// pin 6 - Serial data out (DIN)
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 3 - LCD reset (RST)
//inicialización del display (Nokia LCD 5110)
Adafruit_PCD8544 display = Adafruit_PCD8544(7, 6, 5, 4, 3);

int sensorPin = A0;         //Pin del sensor UV
int resetBtnPin = 8;        //Pin del boton de reseteo del timer
int memoryBtnPin = 9;       //Pin del boton del modo de visualizacion
int buzzerPin = 10;         //Pin del buzzer
int ledPin = 13;            //Pin del Led indicador
int rxPin = 11;             //Pin del receptor RF 433MHz

unsigned int nReadings = 0;                     //Numero de lecturas desde el momento de reset
unsigned long int uvIntensity = 0;              //Intensidad actual UV
unsigned long int uvFilteredIntensity = 0;      //Intensidad suavizada UV
unsigned long int cumulatedUVFull = 0;          //Dosis UV total acumulada
unsigned long int cumulatedUV = 0;              //1% de la dosis total acumulada
unsigned long int memoryCumUV = 10000;          //Dosis límite almacenada en memoria EEPROM
unsigned int minUV = 1023;                      //Valor minimo de la Intensidad UV desde el momento de reset
unsigned int maxUV = 0;                         //Valor máximo de la Intensidad UV desde el momento de reset
unsigned int tareValue = 200;                   //"Valor zero" de calibración del sensor UV
unsigned int multiplierValue = 5;               //Factor de multiplicacion del valor de entrada
unsigned long int lastRFReading;                //Tiempo de la ultima lectura RF

String rfValue;                                 //Valor crudo de la ultima lectura RF
int rfReading;                    //Valor numerico de la ultima lectura RF

float vi = 0;

byte buzzStatus = 1;                            //Indica si debe sonar el buzzer cuando la dosis limite es alcanzada
byte rfStatus = 0;                              //Indica si se están recibiendo datos desde el receptor RF
byte displayMode = 0;                           //Indica el modo de visualizacion:
//0: Mostrar tiempos
//1: Mostrar valores actuales de Intensidad
//2: Mostrar Dosis acumulada

ButtonV2 resetBtn;                              //Inicializacion del boton de Reset
ButtonV2 memoryBtn;                             //Inicializacion del boton de Modo de visualizacion

Timer t;                                        //Inicializacion del timer

void setup()   {
  display.begin();
  // init done

  // you can change the contrast around to adapt the display
  // for the best viewing!
  display.setContrast(50);

  setTime(0);

  //Comportamiento de los pines de entrada y salida
  pinMode(sensorPin, INPUT);
  pinMode(resetBtnPin, INPUT_PULLUP);
  pinMode(memoryBtnPin, INPUT_PULLUP);

  //Inicializacion de botones
  resetBtn.SetStateAndTime(LOW);
  memoryBtn.SetStateAndTime(LOW);

  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  //Tono de Inicializacion
  tone(buzzerPin, 1000, 100);
  delay(100);
  tone(buzzerPin, 2000, 100);

  digitalWrite(ledPin, LOW);

  //Definicion de las interrupciones por Software
  //Hacer una lectura del valor UV 10 veces por segundo
  t.every(100, takeReading);

  //Actualizar la informacion mostrada en pantalla 10 veces por segundo
  t.every(100, render);

  //Sonar el buzzer cada 2 segundos (si se ha superado la Dosis limite)
  t.every(2000, beep);

  //Lee el valor en memoria de la Dosis limite
  retrieveMemoryCumUV();

  Serial.begin(9600);  // Debugging only
  Serial.println("setup");

  // Init del receptor RF
  vw_set_rx_pin(rxPin);
  vw_setup(2000);       // Bits per sec
  vw_rx_start();        // Start the receiver PLL running
}

void loop() {
  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;

  if (vw_get_message(buf, &buflen)) // Non-blocking
  {
    int i;
    // Message with a good checksum received, print it.
    //Serial.print("Got: ");
    rfValue = "";
    for (i = 0; i < buflen; i++)
    {
      rfValue = rfValue + char(buf[i]);
      rfReading = rfValue.toInt();
      //Serial.print(buf[i]);
      //Serial.print(' ');
    }
    //Serial.println();
    rfStatus = 1;
    lastRFReading = millis();
  }

  //Si pasan mas de 5 segundos sin recibir datos RF...
  if (millis() - lastRFReading > 5 * 1000) {
    rfStatus = 0;
  }

  switch (resetBtn.CheckButton(resetBtnPin))
  {
    case PRESSED:
      resetCounter();
      break;
    case DOUBLE_PRESSED:
      buzzStatus = !buzzStatus;
      break;
    case MULTI_PRESSED:
      storeMemoryCumUV();
      break;
  }


  switch (memoryBtn.CheckButton(memoryBtnPin))
  {
    case PRESSED:
      displayMode ++;
      if (displayMode > 3) {
        displayMode = 0;
      }
      break;
  }

  t.update();
}

//Hace la lectura del sensor
void takeReading() {
  int rawValue = analogRead(sensorPin) - tareValue; // Valor crudo

  //Si se reciben datos RF entonces priorizar su utilizacion
  if (rfStatus) {
    rawValue = rfReading;
  }
  float vf = (float) rawValue / (1023 - tareValue); // Valor normalizado
  vf = multiplierValue * vf;                        // Valor multiplicado
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
}

//Reset de variables
void resetCounter() {
  setTime(0);
  uvIntensity = 0;
  cumulatedUVFull = 0;
  nReadings = 0;
  minUV = 1023;
  maxUV = 0;
  tone(buzzerPin, 1000, 100);
}

//Obtener el valor almacenado en la memoria EEPROM de la Dosis limite
void retrieveMemoryCumUV() {
  EEPROM.get(0, memoryCumUV);
}

//Almacenar la Dosis limite en memoria
void storeMemoryCumUV() {
  memoryCumUV = cumulatedUV;
  EEPROM.put(0, memoryCumUV);
}

// utility function for digital clock display: prints preceding colon and leading 0
void printDigits(int digits) {
  display.print(":");
  if (digits < 10)
    display.print('0');
  display.print(digits);
}

//Mostrar informacion de los tiempos en el display
void renderTime(int what) {
  unsigned long int eta = now() * memoryCumUV / cumulatedUV;
  long int etl = eta - now();

  switch (what)
  {
    case 0:
      display.print("CURR:");
      display.print(hour());
      printDigits(minute());
      printDigits(second());
      display.println();
      break;

    case 1:
      display.print("LEFT:");
      if (etl < 0) {
        String v = ((second() % 2) == 0) ? "0:00:00" : "-:--:--";
        display.println(v);
      }
      else {
        display.print(hour(etl));
        printDigits(minute(etl));
        printDigits(second(etl));
        display.println();
      }
      break;

    case 2:
      display.print("ESTI:");
      display.print(hour(eta));
      printDigits(minute(eta));
      printDigits(second(eta));
      display.println();
      break;
  }
}

//mostrar los valores actuales de Intensidad UV en el display
void renderUV(int what) {
  switch (what)
  {
    case 0:
      display.print("CURR:");
      display.print(uvIntensity);
      display.println();
      break;

    case 1:
      display.print("MIN :");
      display.print(minUV);
      display.println();
      break;

    case 2:
      display.print("MAX :");
      display.print(maxUV);
      display.println();
      break;
  }
}

//mostrar informacion en pantalla
void render() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0, 0);

  switch (displayMode) {
    case 0:
      display.println("--TIMINGS");
      renderTime(0);
      renderTime(1);
      renderTime(2);
      display.print("NR:");
      display.println(nReadings);
      break;
    case 1:
      display.println("--UV VALUES");
      renderUV(0);
      renderUV(1);
      renderUV(2);
      break;

    case 2:
      display.println("--DOSE");
      display.print("CURR:");
      display.print(cumulatedUV);
      display.print("");
      display.println();

      display.print("TARG:");
      display.print(memoryCumUV);
      display.print("");
      display.println();

      display.print("PERC:");
      display.print(100 * cumulatedUV / memoryCumUV);
      display.print("%");
      display.println();
      break;

    case 3:
      display.println("--RF-INFO");
      display.println(rfValue);
      display.println(rfReading);
      break;
  }

  display.setCursor(84 - 5, 0);
  if (buzzStatus) {
    display.print("S");
  }

  display.setCursor(84 - 11, 0);
  if (rfStatus) {
    display.print("W");
  }

  renderProgress(39, 100 * uvIntensity / 100);
  renderProgress(43, 100 * cumulatedUV / memoryCumUV);

  display.display();
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

//Sonar el buzzer si el valor de la dosis limite es superada y si el buzzer está activado

void beep ()
{
  if (cumulatedUV > memoryCumUV && buzzStatus) {
    tone(buzzerPin, 440, 200);
  }
}
