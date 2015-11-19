#include <VirtualWire.h>

const int ledPin = 13;
const int txPin = 12;

void setup()
{
  // Initialise the IO and ISR
  vw_set_tx_pin(txPin);
  pinMode(ledPin, OUTPUT);
  vw_setup(2000);   // Bits per sec
}

String value;

void loop()
{
  char msg[4];

  value = String (random(1, 100));
  for (int i = 0; i < value.length(); i++) {
    msg[i] = value[i];
  }

  digitalWrite(ledPin, HIGH); // Flash a light to show transmitting
  vw_send((uint8_t *)msg, value.length());
  vw_wait_tx(); // Wait until the whole message is gone
  digitalWrite(ledPin, LOW);
  delay(100);
}
