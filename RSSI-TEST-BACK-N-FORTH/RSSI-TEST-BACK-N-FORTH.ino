#include <RadioLib.h>
#include <IWatchdog.h>
#include <EEPROM.h>

//L0 LAMA
// #define LED       PC13
// #define LR_RST    PA3
// #define NSS       PA4
// #define DIO0      PB1
// #define DIO1      PA15
// #define DIO2      PB3
// #define LORA_ON   PA11  // active LOW
// #define OUT       PB1

//STM32G0
// #define LED       PC13
// #define LR_RST    PA3
// #define NSS       PA4
// #define DIO0      PB1
// #define DIO1      PA15
// #define DIO2      PB3
// #define LORA_ON   PA11  // active LOW
// #define OUT       PB1


//L0 NODE SWM
#define HAL_PWR_MODULE_ENABLED
#define LED PC13
#define NSS PA4
#define DIO0 PB2
#define DIO1 PB1
#define LORA_RST PC13
#define LORA_ON PA11
#define SENSOR PA0
#define SENSOR_ON PA1

//G0 NODE DCU
// #define LED PC13
// #define LR_RST PA3
// #define NSS PA4
// #define DIO0 PB0
// #define DIO1 PB1
// #define DIO2 PB2
// #define LORA_PWR PA11  // active LOW
// #define VSOURCE PA1

// SX1276 radio = new Module(NSS, DIO0, LR_RST, DIO1);
SX1276 radio = new Module(NSS, DIO0, LORA_RST, DIO1);

int transmissionState = RADIOLIB_ERR_NONE;
bool transmitFlag = false;
volatile bool operationDone = false;
unsigned long previousTransmissionTime = 0;
const unsigned long transmissionInterval = 20000;  // Set the transmission interval in milliseconds

void setup() {
  Serial.begin(921600);

  //STM32G0
  // pinMode(LED, OUTPUT);
  // digitalWrite(LED, HIGH);
  // pinMode(LORA_ON, OUTPUT);
  // digitalWrite(LORA_ON, LOW);
  // pinMode(OUT, OUTPUT);
  // digitalWrite(OUT, LOW);

  //L0 LAMA
  // pinMode(LED, OUTPUT);
  // digitalWrite(LED, HIGH);
  // pinMode(LORA_ON, OUTPUT);
  // digitalWrite(LORA_ON, LOW);
  // pinMode(OUT, OUTPUT);
  // digitalWrite(OUT, LOW);

  //L0 NODE SWM
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  pinMode(LORA_ON, OUTPUT);
  digitalWrite(LORA_ON, LOW);

  //G0 NODE DCU
  // pinMode(LED, OUTPUT);
  // digitalWrite(LED, HIGH);
  // //#ifdef LORA_PWR
  // pinMode(LORA_PWR, OUTPUT);
  // digitalWrite(LORA_PWR, LOW);

  if (IWatchdog.isReset(true)) {
    blinkLed(10);
  }

  int state = radio.begin(901.0, 62.5, 9, 7, RADIOLIB_SX127X_SYNC_WORD, 17, 16, 1);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Initializing ... success!"));
  } else {
    Serial.print(F("Initialization failed, code "));
    Serial.println(state);
    while (true)
      ;
  }

  radio.setDio0Action(setFlag, RISING);

  // Start listening for LoRa packets
  Serial.print(F("Starting to listen ... "));
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("Failed to start listening, code "));
    Serial.println(state);
    while (true)
      ;
  }

  blinkLed(3);
  analogReadResolution(12);
  IWatchdog.begin(10000000);
}



void loop() {
  IWatchdog.reload();
  unsigned long currentMillis = millis();

  if (currentMillis - previousTransmissionTime >= transmissionInterval) {
    previousTransmissionTime = currentMillis;

    // It's time to transmit data
    // Start the transmission (asynchronous)
    transmissionState = radio.startTransmit("30");
    Serial.println("Transmit: '30'");
    blinkLed(2);
    transmitFlag = true;
  }

  if (operationDone) {
    operationDone = false;

    if (transmitFlag) {
      if (transmissionState == RADIOLIB_ERR_NONE) {
        Serial.println(F("Transmission finished!"));
      } else {
        Serial.print(F("Transmission failed, code "));
        Serial.println(transmissionState);
      }

      // Start listening for the next packet
      radio.startReceive();
      transmitFlag = false;
    } else {
      String strr;
      int state = radio.readData(strr);
      if (state == RADIOLIB_ERR_NONE) {
        // Packet was successfully received
        // Continue with your program logic as needed
        strr += " / " + String(radio.getRSSI()) + ",";
        strr += String(radio.getSNR()) + ",";
        strr += String(radio.getFrequencyError());
        Serial.println(strr);

        blinkLed(2);
      }
    }

    Serial.flush();
  }
}

void setFlag(void) {
  operationDone = true;
}

void blinkLed(int n) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED, LOW);
    delay(200);
    digitalWrite(LED, HIGH);
    delay(200);
  }
}
