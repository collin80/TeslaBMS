#pragma once

enum BOARD_STATUS 
{
    BS_STARTUP,       //Haven't tried to find this ID yet
    BS_FOUND,         //Something responded to the ID
    BS_MISSING        //Nobody responded
};

class BMSModule
{
public:
    void readStatus();
    bool readModuleValues();
    float getCellVoltage(int cell);
    float getLowCellV();
    float getHighCellV();
    float getAverageV();
    float getLowTemp();
    float getHighTemp();
    float getAvgTemp();
    float getModuleVoltage();
    float getTemperature(int temp);
    void setAddress(int newAddr);
    int getAddress();
    bool isExisting();
    void setExists(bool ex);

private:
    float cellVolt[6];      // calculated as 16 bit value * 6.250 / 16383 = volts
    float moduleVolt;       // calculated as 16 bit value * 33.333 / 16383 = volts
    float temperatures[2];  // Don't know the proper scaling at this point
    bool exists;

    uint8_t moduleAddress;  //1 to 0x3E
};
