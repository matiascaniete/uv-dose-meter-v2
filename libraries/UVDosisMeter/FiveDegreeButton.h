/*
*/
#ifndef FiveDegreeButton_h
#define FiveDegreeButton_h

#include "Arduino.h"

//keypad debounce parameter
#define DEBOUNCE_MAX    15
#define DEBOUNCE_ON     10
#define DEBOUNCE_OFF    3

#define NUM_KEYS        5

// joystick number
#define LEFT_KEY        0
#define CENTER_KEY      1
#define DOWN_KEY        2
#define RIGHT_KEY       3
#define UP_KEY          4

class FiveDegreeButton {
    public:
        FiveDegreeButton(int pin);
        int update();
        unsigned long int lastKeypress;                 //Tiempo de la ultima keypressed
    private:
        int joystickPin;
        unsigned long buttonFlasher;
        int adc_key_val[5] = {50, 200, 400, 600, 800};

        // debounce counters
        byte button_count[NUM_KEYS];
        // button status - pressed/released
        byte button_status[NUM_KEYS];
        // button on flags for user program
        byte button_flag[NUM_KEYS];

        char get_key(unsigned int input);
        void update_adc_key();
};

#endif
