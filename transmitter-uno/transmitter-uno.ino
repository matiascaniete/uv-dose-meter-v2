#include <VirtualWire.h>

/*
  MP8511 UV Sensor Read Example
  Date: January 15th, 2014
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  The MP8511 UV Sensor outputs an analog signal in relation to the amount of UV light it detects.

  Connect the following MP8511 breakout board to Arduino:
  3.3V = 3.3V
  OUT = A0
  GND = GND
  EN = 3.3V
  3.3V = A1
  These last two connections are a little different. Connect the EN pin on the breakout to 3.3V on the breakout.
  This will enable the output. Also connect the 3.3V pin of the breakout to Arduino pin 1.

  This example uses a neat trick. Analog to digital conversions rely completely on VCC. We assume
  this is 5V but if the board is powered from USB this may be as high as 5.25V or as low as 4.75V:
  http://en.wikipedia.org/wiki/USB#Power Because of this unknown window it makes the ADC fairly inaccurate
  in most cases. To fix this, we use the very accurate onboard 3.3V reference (accurate within 1%). So by doing an
  ADC on the 3.3V pin (A1) and then comparing this against the reading from the sensor we can extrapolate
  a true-to-life reading no matter what VIN is (as long as it's above 3.4V).


  This sensor detects 280-390nm light most effectively. This is categorized as part of the UVB (burning rays)
  spectrum and most of the UVA (tanning rays) spectrum.

  There's lots of good UV radiation reading out there:
  http://www.ccohs.ca/oshanswers/phys_agents/ultravioletradiation.html
  https://www.iuva.org/uv-faqs

*/

//Hardware pin definitions
int UVOUT = A0; //Output from the sensor
int REF_3V3 = A1; //3.3V power on the Arduino board

const int ledPin = 13;
const int txPin = 4;

char Sensor1CharMsg[21];// The string that we are going to send trought rf

byte counter = 0;
int batteryLevel = 0;

void setup() {
  // Initialise the IO and ISR
  vw_set_tx_pin(txPin);
  pinMode(ledPin, OUTPUT);
  vw_setup(2000);   // Bits per sec

  pinMode(UVOUT, INPUT);
  pinMode(REF_3V3, INPUT);
}

void loop() {
  unsigned int uvLevel = analogRead(UVOUT);
  unsigned int refLevel = analogRead(REF_3V3);

  counter++;
  batteryLevel = readVcc();

  //Use the 3.3V power pin as a reference to get a very accurate output value from sensor
  int value =  int ( mapfloat( uvLevel, 0, refLevel, 0, 1023 ));

  sprintf(Sensor1CharMsg, "%d,%d,%d,", value, counter, batteryLevel);

  digitalWrite(ledPin, HIGH);
  vw_send((uint8_t *)Sensor1CharMsg, strlen(Sensor1CharMsg));
  vw_wait_tx(); // Wait until the whole message is gone
  digitalWrite(ledPin, LOW);
  delay(500);

}


//The Arduino Map function but for floats
//From: http://forum.arduino.cc/index.php?topic=3922.0
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
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

