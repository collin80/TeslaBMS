#pragma once

class BMSModule
{
public:
    BMSModule();
    void readStatus();
    bool readModuleValues();
    float getCellVoltage(int cell);
    float getLowCellV();
    float getHighCellV();
    float getAverageV();
    float getLowTemp();
    float getHighTemp();
    float getHighestModuleVolt();
    float getLowestModuleVolt();
    float getHighestCellVolt(int cell);
    float getLowestCellVolt(int cell);
    float getHighestTemp();
    float getLowestTemp();
    float getAvgTemp();
    float getModuleVoltage();
    float getTemperature(int temp);
    uint8_t getFaults();
    uint8_t getAlerts();
    uint8_t getCOVCells();
    uint8_t getCUVCells();
    void setAddress(int newAddr);
    int getAddress();
    bool isExisting();
    void setExists(bool ex);
    void balanceCells();
    uint8_t getBalancingState(int cell);

private:
    float cellVolt[6];          // calculated as 16 bit value * 6.250 / 16383 = volts
    float lowestCellVolt[6];
    float highestCellVolt[6];
    float moduleVolt;          // calculated as 16 bit value * 33.333 / 16383 = volts
    float temperatures[2];     // Don't know the proper scaling at this point    
    float lowestTemperature;
    float highestTemperature;
    float lowestModuleVolt;
    float highestModuleVolt;
    uint8_t balanceState[6]; //0 = balancing off for this cell, 1 = balancing currently on
    bool exists;
    int alerts;
    int faults;
    int COVFaults;
    int CUVFaults;
    int goodPackets;
    int badPackets;

    uint8_t moduleAddress;     //1 to 0x3E
};
