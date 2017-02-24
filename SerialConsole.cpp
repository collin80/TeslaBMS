/*
 * SerialConsole.cpp
 *
 Copyright (c) 2017 EVTV / Collin Kidder

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#include "SerialConsole.h"
#include "Logger.h"
template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; } //Lets us stream SerialUSB

SerialConsole::SerialConsole() {
    init();
}

void SerialConsole::init() {
    //State variables for serial console
    ptrBuffer = 0;
    state = STATE_ROOT_MENU;
    loopcount=0;
    cancel=false;
}

void SerialConsole::loop() {  
    if (SERIALCONSOLE.available()) {
        serialEvent();
    }
}

void SerialConsole::printMenu() {   
    Logger::console("\n*************SYSTEM MENU *****************");
    Logger::console("Enable line endings of some sort (LF, CR, CRLF)");
    Logger::console("Most commands case sensitive\n");
    Logger::console("GENERAL SYSTEM CONFIGURATION\n");
    Logger::console("   E = dump system EEPROM values");
    Logger::console("   h = help (displays this message)");
  
    Logger::console("   LOGLEVEL=%i - set log level (0=debug, 1=info, 2=warn, 3=error, 4=off)", Logger::getLogLevel());
    //Logger::console("   CANSPEED=%i - set first CAN bus speed (in thousands)", canHandlerEv.getBusSpeed() / 1000);

    /*
    Logger::console("\nBATTERY MANAGEMENT CONTROLS\n");
    Logger::console("   CAPACITY=%i - Capacity of battery pack in tenths ampere-hours", config->packCapacity);
    Logger::console("   AHLEFT=%i - Number of amp hours remaining in pack in tenths ampere-hours", config->packAHRemaining / 100000);
    Logger::console("   VOLTLIMHI=%i - High limit for pack voltage in tenths of a volt", config->highVoltLimit);
    Logger::console("   VOLTLIMLO=%i - Low limit for pack voltage in tenths of a volt", config->lowVoltLimit);
    Logger::console("   CELLLIMHI=%i - High limit for cell voltage in hundredths of a volt", config->highCellLimit);
    Logger::console("   CELLLIMLO=%i - Low limit for cell voltage in hundredths of a volt", config->lowCellLimit);
    Logger::console("   TEMPLIMHI=%i - High limit for pack and cell temperature in tenths of a degree C", config->highTempLimit);
    Logger::console("   TEMPLIMLO=%i - Low limit for pack and cell temperature in tenths of a degree C", config->lowTempLimit);
    */
}

/*	There is a help menu (press H or h or ?)

    Commands are submitted by sending line ending (LF, CR, or both)
 */
void SerialConsole::serialEvent() {
    int incoming;
    incoming = SERIALCONSOLE.read();
    if (incoming == -1) { //false alarm....
        return;
    }

    if (incoming == 10 || incoming == 13) { //command done. Parse it.
        handleConsoleCmd();
        ptrBuffer = 0; //reset line counter once the line has been processed
    } else {
        cmdBuffer[ptrBuffer++] = (unsigned char) incoming;
        if (ptrBuffer > 79)
            ptrBuffer = 79;
    }
}

void SerialConsole::handleConsoleCmd() {

    if (state == STATE_ROOT_MENU) {
        if (ptrBuffer == 1) { //command is a single ascii character
            handleShortCmd();
        } else { //if cmd over 1 char then assume (for now) that it is a config line
            handleConfigCmd();
        }
    }
}

/*For simplicity the configuration setting code uses four characters for each configuration choice. This makes things easier for
 comparison purposes.
 */
void SerialConsole::handleConfigCmd() {
    int i;
    int newValue;

    //Logger::debug("Cmd size: %i", ptrBuffer);
    if (ptrBuffer < 6)
        return; //4 digit command, =, value is at least 6 characters
    cmdBuffer[ptrBuffer] = 0; //make sure to null terminate
    String cmdString = String();
    unsigned char whichEntry = '0';
    i = 0;

    while (cmdBuffer[i] != '=' && i < ptrBuffer) {
        cmdString.concat(String(cmdBuffer[i++]));
    }
    i++; //skip the =
    if (i >= ptrBuffer)
    {
        Logger::console("Command needs a value..ie TORQ=3000");
        Logger::console("");
        return; //or, we could use this to display the parameter instead of setting
    }

    // strtol() is able to parse also hex values (e.g. a string "0xCAFE"), useful for enable/disable by device id
    newValue = strtol((char *) (cmdBuffer + i), NULL, 0);

    cmdString.toUpperCase();
    /*
    if (cmdString == String("CAN0SPEED")) {
        if (newValue >= 33 && newValue <= 1000) {
            sysPrefs->write(EESYS_CAN0_BAUD, (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            canHandlerEv.setup();
            Logger::console("Setting CAN0 speed to %i", newValue);
        }
        else Logger::console("Invalid speed. Enter a value between 33 and 1000");
    } else if (cmdString == String("LOGLEVEL")) {
        switch (newValue) {
        case 0:
            Logger::setLoglevel(Logger::Debug);
            Logger::console("setting loglevel to 'debug'");
            break;
        case 1:
            Logger::setLoglevel(Logger::Info);
            Logger::console("setting loglevel to 'info'");
            break;
        case 2:
            Logger::console("setting loglevel to 'warning'");
            Logger::setLoglevel(Logger::Warn);
            break;
        case 3:
            Logger::console("setting loglevel to 'error'");
            Logger::setLoglevel(Logger::Error);
            break;
        case 4:
            Logger::console("setting loglevel to 'off'");
            Logger::setLoglevel(Logger::Off);
            break;
        }
        if (!sysPrefs->write(EESYS_LOG_LEVEL, (uint8_t)newValue))
            Logger::error("Couldn't write log level!");
        sysPrefs->saveChecksum();   
    } else if (cmdString == String("CAPACITY") && bmsConfig ) {
        if (newValue >= 0 && newValue <= 6000) {
            bmsConfig->packCapacity = newValue;
            bms->saveConfiguration();
            Logger::console("Battery Pack Capacity set to: %d", bmsConfig->packCapacity);
        }
        else Logger::console("Invalid capacity please enter a value between 0 and 6000");
    } else if (cmdString == String("AHLEFT") && bmsConfig ) {
        if (newValue >= 0 && newValue <= 6000) {
            bmsConfig->packAHRemaining = newValue * 100000ul;
            bms->saveConfiguration();
            Logger::console("Battery Pack remaining capacity set to: %d", newValue);
        }
        else Logger::console("Invalid remaining capacity please enter a value between 0 and 6000");
    } else if (cmdString == String("VOLTLIMHI") && bmsConfig ) {
        if (newValue >= 0 && newValue <= 6000) {
            bmsConfig->highVoltLimit = newValue;
            bms->saveConfiguration();
            Logger::console("Battery High Voltage Limit set to: %d", bmsConfig->highVoltLimit);
        }
        else Logger::console("Invalid high voltage limit please enter a value between 0 and 6000");
    } else if (cmdString == String("VOLTLIMLO") && bmsConfig ) {
        if (newValue >= 0 && newValue <= 6000) {
            bmsConfig->lowVoltLimit = newValue;
            bms->saveConfiguration();
            Logger::console("Battery Low Voltage Limit set to: %d", bmsConfig->lowVoltLimit);
        }
        else Logger::console("Invalid low voltage limit please enter a value between 0 and 6000");
    } else if (cmdString == String("CELLLIMHI") && bmsConfig ) {
        if (newValue >= 0 && newValue <= 20000) {
            bmsConfig->highCellLimit = newValue;
            bms->saveConfiguration();
            Logger::console("Cell High Voltage Limit set to: %d", bmsConfig->highCellLimit);
        }
        else Logger::console("Invalid high voltage limit please enter a value between 0 and 20000");
    } else if (cmdString == String("CELLLIMLO") && bmsConfig ) {
        if (newValue >= 0 && newValue <= 20000) {
            bmsConfig->lowCellLimit = newValue;
            bms->saveConfiguration();
            Logger::console("Cell Low Voltage Limit set to: %d", bmsConfig->lowCellLimit);
        }
        else Logger::console("Invalid low voltage limit please enter a value between 0 and 20000");
    } else if (cmdString == String("TEMPLIMHI") && bmsConfig ) {
        if (newValue >= 0 && newValue <= 2000) {
            bmsConfig->highTempLimit = newValue;
            bms->saveConfiguration();
            Logger::console("Battery Temperature Upper Limit set to: %d", bmsConfig->highTempLimit);
        }
        else Logger::console("Invalid temperature upper limit please enter a value between 0 and 2000");
    } else if (cmdString == String("TEMPLIMLO") && bmsConfig ) {
        if (newValue >= -2000 && newValue <= 2000) {
            bmsConfig->lowTempLimit = newValue;
            bms->saveConfiguration();
            Logger::console("Battery Temperature Lower Limit set to: %d", bmsConfig->lowTempLimit);
        }
        else Logger::console("Invalid temperature lower limit please enter a value between -2000 and 2000");
    } else {
        Logger::console("Unknown command");
        updateWifi = false;
    } */
}

void SerialConsole::handleShortCmd() {
    uint8_t val;

    switch (cmdBuffer[0]) {
    case 'h':
    case '?':
    case 'H':
        printMenu();
        break;  
    case 'X':
        setup(); //this is probably a bad idea. Do not do this while connected to anything you care about - only for debugging in safety!
        break;
    }
}

/*
    if (SERIALCONSOLE.available()) 
    {     
        char y = SERIALCONSOLE.read();
        switch (y)
        {
        case '1': //ascii 1
            renumberBoardIDs();  // force renumber and read out
            break;
        case '2': //ascii 2
            SERIALCONSOLE.println();
            findBoards();
            break;
        case '3': //activate cell balance for 5 seconds 
            SERIALCONSOLE.println();
            SERIALCONSOLE.println("Balancing");
            cellBalance();
            break;
      case '4': //clear all faults on all boards, required after Reset or FPO (first power on)
       SERIALCONSOLE.println();
       SERIALCONSOLE.println("Clearing Faults");
       clearFaults();
      break;

      case '5': //read out the status of first board
       SERIALCONSOLE.println();
       SERIALCONSOLE.println("Reading status");
       readStatus(1);
      break;

      case '6': //Read out the limit setpoints of first board
       SERIALCONSOLE.println();
       SERIALCONSOLE.println("Reading Setpoints");
       readSetpoint(1);
       SERIALCONSOLE.println(OVolt);
       SERIALCONSOLE.println(UVolt);
       SERIALCONSOLE.println(Tset);
      break; 
                
      case '0': //Send all boards into Sleep state
       Serial.println();
       Serial.println("Sleep Mode");
       sleepBoards();
      break;

      case '9'://Pull all boards out of Sleep state
       Serial.println();
       Serial.println("Wake Boards");
       wakeBoards();
      break;          
                      
        }
    }     
 */



