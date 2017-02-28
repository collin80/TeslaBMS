#pragma once
#include "config.h"
#include "BMSModule.h"

class BMSModuleManager
{
public:
    BMSModuleManager();
    void balanceCells();
    void setupBoards();
    void findBoards();
    void renumberBoardIDs();
    void clearFaults();
    void sleepBoards();
    void wakeBoards();
    void getAllVoltTemp();
    void readSetpoints();
    void setUnderVolt(float newVal);
    void setOverVolt(float newVal);
    void setOverTemp(float newVal);
    void setBalanceV(float newVal);
    void setBalanceHyst(float newVal);
    float getPackVoltage();

private:
    float packVolt;                 // All modules added together
    float OVolt;                    // Overvoltage setpoint
    float UVolt;                    // Undervoltage setpoint
    float Tset;                     // Temperature setpoint
    float balVolt = 4.01f;          // Voltage where balancing should start
    float balHyst = 0.04f;          // Amount cell voltage must be different from setpoint to turn it back off
    BMSModule modules[MAX_MODULE_ADDR]; // store data for as many modules as we've configured for.
    int numFoundModules;            // The number of modules that seem to exist
};
