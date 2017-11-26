#include <Arduino.h>
#include "LiquidCrystal_I2C.h"
#include "Rotary.h"

#define RELAY_PIN 12
#define ENC_PIN_BTN 4

#define EDIT_NONE 0
#define EDIT_HOUR 1
#define EDIT_MIN 2
#define EDIT_ON_HOUR 3
#define EDIT_ON_MIN 4
#define EDIT_OFF_HOUR 5
#define EDIT_OFF_MIN 6
#define EDIT_MODULUS 7

LiquidCrystal_I2C lcd(0x3F, 16, 2);
Rotary rot(3, 2);

/*
  Measured time error in msec per minute.
  > 0 means that clock go to fast and makes it go slower
  < 0 means that clock go to slow and makes it go faster
*/
const int timeFix = 65;

unsigned long prevMillis;

/* current time */
volatile int hour = 0;
volatile int min = 0;
volatile int sec = 0;
int msec = 0;

/* on @ */
volatile int on_hour = 9;
volatile int on_min = 0;

/* off @ */
volatile int off_hour = 22;
volatile int off_min = 0;

bool relay_on = false;
bool btn_was_released = true;

volatile uint8_t current_edit = EDIT_NONE;

void display() {
    String line = "Time  ";
    if(relay_on) line += "+";
    else line += " ";
    line += "On  ";

    if((current_edit == EDIT_ON_HOUR) && (sec & 1))
        line += "  ";
    else {
        if(on_hour < 10) line += "0";
        line += String(on_hour);
    }

    line += ":";

    if((current_edit == EDIT_ON_MIN) && (sec & 1))
        line += "  ";
    else {
        if(on_min < 10) line += "0";
        line += String(on_min);
    }

    lcd.setCursor(0, 0);
    lcd.print(line);

    line = "";

    if((current_edit == EDIT_HOUR) && (sec & 1))
        line += "  ";
    else {
        if(hour < 10) line += "0";
        line += String(hour);
    }

    if(sec & 1) line += ":";
    else line += " ";

    if((current_edit == EDIT_MIN) && (sec & 1))
        line += "  ";
    else {
        if(min < 10) line += "0";
        line += String(min);
    }
    
    line += " ";
    if(relay_on) line += " ";
    else line += "+";
    line += "Off ";

    if((current_edit == EDIT_OFF_HOUR) && (sec & 1))
        line += "  ";
    else {
        if(off_hour < 10) line += "0";
        line += String(off_hour);
    }
    
    line += ":";

    if((current_edit == EDIT_OFF_MIN) && (sec & 1))
        line += "  ";
    else {
        if(off_min < 10) line += "0";
        line += String(off_min);
    }

    lcd.setCursor(0, 1);
    lcd.print(line);
}

int time_mod(int x, int mod) {
    if(x >= mod) return 0;
    else if(x < 0) return (mod - 1);
    else return x;
}

ISR(PCINT2_vect) {  
    unsigned char result = rot.process();
    if (result == DIR_NONE) {  
        // do nothing
    } else if (result == DIR_CW) { // increment 
        if(current_edit == EDIT_HOUR)
            hour = time_mod(hour + 1, 24);
        else if(current_edit == EDIT_MIN) {
            min = time_mod(min + 1, 60);
            sec = 0;
        } else if(current_edit == EDIT_ON_HOUR)
            on_hour = time_mod(on_hour + 1, 24);
        else if(current_edit == EDIT_ON_MIN)
            on_min = time_mod(on_min + 1, 60);
        else if(current_edit == EDIT_OFF_HOUR)
            off_hour = time_mod(off_hour + 1, 24);
        else if(current_edit == EDIT_OFF_MIN)
            off_min = time_mod(off_min + 1, 60);
    } else if(result == DIR_CCW) { // decrement 
        if(current_edit == EDIT_HOUR)
            hour = time_mod(hour - 1, 24);
        else if(current_edit == EDIT_MIN) {
            min = time_mod(min - 1, 60);
            sec = 0;
        } else if(current_edit == EDIT_ON_HOUR)
            on_hour = time_mod(on_hour - 1, 24);
        else if(current_edit == EDIT_ON_MIN)
            on_min = time_mod(on_min - 1, 60);
        else if(current_edit == EDIT_OFF_HOUR)
            off_hour = time_mod(off_hour - 1, 24);
        else if(current_edit == EDIT_OFF_MIN)
            off_min = time_mod(off_min - 1, 60);
    }  
}

void setup() {
    lcd.begin();

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    pinMode(ENC_PIN_BTN, INPUT_PULLUP);

    // rotory interrupt
    PCICR |= (1 << PCIE2);
    PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
    sei();
}

void loop() {
    unsigned long currMillis;
    unsigned long delta;

    delay(50);

    currMillis = millis();
    delta = currMillis - prevMillis;
    prevMillis = currMillis;
    msec += delta;

    while(msec >= 1000) {
        msec -= 1000;
        sec++;
        if(sec == 60) {
            sec = 0;
            min++;

            msec -= timeFix;

            if(min == 60) {
                min = 0;
                hour++;
                if(hour == 24)
                    hour = 0;
            }
        }
    }

    bool btn_pressed = (digitalRead(ENC_PIN_BTN) == LOW);
    if(!btn_pressed) {
        btn_was_released = true;
    } else if(btn_was_released) {
        current_edit = (current_edit + 1) % EDIT_MODULUS;
        btn_was_released = false;
    }

    int on = on_hour * 100 + on_min;
    int off = off_hour * 100 + off_min;
    int t = hour*100 + min;

    /* The logic is simple - just draw a few circles */
    relay_on = ((on < off) && (t >= on) && (t < off)) ||
                ((on > off) && !((t >= off && t < on)));

    digitalWrite(RELAY_PIN, relay_on ? HIGH : LOW);
    display();
}
