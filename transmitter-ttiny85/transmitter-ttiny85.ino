#include <VirtualWire.h>

const int ledPin = 1;
const int sensorPin = 3;
const int txPin = 4;

void setup()
{
  // Initialise the IO and ISR
  vw_set_tx_pin(txPin);
  vw_set_ptt_pin(2);    //Necesarias para hacer funcionar la libreria en el ttiny85
  vw_set_rx_pin(0);     //Necesarias para hacer funcionar la libreria en el ttiny85
  pinMode(ledPin, OUTPUT);
  vw_setup(2000);   // Bits per sec

  pinMode(sensorPin, INPUT);
}

String value;

void loop()
{
  char msg[4];

  //value = String (random(1, 100));
  value = String (analogRead(sensorPin) - 627);
  for (int i = 0; i < value.length(); i++) {
    msg[i] = value[i];
  }

  digitalWrite(ledPin, HIGH); // Flash a light to show transmitting
  vw_send((uint8_t *)msg, value.length());
  vw_wait_tx(); // Wait until the whole message is gone
  digitalWrite(ledPin, LOW);
  delay(100);
}

