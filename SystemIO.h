#pragma once

enum OUTPUTSTATE {
    FLOATING = 0,
    HIGH_12V = 1,
    GND = 2
};

class SystemIO
{
public:
    SystemIO();
    void setup();
    bool readInput(int pin);
    void setOutput(int pin, OUTPUTSTATE state);
    
private:

};

extern SystemIO systemIO;
