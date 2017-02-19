#include <chip.h>

#define REG_DEV_STATUS      0
#define REG_GPAI            1
#define REG_VCELL1          3
#define REG_VCELL2          5
#define REG_VCELL3          7
#define REG_VCELL4          9
#define REG_VCELL5          0xB
#define REG_VCELL6          0xD
#define REG_TEMPERATURE1    0xF
#define REG_TEMPERATURE2    0x11
#define REG_ALERT_STATUS    0x20
#define REG_FAULT_STATUS    0x21
#define REG_COV_FAULT       0x22
#define REG_CUV_FAULT       0x23
#define REG_ADC_CTRL        0x30
#define REG_IO_CTRL         0x31
#define REG_BAL_CTRL        0x32
#define REG_BAL_TIME        0x33
#define REG_ADC_CONV        0x34
#define REG_ADDR_CTRL       0x3B

#define MAX_MODULE_ADDR     0x3E

//Set to the proper port for your USB connection - SERIALCONSOLE on Due (Native) or Serial for Due (Programming) or Teensy
#define SERIALCONSOLE   SerialUSB

//Define this to be the serial port the Tesla BMS modules are connected to.
//On the Due you need to use a USART port (SERIAL, Serial2, Serial3) and update the call to serialSpecialInit if not SERIAL
#define SERIAL  Serial1

//There can be only 62 modules but 63 are allocated because the first valid module address is 1 not 0. This way you can
//ignore the first entry in the array and store things 1-63 just like the bus addresses.
float cellVolt[MAX_MODULE_ADDR + 1][6];     // calculated as 16 bit value * 6.250 / 16383 = volts
float moduleVolt[MAX_MODULE_ADDR + 1];      // calculated as 16 bit value * 33.333 / 16383 = volts
float packVolt;                 // All modules added together
uint16_t temperatures[MAX_MODULE_ADDR + 1][2];   // Storage for temperature readings
uint8_t serBuff[128];
uint8_t boards[MAX_MODULE_ADDR + 1];

float balVolt = 3.01; // CHANGE - arbitrairy balance value set to balance on my pack

uint8_t actBoards = 0;     // Number of active boards/slaves

uint8_t decodeuart = 0;    // Transfer uart data to serial output

enum BOARD_STATUS 
{
    BS_STARTUP,       //Haven't tried to find this ID yet
    BS_FOUND,         //Something responded to the ID
    BS_MISSING        //Nobody responded
};

//This code only applicable to Due to fixup lack of functionality in the arduino core.
#if defined (__arm__) && defined (__SAM3X8E__)
void serialSpecialInit(Usart *pUsart, uint32_t baudRate)
{
  // Reset and disable receiver and transmitter
  pUsart->US_CR = UART_CR_RSTRX | UART_CR_RSTTX | UART_CR_RXDIS | UART_CR_TXDIS;

  // Configure mode
  pUsart->US_MR =  US_MR_CHRL_8_BIT | US_MR_NBSTOP_1_BIT | UART_MR_PAR_NO | US_MR_USART_MODE_NORMAL | 
                   US_MR_USCLKS_MCK | US_MR_CHMODE_NORMAL | US_MR_OVER; // | US_MR_INVDATA;

  //Get the integer divisor that can provide the baud rate
  int divisor = SystemCoreClock / baudRate;
  int error1 = abs(baudRate - (SystemCoreClock / divisor)); //find out how close that is to the real baud
  int error2 = abs(baudRate - (SystemCoreClock / (divisor + 1))); //see if bumping up one on the divisor makes it a better match

  if (error2 < error1) divisor++;   //If bumping by one yielded a closer rate then use that instead

  // Configure baudrate including the optional fractional divisor possible on USART
  pUsart->US_BRGR = (divisor >> 3) | ((divisor & 7) << 16);

  // Enable receiver and transmitter
  pUsart->US_CR = UART_CR_RXEN | UART_CR_TXEN;
}
#endif

uint8_t genCRC(uint8_t *input, int lenInput)
{
    uint8_t generator = 0x07;
    uint8_t crc = 0;

    for (int x = 0; x < lenInput; x++)
    {
        crc ^= input[x]; /* XOR-in the next input byte */

        for (int i = 0; i < 8; i++)
        {
            if ((crc & 0x80) != 0)
            {
                crc = (uint8_t)((crc << 1) ^ generator);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

void sendData(uint8_t *data, uint8_t dataLen, bool isWrite)
{
    uint8_t addrByte = data[0];
    if (isWrite) addrByte |= 1;
    SERIAL.write(data[0]);
    SERIAL.write(&data[1], dataLen - 1);  //assumes that there are at least 2 bytes sent every time. There should be, addr and cmd at the least.
    if (isWrite) SERIAL.write(genCRC(data, dataLen));

    if (decodeuart == 1) 
    {
        SERIALCONSOLE.print("Sending: ");
        for (int x = 0; x < dataLen; x++) {
            SERIALCONSOLE.print(data[x], HEX);
            SERIALCONSOLE.print(" ");
        }
        SERIALCONSOLE.println(genCRC(data, dataLen), HEX);
    }
}

int getReply(uint8_t *data)
{  
    int numBytes = 0; 
    if (decodeuart == 1) SERIALCONSOLE.print("Reply: ");
    while (SERIAL.available())
    {
        data[numBytes] = SERIAL.read();
        if (decodeuart == 1) SERIALCONSOLE.print(data[numBytes], HEX);
        numBytes++;
    }
    if (decodeuart == 1) SERIALCONSOLE.println();
    return numBytes;
}

void cellBalance()
{
    uint8_t payload[4];
    uint8_t buff[30];
    uint8_t balance = 0;//bit 0 - 5 are to activate cell balancing 1-6
  
    for (int address = 1; address <= MAX_MODULE_ADDR; address++)
    {
        balance = 0;
        for (int i = 0; i < 6; i++)
        {
            if (balVolt < cellVolt[address][i])
            {
                balance = balance | (1<<i);
            }
        }
  
        if (balance != 0) //only send balance command when needed
        {
            payload[0] = address << 1;
            payload[1] = REG_BAL_TIME;
            payload[2] = 0x05; //5 second balance limit, if not triggered to balance it will stop after 5 seconds
            sendData(payload, 3, true);
            delay(2);
            getReply(buff);

            payload[0] = address << 1;
            payload[1] = REG_BAL_CTRL;
            payload[2] = balance; //write balance state to register
            sendData(payload, 3, true);
            delay(2);
            getReply(buff);

            if (decodeuart == 1) //read registers back out to check if everthing is good
            {
                delay(50);
                payload[0] = address << 1;
                payload[1] = REG_BAL_TIME;
                payload[2] = 1; //
                sendData(payload, 3, false);
                delay(2);
                getReply(buff);
         
                payload[0] = address << 1;
                payload[1] = REG_BAL_CTRL;
                payload[2] = 1; //
                sendData(payload, 3, false);
                delay(2);
                getReply(buff);
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
void setupBoards()
{
    uint8_t payload[3];
    uint8_t buff[10];

    payload[0] = 0;
    payload[1] = 0;
    payload[2] = 0;  
    while (1 == 1)
    {
        sendData(payload, 3, false);
        delay(3);
        if (getReply(buff) > 2)
        {
            if (buff[0] == 0x80 && buff[1] == 0 && buff[2] == 0)
            {
                if (decodeuart == 1) SERIALCONSOLE.println("00 found");
                //look for a free address to use
                for (int y = 1; y < 64; y++) 
                {
                    if (boards[y] == BS_MISSING)
                    {
                        payload[0] = 0;
                        payload[1] = REG_ADDR_CTRL;
                        payload[2] = y | 0x80;
                        sendData(payload, 3, true);
                        delay(3);
                        if (getReply(buff) > 2)
                        {
                            if (buff[0] == (y << 1) && buff[1] == REG_ADDR_CTRL && buff[2] == (y | 0x80)) 
                            {
                                boards[y] = BS_FOUND; //Success!
                                actBoards++;
                                if (decodeuart == 1) SERIALCONSOLE.println("Address assigned");
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
 * Iterate through all 63 possible board addresses (1-63) to see if they respond
 */
void findBoards()
{
    uint8_t payload[3];
    uint8_t buff[8];

    actBoards = 0;

    payload[1] = 0; //read registers starting at 0
    payload[2] = 1; //read one byte
    for (int x = 1; x <= MAX_MODULE_ADDR; x++) 
    {
        boards[x] = BS_MISSING;
        payload[0] = x << 1;
        sendData(payload, 3, false);
        delay(2);
        if (getReply(buff) > 4)
        {
            if (buff[0] == (x << 1) && buff[1] == 0 && buff[2] == 1 && buff[4] > 0) {
                boards[x] = BS_FOUND;
                actBoards++;              
                SERIALCONSOLE.print("Found module with address: ");
                SERIALCONSOLE.println(x, HEX);
            }
        }
    }
}

void renumber()
{
  uint8_t payload[3];
  uint8_t buff[8];
  while (actBoards != 0)
  {
    for (int x = 1; x <= MAX_MODULE_ADDR; x++)
    {
      if (boards[x] != BS_MISSING)
      {
        payload[0] = x << 1;
        payload[1] = 0x3c;//reset
        payload[2] = 0xa5;//data to cause a reset
        sendData(payload, 3, true);
        delay(2);
      }
      
    }
    findBoards();
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(actBoards);
  }
  delay(10);
  setupBoards();
}


bool getModuleVoltage(uint8_t address)
{
    uint8_t payload[4];
    uint8_t buff[30];
    if (address < 1 || address > MAX_MODULE_ADDR) return false;
    payload[0] = address << 1;
    payload[1] = REG_ADC_CTRL;
    payload[2] = 0b00111101; //ADC Auto mode, read every ADC input we can (Both Temps, Pack, 6 cells)
    sendData(payload, 3, true);
    delay(2);
    getReply(buff);     //TODO: we're not validating the reply here. Perhaps check to see if a valid reply came back
    payload[1] = REG_ADC_CONV;
    payload[2] = 1;
    sendData(payload, 3, true);
    delay(2);
    getReply(buff);    //TODO: we're not validating the reply here. Perhaps check to see if a valid reply came back
    payload[1] = REG_GPAI; //start reading registers at the module voltage registers
    payload[2] = 0x12; //read 18 bytes (Each value takes 2 - ModuleV, CellV1-6, Temp1, Temp2)
    sendData(payload, 3, false);
    delay(2);
    if (getReply(buff) > 0x14)
    {
        if (buff[0] == (address << 1) && buff[1] == REG_GPAI && buff[2] == 0x12)
        {
            //payload is 2 bytes gpai, 2 bytes for each of 6 cell voltages, 2 bytes for each of two temperatures (18 bytes of data)
            moduleVolt[address - 1] = (buff[3] * 256 + buff[4]) * 0.002034609f;
            for (int i = 0; i < 6; i++) cellVolt[address - 1][i] = (buff[5 + (i * 2)] * 256 + buff[6 + (i * 2)]) * 0.000381493f;
            temperatures[address - 1][0] = (buff[17] * 256 + buff[18]);
            temperatures[address - 1][1] = (buff[19] * 256 + buff[20]);
            return true;
        }        
    }
    return false;
}

void setup() 
{
    delay(4000);
    SERIALCONSOLE.begin(115200);
    SERIALCONSOLE.println("Starting up!");
    SERIAL.begin(612500);
#if defined (__arm__) && defined (__SAM3X8E__)
    serialSpecialInit(USART0, 612500); //required for Due based boards as the stock core files don't support 612500 baud.
#endif
    SERIALCONSOLE.println("Fired up serial at 612500 baud!");
    for (int x = 1; x <= MAX_MODULE_ADDR; x++) boards[x] = BS_STARTUP;
    findBoards();
    for (int x = 1; x <= MAX_MODULE_ADDR; x++) SERIALCONSOLE.println(boards[x]);
}

void loop() 
{
    if (SERIALCONSOLE.available()) 
    {     // force renumber and read out
        char y = SERIALCONSOLE.read();
        switch (y)
        {
        case '1': //ascii 1
            renumber();
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
        }
    }    

    delay(500);
    getModuleVoltage(1);
    SERIALCONSOLE.println(moduleVolt[0]);
    SERIALCONSOLE.println();
    for (int y = 1; y <= MAX_MODULE_ADDR; y++) 
    {
        if (boards[y] == BS_FOUND)
        {
            getModuleVoltage(y);
            SERIALCONSOLE.println();
            SERIALCONSOLE.print("Slave Address ");
            SERIALCONSOLE.println(y);
            SERIALCONSOLE.print("Module voltage: ");
            SERIALCONSOLE.println(moduleVolt[y-1]);
            SERIALCONSOLE.print("Cell voltages: ");
            for (int x = 0; x < 6; x++) 
            {
              SERIALCONSOLE.print(cellVolt[y-1][x]); 
              SERIALCONSOLE.print(", ");
            }
            SERIALCONSOLE.println();
            SERIALCONSOLE.print("Temperatures : ");
            SERIALCONSOLE.print(temperatures[y-1][0]);
            SERIALCONSOLE.print(", ");
            SERIALCONSOLE.print(temperatures[y-1][1]);
            SERIALCONSOLE.println();           
        }
    } 
}

