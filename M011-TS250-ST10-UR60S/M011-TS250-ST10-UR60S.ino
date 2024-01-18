#include <RadioLib.h>
#include "STM32LowPower.h"
#include "backup.h"

// #define HAL_PWR_MODULE_ENABLED

// #define LED       PC13
// #define NSS       PA4
// #define DIO0      PB1
// #define DIO1      PA15
// //#define DIO2      PB3
// #define LORA_RST  PB14
// //#define WAKE_UP   PA0
// //#define SDA       PB9
// //#define SCL       PB8
// //#define RXD       PA10
// //#define TXD       PA9
// #define LORA_ON   PA11
// #define SENSOR    PA0
// #define SENSOR_ON PA1
#define HAL_PWR_MODULE_ENABLED
#define LED       PC13
#define NSS       PA4
#define DIO0      PB2
#define DIO1      PB1
#define LORA_RST  PC14
#define LORA_ON   PA11
#define SENSOR    PA0
#define SENSOR_ON PA1

#define lowPin    PB10
#define inp       PB11

#define CONTROL_WORD   0
#define SAMPLING_REG   1
#define METER_REG      2
#define STA_REG        3
#define SAMPLING_RATE  100
#define UPDATE_RATE    600
#define THRESHOLD      250

SX1276 lora = new Module(NSS, DIO0, LORA_RST, DIO1);

String DEVID = "M009";
uint32_t meterCounter;
uint32_t samplingCounter;
uint32_t st1;
uint32_t sts1;
uint32_t sts2;

void blinkLed(int n) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED, LOW);
    delay(2);
    digitalWrite(LED, HIGH);
    delay(2);
  }
}

String xorChecksum(String s) {
  byte b = s.charAt(0);
  for (int i = 1; i < s.length(); i++) {
    b = b ^ s.charAt(i);
  }
  String checksum = String(b, HEX);
  if (checksum.length() == 1) checksum = "0" + checksum;
  return checksum;
}

void sendDataSensor() {

  int state = lora.begin(920.0, 125.0, 9, 7, SX127X_SYNC_WORD, 17, 8, 0);
  if (state == ERR_NONE) {
    //Serial.println(F("success!"));
  } else {
    //Serial.print(F("failed, code "));
    //Serial.println(state);
    blinkLed(20);
    while (true);
  }

  uint32_t count = getBackupRegister(METER_REG);
  String str = String(DEVID) + "," + String(count);
  String str1 = xorChecksum(str);
  str += ":" + str1 + "$";
  state = lora.scanChannel();
  int cacah = 0;
  // tunggu sampai channel nya free
  while ((state == PREAMBLE_DETECTED) && (cacah < 5)) {
    delay(random(300, 1000));
    state = lora.scanChannel();
    cacah++;
  }

  state = lora.transmit(str.c_str());
  if (state == ERR_NONE) {
    // the packet was successfully transmitted
    blinkLed(1);

  }

  //Serial.println();
}

void setup() {
  // put your setup code here, to run once:
   Serial.begin(115200);

  enableBackupDomain();
  LowPower.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  pinMode(SENSOR, INPUT_ANALOG);
  pinMode(SENSOR_ON, OUTPUT_OPEN_DRAIN);
  digitalWrite(SENSOR_ON, LOW);
  delay(2); // 4
  uint32_t controlWord = getBackupRegister(CONTROL_WORD);
  if (controlWord == 0x40) {
    samplingCounter = getBackupRegister(SAMPLING_REG);
    meterCounter = getBackupRegister(METER_REG);
    st1 = getBackupRegister(STA_REG);
  } else {
    setBackupRegister(CONTROL_WORD, 0x40);
    setBackupRegister(SAMPLING_REG, 0);
    setBackupRegister(METER_REG, 0);
    setBackupRegister(STA_REG, 0);
    meterCounter = 0;
    st1 = 100100;
    samplingCounter = 0;
  }
  int st_atas = 10;
  int st_bawah = 10;
  int analogIn = analogRead(SENSOR);
  String sts99 = String(st1);
  sts1 = (sts99.substring(0, 3)).toInt(); sts2 = (sts99.substring(3, 7)).toInt();
  if (analogIn > THRESHOLD) { // Saat Nilai Sensor HIGH
    sts2 = 100;
    if (sts1 < 995) {
      sts1++;
    } else {
      sts1 = 995;
    }
    if (sts1 >= 100 + st_atas) {
      if (sts2 < 100 + st_bawah) {
        sts2 = 100;
      }
    }
  } else {  // Saat Nilai Sensor LOW
    if (sts2 < 995) {
      sts2++;
    } else {
      sts2 = 995;
    }
    if (sts2 >= 100 + st_bawah) {
      if (sts1 < 100 + st_atas) {
        sts1 = 100;
      }
    }
    if ((sts2 >= (100 + st_bawah)) && (sts1 >= (100 + st_atas)) ) {
      meterCounter++;
      setBackupRegister(METER_REG, meterCounter);
      sts1 = 100;
      sts2 = 100;
    }
  }

  uint32_t insertreg = (String(sts1) + String(sts2)).toInt();
  setBackupRegister(STA_REG, insertreg);
  digitalWrite(SENSOR_ON, HIGH);
   Serial.print(sts1);
   Serial.print(",");
   Serial.print(sts2);
   Serial.print(",");
   Serial.print(meterCounter);
   Serial.print(",");
   Serial.println(analogIn);
  samplingCounter = getBackupRegister(SAMPLING_REG);
  samplingCounter++;
  if (samplingCounter > UPDATE_RATE) {  // send data
    samplingCounter = 0;
    pinMode(LED, OUTPUT);
    pinMode(LORA_ON, OUTPUT);
    digitalWrite(LED, HIGH);
    digitalWrite(LORA_ON, LOW);
    sendDataSensor();
    digitalWrite(LORA_ON, HIGH);
  }
  setBackupRegister(SAMPLING_REG, samplingCounter);
  //  delay(100);
  LowPower.shutdown(SAMPLING_RATE);
}
