/*
*/

#include "Arduino.h"
#include "FiveDegreeButton.h"

FiveDegreeButton::FiveDegreeButton(int pin) {
    pinMode(pin, OUTPUT);
    joystickPin = pin;
    buttonFlasher = 0;
}

int FiveDegreeButton::update() {
    int kp = -1;
    for (byte i = 0; i < NUM_KEYS; i++) {
        if (button_flag[i] != 0) {
            button_flag[i] = 0; // reset button flag
            lastKeypress = millis();
            kp = i;
        }
    }

    if (millis() - buttonFlasher > 5) {
        update_adc_key();
        buttonFlasher = millis();
    }
    return kp;
}

// which includes DEBOUNCE ON/OFF mechanism, and continuous pressing detection
// Convert ADC value to key number
char FiveDegreeButton::get_key(unsigned int input) {
    char k;
    for (k = 0; k < NUM_KEYS; k++) {
        if (input < adc_key_val[k]) {
            return k;
        }
    }

    if (k >= NUM_KEYS)
        k = -1;     // No valid key pressed

    return k;
}

void FiveDegreeButton::update_adc_key() {
    int adc_key_in = analogRead(joystickPin);
    char key_in = get_key(adc_key_in);
    for (byte i = 0; i < NUM_KEYS; i++) {
        if (key_in == i) { //one key is pressed
            if (button_count[i] < DEBOUNCE_MAX) {
                button_count[i]++;
                if (button_count[i] > DEBOUNCE_ON) {
                    if (button_status[i] == 0) {
                        button_flag[i] = 1;
                        button_status[i] = 1; //button debounced to 'pressed' status
                    }
                }
            }
        } else { // no button pressed
            if (button_count[i] > 0) {
                button_flag[i] = 0;
                button_count[i]--;
                if (button_count[i] < DEBOUNCE_OFF) {
                    button_status[i] = 0; //button debounced to 'released' status
                }
            }
        }
    }
}