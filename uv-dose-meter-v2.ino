#include <EEPROM.h>

#include <ButtonV2.h>

#include <Event.h>
#include <Timer.h>

#include <Time.h>

/*********************************************************************
This is an example sketch for our Monochrome Nokia 5110 LCD Displays

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/products/338

These displays use SPI to communicate, 4 or 5 pins are required to
interface

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.
BSD license, check license.txt for more information
All text above, and the splash screen must be included in any redistribution
*********************************************************************/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// Software SPI (slower updates, more flexible pin options):
// pin 7 - Serial clock out (SCLK)
// pin 6 - Serial data out (DIN)
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 3 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(7, 6, 5, 4, 3);

// Hardware SPI (faster, but must use certain hardware pins):
// SCK is LCD serial clock (SCLK) - this is pin 13 on Arduino Uno
// MOSI is LCD DIN - this is pin 11 on an Arduino Uno
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 3 - LCD reset (RST)
// Adafruit_PCD8544 display = Adafruit_PCD8544(5, 4, 3);
// Note with hardware SPI MISO and SS pins aren't used but will still be read
// and written to during SPI transfer.  Be careful sharing these pins!

int sensorPin = A0;
int resetBtnPin = 8;
int memoryBtnPin = 9;
int buzzerPin = 10;
int ledPin = 13;

unsigned int nReadings = 0;
unsigned long int uvIntensity = 0;
unsigned long int uvFilteredIntensity = 0;
unsigned long int cumulatedUVFull = 0;
unsigned long int cumulatedUV = 0;
unsigned long int memoryCumUV = 10000;
unsigned int minUV = 1023;
unsigned int maxUV = 0;

float vi = 0;

byte buzzStatus = 1;
byte displayMode = 0;

ButtonV2 resetBtn;
ButtonV2 memoryBtn;

Timer t;

void setup()   {
  display.begin();
  // init done

  // you can change the contrast around to adapt the display
  // for the best viewing!
  display.setContrast(50);
  setTime(0);

  pinMode(sensorPin, INPUT);
  pinMode(resetBtnPin, INPUT_PULLUP);
  pinMode(memoryBtnPin, INPUT_PULLUP);

  resetBtn.SetStateAndTime(LOW);
  memoryBtn.SetStateAndTime(LOW);

  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  tone(buzzerPin, 1000, 100);
  delay(100);
  tone(buzzerPin, 2000, 100);

  digitalWrite(ledPin, LOW);
  t.every(100, takeReading);
  t.every(100, render);
  t.every(2000, beep);

  retrieveMemoryCumUV();
}

void beep ()
{
  if (cumulatedUV > memoryCumUV && buzzStatus) {
    tone(buzzerPin, 440, 200);
  }
}


void takeReading() {
  int rawValue = 1023 - analogRead(sensorPin); //Valor crudo
  float vf = (float) rawValue / 1023; // Valor normalizado
  vf = vi + (vf - vi) * 0.1; // Valor Filtrado
  uvIntensity = 100 * vf; // Valor normalizado a 100
  vi = vf;

  nReadings++;
  cumulatedUVFull = cumulatedUVFull + uvIntensity;
  cumulatedUV = cumulatedUVFull / 100;

  if (minUV > uvIntensity) {
    minUV = uvIntensity;
  }

  if (maxUV < uvIntensity) {
    maxUV = uvIntensity;
  }
}

void resetCounter() {
  setTime(0);
  uvIntensity = 0;
  cumulatedUVFull = 0;
  nReadings = 0;
  minUV = 1023;
  maxUV = 0;
  tone(buzzerPin, 1000, 100);
}

void retrieveMemoryCumUV() {
  EEPROM.get(0, memoryCumUV);
}

void storeMemoryCumUV() {
  memoryCumUV = cumulatedUV;
  EEPROM.put(0, memoryCumUV);
}

void printDigits(int digits) {
  // utility function for digital clock display: prints preceding colon and leading 0
  display.print(":");
  if (digits < 10)
    display.print('0');
  display.print(digits);
}

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
  }

  display.setCursor(84 - 5, 0);
  if (buzzStatus) {
    display.print("S");
  }

  renderProgress(39, 100 * uvIntensity / 100);
  renderProgress(43, 100 * cumulatedUV / memoryCumUV);

  display.display();

}

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

void loop() {
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
      if (displayMode >= 3) {
        displayMode = 0;
      }
      break;
  }

  t.update();
}


