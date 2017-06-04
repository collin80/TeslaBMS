#pragma once
#include "config.h"
#include "BMSModule.h"
#include <due_can.h>

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
    void setBatteryID();
    float getPackVoltage();
    float getAvgTemperature();
    float getAvgCellVolt();
    void processCANMsg(CAN_FRAME &frame);
    void printPackSummary();
    void printPackDetails();

private:
    float packVolt;                         // All modules added together
    float lowestPackVolt;
    float highestPackVolt;
    float lowestPackTemp;
    float highestPackTemp;
    BMSModule modules[MAX_MODULE_ADDR + 1]; // store data for as many modules as we've configured for.
    int numFoundModules;                    // The number of modules that seem to exist
    bool isFaulted;
    
    void sendBatterySummary();
    void sendModuleSummary(int module);
    void sendCellDetails(int module, int cell);
    
};
