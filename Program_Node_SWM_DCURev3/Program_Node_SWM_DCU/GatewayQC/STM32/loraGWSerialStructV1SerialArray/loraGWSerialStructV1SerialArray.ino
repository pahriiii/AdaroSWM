#include <RadioLib.h>
#include <IWatchdog.h>
#include <EEPROM.h>

#define CTRBYTE       42
// board wemosGatewayLoRaVer2, STM32G030C8T6
#define LR_RST     PA3
#define NSS        PA4
#define DIO0       PB1
#define DIO1       PA15
#define DIO2       PB3
#define LED        PC13
#define LORA_PWR   PA11

SX1276 radio = new Module(NSS, DIO0, LR_RST, DIO1);

#define SERIAL_BAUD   115200

typedef struct {
  uint8_t typeId;      //
  uint8_t nodeId[12];  //store this nodeId
  uint8_t cmd;     //type parameter
  uint32_t vParams;      //temperature maybe?
} Payload;
Payload theData;

typedef struct {
  uint8_t configid;
  uint8_t devid;
} Params;
Params  id;

String DEVID = "GW81";
long lastUpdate;
int updateRate = 60000; // update rate 1 menit
int countUpdate = 0;

void blinkLed(int n) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED, LOW);
    delay(200);
    digitalWrite(LED, HIGH);
    delay(200);
  }
}

byte nibble(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;

  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;

  return 0;  // Not a valid hexadecimal character
}

String getValue(String data, char separator, int index) // 2,2,3,5
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// save transmission states between loops
int transmissionState = ERR_NONE;

// flag to indicate transmission or reception state
bool transmitFlag = false;

// flag to indicate that a packet was received
volatile bool receivedFlag = false;

// disable interrupt when it's not needed
volatile bool enableInterrupt = true;

// flag to indicate that a packet was sent or received
volatile bool operationDone = false;

// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
void setFlag(void) {
  // check if the interrupt is enabled
  //  if (!enableInterrupt) {
  //    return;
  //  }
  // we got a packet, set the flag
  operationDone = true;
}

void sendDataLora(String str) {
  String typeIDStr = getValue(str, ',', 0);
  uint8_t typeID = typeIDStr.toInt();
  theData.typeId = typeID;
  String destIDStr = getValue(str, ',', 1);
  if (destIDStr.length()  >= 24) {
    hexCharacterStringToBytes(theData.nodeId, destIDStr.c_str());
  } else {
    return;
  }
  String cmdStr = getValue(str, ',', 2);
  uint8_t cmd = cmdStr.toInt();
  String paramStr = getValue(str, ',', 3);
  uint32_t vParams = paramStr.toInt();

  theData.cmd = cmd;
  theData.vParams = vParams;
  byte len = sizeof(theData);
  byte byteArray[len + 1];
  memcpy(byteArray, (const void*)(&theData), len);
  byteArray[len] = calculateCRC(byteArray, len);
  transmissionState = radio.startTransmit(byteArray, len + 1);
  transmitFlag = true;
}
void hexCharacterStringToBytes(uint8_t *byteArray, const char *hexString) {
  bool oddLength = strlen(hexString) & 1;

  uint8_t currentByte = 0;
  uint8_t byteIndex = 0;

  for (uint8_t charIndex = 0; charIndex < strlen(hexString); charIndex++) {
    bool oddCharIndex = charIndex & 1;

    if (oddLength) {
      // If the length is odd
      if (oddCharIndex) {
        // odd characters go in high nibble
        currentByte = nibble(hexString[charIndex]) << 4;
      } else {
        // Even characters go into low nibble
        currentByte |= nibble(hexString[charIndex]);
        byteArray[byteIndex++] = currentByte;
        currentByte = 0;
      }
    } else {
      // If the length is even
      if (!oddCharIndex) {
        // Odd characters go into the high nibble
        currentByte = nibble(hexString[charIndex]) << 4;
      } else {
        // Odd characters go into low nibble
        currentByte |= nibble(hexString[charIndex]);
        byteArray[byteIndex++] = currentByte;
        currentByte = 0;
      }
    }
  }
}

//Function to calculate CRC over an array 'ar' up to element 's'
byte calculateCRC(byte ar[], byte s) {
  byte rtn = 0;;
  for (byte i = 0; i < s; i++) {
    rtn ^= ar[i];
  }
  return rtn;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(SERIAL_BAUD);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  //#ifdef LORA_PWR
  pinMode(LORA_PWR, OUTPUT);
  digitalWrite(LORA_PWR, LOW);
  //#endif
  delay(10);

  if (IWatchdog.isReset(true)) {
    // LED blinks to indicate reset
    blinkLed(10);
  }

  EEPROM.get(0, id);
  if (id.configid == CTRBYTE) {
    DEVID = "GW" + String(id.devid);
  } else {
    id.configid = CTRBYTE;
    id.devid = 65;
    DEVID = "GW65";
    EEPROM.put(0, id);
  }

  int state = radio.begin(920.0, 125.0, 9, 7, SX127X_SYNC_WORD, 17, 8, 0);
  if (state == ERR_NONE) {
    //Serial.println(F("success!"));
  } else {
    //Serial.print(F("failed, code "));
    //Serial.println(state);
    blinkLed(20);
    while (true);
  }

  // set the function that will be called
  // when new packet is received
  radio.setDio0Action(setFlag);

  state = radio.startReceive();
  if (state == ERR_NONE) {
    // Serial.println(F("success!"));
  } else {
    //Serial.print(F("failed, code "));
    //Serial.println(state);
    while (true);
  }

  // Init the watchdog timer with 10 seconds timeout
  IWatchdog.begin(10000000);
  lastUpdate = millis();
  blinkLed(3);
}

void loop() {
  // put your main code here, to run repeatedly:
  IWatchdog.reload();

  if (Serial.available() > 0) {
    String str = Serial.readStringUntil('#');  //2,2,3,4#
    String tmp = getValue(str, ',', 0);
    if (tmp == "GW") {
      String dvid = getValue(str, ',', 1);
      DEVID = "GW" + dvid;
      id.devid = dvid.toInt();
      EEPROM.put(0, id);
    } else {
      sendDataLora(str);
      //Serial.begin(115200);
    }
  }

  if (operationDone) {
    //enableInterrupt = false;
    // reset flag
    operationDone = false;
    if (transmitFlag) {
      // the previous operation was transmission, listen for response
      // print the result
      if (transmissionState == ERR_NONE) {
        // packet was successfully sent
        //Serial.println(F("transmission finished!"));
        blinkLed(2);

      } else {
        //Serial.print(F("failed, code "));
        //Serial.println(transmissionState);

      }

      // listen for response
      radio.startReceive();
      transmitFlag = false;
    } else {
      byte len = sizeof(theData);
      byte byteArray[len + 1]; //byteArray[23+1]
      int state = radio.readData(byteArray, len + 1);
      if (state == ERR_NONE) {
        theData = *(Payload*)byteArray;
        if (byteArray[len] == calculateCRC(byteArray, len)) {
          if (theData.typeId != 0) {
            String str = String(theData.typeId, DEC) + ",";
            for (int i = 0; i < 12; i++) {
              if (theData.nodeId[i] < 0x10) {
                str += "0";
              }
              str += String(theData.nodeId[i], HEX);
            }
            str += ",";
            str += String(theData.cmd, DEC) + ",";
            str += String(theData.vParams, DEC);  // 2,2,1,4$
            str += "$";
            Serial.print(str);
            blinkLed(2);

          }
        }
      }
      radio.startReceive();
      transmitFlag = false;
    }

  }

  long wkt = millis();
  if ((wkt - lastUpdate) > updateRate) {
    lastUpdate = wkt;
    String str1 = String(DEVID) + "," + String(countUpdate) + "$";
    Serial.print(str1);
    //delay(10);
    //Serial.begin(115200);
    countUpdate++;
  }
}
