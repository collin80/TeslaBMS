#include <Arduino.h>
#include "config.h"
#include "Logger.h"
#include "SystemIO.h"

int DigitalInputs[4] = {DIN1, DIN2, DIN3, DIN4};
int DigitalOutputs[4][2] = { {DOUT1_L, DOUT1_H}, {DOUT2_L, DOUT2_H}, {DOUT3_L, DOUT3_H}, {DOUT4_L, DOUT4_H} };

SystemIO::SystemIO()
{
    
}

void SystemIO::setup()
{
    for (int x = 0; x < 4; x++) pinMode(DigitalInputs[x], INPUT);
    
    for (int y = 0; y < 4; y++) {
        pinMode(DigitalOutputs[y][0], OUTPUT);
        digitalWrite(DigitalOutputs[y][0], LOW);
        pinMode(DigitalOutputs[y][1], OUTPUT);
        digitalWrite(DigitalOutputs[y][1], LOW);
    }
}

bool SystemIO::readInput(int pin)
{
    if (pin < 0 || pin > 3) return false;
    return !digitalRead(DigitalInputs[pin]);
}

void SystemIO::setOutput(int pin, OUTPUTSTATE state)
{
    if (pin < 0 || pin > 3) return;
    //first set it floating
    digitalWrite(DigitalOutputs[pin][0], LOW);
    digitalWrite(DigitalOutputs[pin][1], LOW);
    delayMicroseconds(10); //give mosfets some time to turn off
    if (state == HIGH_12V) digitalWrite(DigitalOutputs[pin][1], HIGH);
    if (state == GND) digitalWrite(DigitalOutputs[pin][0], HIGH);
}

SystemIO systemIO;