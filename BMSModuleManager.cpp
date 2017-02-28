#include "config.h"
#include "BMSModuleManager.h"
#include "BMSUtil.h"
#include "Logger.h"

extern EEPROMSettings settings;

BMSModuleManager::BMSModuleManager()
{
    for (int i = 1; i <= MAX_MODULE_ADDR; i++) {
        modules[i].setExists(false);
        modules[i].setAddress(i);
    }
}

void BMSModuleManager::balanceCells()
{
    uint8_t payload[4];
    uint8_t buff[30];
    uint8_t balance = 0;//bit 0 - 5 are to activate cell balancing 1-6
  
    for (int address = 1; address <= MAX_MODULE_ADDR; address++)
    {
        balance = 0;
        for (int i = 0; i < 6; i++)
        {
            if (settings.balanceVoltage < modules[address].getCellVoltage(i))
            {
                balance = balance | (1<<i);
            }
        }
  
        if (balance != 0) //only send balance command when needed
        {
            payload[0] = address << 1;
            payload[1] = REG_BAL_TIME;
            payload[2] = 0x05; //5 second balance limit, if not triggered to balance it will stop after 5 seconds
            BMSUtil::sendData(payload, 3, true);
            delay(2);
            BMSUtil::getReply(buff, 30);

            payload[0] = address << 1;
            payload[1] = REG_BAL_CTRL;
            payload[2] = balance; //write balance state to register
            BMSUtil::sendData(payload, 3, true);
            delay(2);
            BMSUtil::getReply(buff, 30);

            if (Logger::isDebug()) //read registers back out to check if everthing is good
            {
                delay(50);
                payload[0] = address << 1;
                payload[1] = REG_BAL_TIME;
                payload[2] = 1; //
                BMSUtil::sendData(payload, 3, false);
                delay(2);
                BMSUtil::getReply(buff, 30);
         
                payload[0] = address << 1;
                payload[1] = REG_BAL_CTRL;
                payload[2] = 1; //
                BMSUtil::sendData(payload, 3, false);
                delay(2);
                BMSUtil::getReply(buff, 30);
            }
        }
    }
}

/*
 * Try to set up any unitialized boards. Send a command to address 0 and see if there is a response. If there is then there is
 * still at least one unitialized board. Go ahead and give it the first ID not registered as already taken.
 * If we send a command to address 0 and no one responds then every board is inialized and this routine stops.
 * Don't run this routine until after the boards have already been enumerated.\
 * Note: The 0x80 conversion it is looking might in theory block the message from being forwarded so it might be required
 * To do all of this differently. Try with multiple boards. The alternative method would be to try to set the next unused
 * address and see if any boards respond back saying that they set the address. 
 */
void BMSModuleManager::setupBoards()
{
    uint8_t payload[3];
    uint8_t buff[10];

    payload[0] = 0;
    payload[1] = 0;
    payload[2] = 0;  
    while (1 == 1)
    {
        BMSUtil::sendData(payload, 3, false);
        delay(3);
        if (BMSUtil::getReply(buff, 10) > 2)
        {
            if (buff[0] == 0x80 && buff[1] == 0 && buff[2] == 0)
            {
                Logger::debug("00 found");
                //look for a free address to use
                for (int y = 1; y < 63; y++) 
                {
                    if (!modules[y].isExisting())
                    {
                        payload[0] = 0;
                        payload[1] = REG_ADDR_CTRL;
                        payload[2] = y | 0x80;
                        BMSUtil::sendData(payload, 3, true);
                        delay(3);
                        if (BMSUtil::getReply(buff, 10) > 2)
                        {
                            if (buff[0] == (y << 1) && buff[1] == REG_ADDR_CTRL && buff[2] == (y | 0x80)) 
                            {
                                modules[y].setExists(true);
                                numFoundModules++;
                                Logger::debug("Address assigned");
                            }
                        }
                        break; //quit the for loop
                    }
                }
            }
            else break; //nobody responded properly to the zero address so our work here is done.
        }
    }
}

/*
 * Iterate through all 62 possible board addresses (1-62) to see if they respond
 */
void BMSModuleManager::findBoards()
{
    uint8_t payload[3];
    uint8_t buff[8];

    numFoundModules = 0;

    for (int x = 1; x <= MAX_MODULE_ADDR; x++) modules[x].setExists(false);
    
    payload[1] = 0; //read registers starting at 0
    payload[2] = 1; //read one byte
    for (int x = 1; x <= MAX_MODULE_ADDR; x++)
    {
        modules[x].setExists(false);
        payload[0] = x << 1;
        BMSUtil::sendData(payload, 3, false);
        delay(2);
        if (BMSUtil::getReply(buff, 8) > 4)
        {
            if (buff[0] == (x << 1) && buff[1] == 0 && buff[2] == 1 && buff[4] > 0) {
                modules[x].setExists(true);
                numFoundModules++;
                Logger::debug("Found module with address: %X", x); 
            }
        }
    }
}


/*
 * Force all modules to reset back to address 0 then set them all up in order so that the first module
 * in line from the master board is 1, the second one 2, and so on.
*/
void BMSModuleManager::renumberBoardIDs()
{
  uint8_t payload[3];
  uint8_t buff[8];
  payload[0] = 0x3F << 1; //broadcast the reset command
  payload[1] = 0x3C;//reset
  payload[2] = 0xA5;//data to cause a reset
  BMSUtil::sendData(payload, 3, true);
  delay(20);
  setupBoards();    //then assign them all consecutive addresses in order
}

/*
After a RESET boards have their faults written due to the hard restart or first time power up, this clears thier faults
*/
void BMSModuleManager::clearFaults()
{
  uint8_t payload[3];
  uint8_t buff[8];
  payload[0] = 0x7F; //broadcast
  payload[1] = REG_ALERT_STATUS;//Alert Status
  payload[2] = 0x80;//data to cause a reset
  BMSUtil::sendData(payload, 3, true);
  delay(2);
  BMSUtil::getReply(buff, 8);
  payload[0] = 0x7F; //broadcast
  payload[2] = 0x00;//data to clear
  BMSUtil::sendData(payload, 3, true);
  delay(2);
  BMSUtil::getReply(buff, 8);
  
  payload[0] = 0x7F; //broadcast
  payload[1] = REG_FAULT_STATUS;//Fault Status
  payload[2] = 0x08;//data to cause a reset
  BMSUtil::sendData(payload, 3, true);
  delay(2);
  BMSUtil::getReply(buff, 8);
  payload[0] = 0x7F; //broadcast
  payload[2] = 0x00;//data to clear
  BMSUtil::sendData(payload, 3, true);
  delay(2);
  BMSUtil::getReply(buff, 8);
}

/*
Puts all boards on the bus into a Sleep state, very good to use when the vehicle is a rest state. 
Pulling the boards out of sleep only to check voltage decay and temperature when the contactors are open.
*/

void BMSModuleManager::sleepBoards()
{
  uint8_t payload[3];
  uint8_t buff[8];
  payload[0] = 0x7F; //broadcast
  payload[1] = REG_IO_CTRL;//IO ctrl start
  payload[2] = 0x04;//write sleep bit
  BMSUtil::sendData(payload, 3, true);
  delay(2);
  BMSUtil::getReply(buff, 8);
}

/*
Wakes all the boards up and clears thier SLEEP state bit in the Alert Status Registery
*/

void BMSModuleManager::wakeBoards()
{
  uint8_t payload[3];
  uint8_t buff[8];
  payload[0] = 0x7F; //broadcast
  payload[1] = REG_IO_CTRL;//IO ctrl start
  payload[2] = 0x00;//write sleep bit
  BMSUtil::sendData(payload, 3, true);
  delay(2);
  BMSUtil::getReply(buff, 8);
  
  payload[0] = 0x7F; //broadcast
  payload[1] = REG_ALERT_STATUS;//Fault Status
  payload[2] = 0x04;//data to cause a reset
  BMSUtil::sendData(payload, 3, true);
  delay(2);
  BMSUtil::getReply(buff, 8);
  payload[0] = 0x7F; //broadcast
  payload[2] = 0x00;//data to clear
  BMSUtil::sendData(payload, 3, true);
  delay(2);
  BMSUtil::getReply(buff, 8);
}

void BMSModuleManager::getAllVoltTemp()
{
    packVolt = 0.0f;
    for (int x = 1; x <= MAX_MODULE_ADDR; x++)
    {
        if (modules[x].isExisting()) 
        {
            Logger::debug("Module %i exists. Reading voltage and temperature values", x);
            modules[x].readModuleValues();
            Logger::debug("Module voltage: %f", modules[x].getModuleVoltage());
            Logger::debug("Lowest Cell V: %f     Highest Cell V: %f", modules[x].getLowCellV(), modules[x].getHighCellV());
            Logger::debug("Temp1: %f       Temp2: %f", modules[x].getTemperature(0), modules[x].getTemperature(1));
            packVolt += modules[x].getModuleVoltage();
        }
    }
}

