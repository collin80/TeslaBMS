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

float cellVolt[6];       // calculated as 16bit value * 6.250 / 16383 = volts
float moduleVolt;        // calculated as 16 bit value * 33.333 / 16383 = volts]
uint8_t serBuff[128];

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
    if (isWrite) data[0] = data[0] | 1;
    Serial.write(data, dataLen);
    if (isWrite) Serial.write(genCRC(data, dataLen));

    SerialUSB.print("Sending: ");
    for (int x = 0; x < dataLen; x++) {
        SerialUSB.print(data[x], HEX);
        SerialUSB.print(" ");
    }
    SerialUSB.println(genCRC(data, dataLen), HEX);
}

void getReply()
{
   SerialUSB.print("Reply: ");
   while (Serial1.available())
   {
      SerialUSB.print(Serial1.read(), HEX);
   }
   SerialUSB.println();
}

void setup() {
  delay(4000);
  Serial.begin(612500);
  SerialUSB.println("Fired up serial at 612500 baud!");
}

void loop() {
    uint8_t payload[8];
  payload[0] = 2;
  payload[1] = 0x30;
  payload[2] = 0b00111101;
  sendData(payload, 3, true);
  delay(3);
  getReply();
  delay(200);
}


