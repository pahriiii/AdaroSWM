#include <RadioLib.h>
#include "STM32LowPower.h"
#include "backup.h"
#include <ArduinoUniqueID.h>

//STM32L051C8T6RFM97CWVer2
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
#define STA_REG1       3
#define STA_REG2       4
#define SAMPLING_RATE  100  // 100 ms
#define UPDATE_RATE    36000 // 36000*100 = 3600000 ms = 1 jam
#define THRESHOLD      100  //100 skala 1000

#define DEBUG

SX1276 lora = new Module(NSS, DIO0, LORA_RST, DIO1);
uint32_t meterCounter;
uint32_t samplingCounter;
uint8_t sts1;
uint8_t sts2;
int transmissionState =  ERR_NONE;
bool transmitFlag = false;
volatile bool enableInterrupt = true;
volatile bool operationDone = false;
volatile bool receivedFlag = false;
const uint8_t st_atas = 5;
const uint8_t st_bawah = 5;

byte calculateCRC(byte ar[], byte s) {
  byte rtn = 0;;
  for (byte i = 0; i < s; i++) {
    rtn ^= ar[i];
  }
  return rtn;
}

void setFlag(void) {
  operationDone = true;
}

typedef struct {
  uint8_t       typeId; //
  uint8_t       nodeId[12]; //store this nodeId
  uint8_t       battery; //type parameter
  uint32_t      cacah;   //temperature maybe?
} Payload;
Payload theData;

void blinkLed(int n) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED, LOW);
    delay(10);
    digitalWrite(LED, HIGH);
    delay(10);
  }
}

void sendDataSensor() {
  
  int state = lora.begin(920.0, 125.0, 9, 7, SX127X_SYNC_WORD, 17, 8, 0);
  lora.setDio0Action(setFlag);
  uint32_t count = getBackupRegister(METER_REG);
  //  Serial.println(",value = "+String(valuee));
  theData.typeId = 8;
  for (int i=0;i<12;i++) {
    theData.nodeId[i] = UniqueID[i];
  }
  theData.battery = 1;
  theData.cacah = count;
  byte len = sizeof(theData);
  byte byteArray[len + 1];
  memcpy(byteArray, (const void*)(&theData), len);
  byteArray[len] = calculateCRC(byteArray, len);
  transmissionState = lora.startTransmit(byteArray, len + 1);
  transmitFlag = true;
}

void setup() {
  //  Serial.begin(115200);
  enableBackupDomain();
  LowPower.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  // Konfigurasi pin dan inisialisasi
  pinMode(SENSOR, INPUT_ANALOG);        // Set pin SENSOR sebagai input analog
  pinMode(SENSOR_ON, OUTPUT_OPEN_DRAIN); // Set pin SENSOR_ON sebagai output open-drain
  digitalWrite(SENSOR_ON, LOW);         // nyalakan SENSOR_ON
  delay(2); // nunggu sensor aktif
  uint32_t controlWord = getBackupRegister(CONTROL_WORD);
  if (controlWord == 0x01) {
    samplingCounter = getBackupRegister(SAMPLING_REG);
    meterCounter = getBackupRegister(METER_REG);
    sts1 = getBackupRegister(STA_REG1);
    sts2 = getBackupRegister(STA_REG2);
  } else {
    setBackupRegister(CONTROL_WORD, 0x01);
    setBackupRegister(SAMPLING_REG, 0);
    setBackupRegister(METER_REG, 0);
    setBackupRegister(STA_REG1, 0);  // 32 bit = 4 x 8b
    setBackupRegister(STA_REG2, 0);
    meterCounter = 0;
    sts1 = 0;
    sts2 = 0;
    samplingCounter = 0;
  }
  // Setting Batas Atas dan batas bawah
  // Memproses nilai sensor analog
  int analogIn = analogRead(SENSOR);
  digitalWrite(SENSOR_ON, HIGH); // matikan sensor
  if (analogIn > THRESHOLD) { // Saat Nilai Sensor HIGH
    sts2 = 0;
    if (sts1 >= 250) {
      sts1 = 250;
    } else {
      sts1++;
    }
    if ((sts1 >= st_atas) && (sts2 < st_bawah)) {
      sts2 = 0;
    }
  } else {  // Saat Nilai Sensor LOW
    if (sts1 >= st_atas) {
      if (sts2 > 250) {
        sts2 = 250;
      } else {
        sts2++;
      }
    }
    if ((sts2 >= st_bawah) && (sts1 < st_atas)) {
      sts1 = 0;
    } else if ((sts1 >= st_atas) && (sts2 >= st_bawah)) {
      meterCounter++; // incre cacahan
      setBackupRegister(METER_REG, meterCounter);
      sts1 = 0;
      sts2 = 0;
    }
  }
  //  Serial.print("input:");
  //  Serial.print(analogIn);
  //  Serial.print(",");
  //  Serial.print("sts1:");
  //  Serial.print(sts1);
  //  Serial.print(",");
  //  Serial.print("sts2:");
  //  Serial.print(sts2);
  //  Serial.print(",");
  //  Serial.print("stlen:");
  //  Serial.print(stlen);
  //  Serial.print(",");
  //  Serial.print("samp:");
  //  Serial.print(samplingCounter);
  //  Serial.print(",");
  //  Serial.print("THRESHOLD:");
  //  Serial.print(THRESHOLD);
  //  Serial.print(",");
  //  Serial.print("cacahan:");
  //  Serial.println(meterCounter);

  setBackupRegister(STA_REG1, sts1);
  setBackupRegister(STA_REG2, sts2);

  samplingCounter = getBackupRegister(SAMPLING_REG);
  samplingCounter++;
  
  uint16_t urate = 0;
  pinMode(inp, INPUT_PULLUP);
  pinMode(lowPin, OUTPUT);
  digitalWrite(lowPin, LOW);
  if (!digitalRead(inp)) {
    urate = 10; // 10 detik(jika samplingrate 100);
  } else {
    urate = UPDATE_RATE;
  }
  if (samplingCounter >= urate) {  // send data
    samplingCounter = 0;
    pinMode(LED, OUTPUT);
    pinMode(LORA_ON, OUTPUT);
    digitalWrite(LED, HIGH);
    digitalWrite(LORA_ON, LOW);
    sendDataSensor();
    while (!operationDone);
  }
  setBackupRegister(SAMPLING_REG, samplingCounter);

  if (operationDone) {
    operationDone = false;
    if (transmitFlag) {
      if (transmissionState ==  ERR_NONE) {
        // Transmisi Berhasil
        blinkLed(1);
      } else {
        // Transmisi gagal
      }
      digitalWrite(LORA_ON, HIGH);
      transmitFlag = false; //Kirim
    } else {
      transmitFlag = false; //Nerima
    }
  }

  if (!operationDone && !transmitFlag) {
    LowPower.shutdown(SAMPLING_RATE);
  }
  //delay(100);
}
