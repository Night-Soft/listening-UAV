
#include "RecordSound.h"
// #include "sound/CompareUtils.h"
#include "sleep.h"
#include "sound/compareFFT.h"
#include "sound/ClearSpectr.h"
//#include "reference/noiseSilence.h"
//#include "utils.h

#define AUDIO_RATE 8000
#define MANUAL_BTN_PIN 23
//#define isRecordToPC true

//#define IS_DEV true

int16_t currentAudio[CURRENT_AUDIO_SIZE] = {0};

RecordSound record(GPIO35_ADC1, AUDIO_RATE);

struct Command {
  String command;
  String value;
};

volatile bool isInterrupt = false;
uint16_t clicksCounter = 0;
volatile int buttonTimer = 0;

IRAM_ATTR void btnItr(){
  isInterrupt = true;
  clicksCounter++;
  buttonTimer = millis();
}

void ifSerialAvailable() {
  if (Serial.available()) {
    String str = Serial.readStringUntil('\n');
    str.trim();
    str.substring(2);

    uint8_t lastIndex = str.lastIndexOf(" ");
    String command = str.substring(0, lastIndex);
    String cmdValue = str.substring(lastIndex + 1);

    Serial.print("Command: ");
    Serial.println(str);

    if (xSemaphore != NULL) {
      Serial.println("wait semafore");
      Serial.printf("Начальное значение ДВОИЧНОГО семафора: %u\n",
                    uxSemaphoreGetCount(xSemaphore));
      xSemaphoreTake(xSemaphore, portMAX_DELAY);
      Serial.println("semafore given");
    }

    Serial.flush();
    if (command == "getNoiseSpectr") {
     // delay(10000);
      static unsigned long startTime = 0;
      startTime = micros();
     // getNoiseSpectr(noiseSilence);

      static int res = micros() - startTime;
      Serial.print("\n\nTime for NoiseSpectr: ");
      Serial.println(res);
      delay(100);
      Serial.println("SendSpectr");
      Serial.flush();
      delay(1000);

      for (int i = 0; i < 512; i++) {
        Serial.write((uint8_t*)&noiseFloor[i], 8);
      }

      Serial.println("EndSpectr");

      Serial.print("\nFirst: ");
      Serial.print(noiseFloor[0]);
      Serial.print("\nLast: ");
      Serial.println(noiseFloor[511]);

    }

    if (command.startsWith("F ")) {
      uint8_t firstIndex = str.indexOf(" ");
      String command = str.substring(firstIndex+1, lastIndex);

      Serial.println("startsWith('F')");
      Serial.print("|command:");
      Serial.print(command);
      Serial.print("|value:");
      Serial.print(cmdValue);

      changeFilter(command, cmdValue);
    }

    if (command == "getSpectr") {
      float hzInOneSample = SAMPLE_RATE / FFT_SIZE;  // 7,8125
      //getCompresedSpectr();
    }

    if (command == "read") {
      bool value = cmdValue.toInt();
      Serial.print("\nonRead set to: ");
      Serial.println(value);
      record.toggleOnRead(value);
    }

    if (command == "ready") {
      bool value = cmdValue.toInt();
      Serial.print("\nonRedy set to: ");
      Serial.println(value);
      record.toggleOnReady(value);
    }

    if (command == "recordToPc") {
      record.toggleOnRead(false);
      record.toggleOnReady(false);

      if (record.isAutoSend == false) {
        record.isAutoSend = true;
        Serial.println("\n set isAutosend to true");
      }

      Serial.println("Record start");
      Serial.flush();

      if (record.isRunning == false) record.start();
    }

    if (command == "start") {
      if (record.isRunning == false) record.start();
    }

    if (command == "stop") {
      Serial.println("Record stop");
      Serial.flush();
      record.isAutoSend = false;
      record.toggleOnRead(true);
      record.toggleOnReady(true);

    }

    if (command == "senE") {
      record.isAutoSend = true;
      // record.pause();
      Serial.println("Record will send");
    }
    if (command == "senD") {
      record.isAutoSend = false;
      // record.pause();
      Serial.println("Record send stop");
    }

    xSemaphoreGive(xSemaphore);
  }
}

bool canCompare = false;
SemaphoreHandle_t semaphoreCanCompare = NULL;
void ifCanCompare() {
  xSemaphoreTake(semaphoreCanCompare, portMAX_DELAY);
  if (canCompare) {
    canCompare = false;
    xSemaphoreGive(semaphoreCanCompare);

    delay(50);
    Serial.println("Start compare");

    //sendWarrningIfNeeded(fullCompare());
    //fullCompare(true);

#ifdef IS_DEV
    fullCompareHz();
#else
    bool isUav = fullCompareHz();
    Serial.print("\nisUav: ");
    Serial.println(isUav);

    sendWarrningIfNeeded(isUav);
#endif

    Serial.println("End compare");

#ifdef IS_DEV
    Serial.println("New Record after 1s");
    delay(1000);
    record.start();
#else
    if(!isUav) goToSleep();// go to sleep to 10s
    Serial.println("Skip sleep.");
    Serial.println("New Record after 10s");
    delay(10000);
    record.start();
#endif

  }
    xSemaphoreGive(semaphoreCanCompare);
}

void dataReady(int16_t* samples) {
  Serial.println("Data ready");
  xSemaphoreTake(semaphoreCanCompare, portMAX_DELAY);
  canCompare = true;
  xSemaphoreGive(semaphoreCanCompare);
}

void setupRecordToBuffer() {
  record.onReady(dataReady);
  record.pauseAfterCb = true;
  current_audio = currentAudio;
  record.setBuffer(currentAudio, CURRENT_AUDIO_SIZE);
}

void setup() {
  Serial.begin(230400);
  pinMode(2,OUTPUT);
  #ifdef IS_DEV
  #else
  setupSleep();
  #endif

  semaphoreCanCompare = xSemaphoreCreateBinary();
  xSemaphoreGive(semaphoreCanCompare);

  Serial.println("Start recording after 500ms");
  delay(500);
  record.enableI2S();
  #ifdef isRecordToPC

  #else
  setupRecordToBuffer();  // comment this for record to pc

  #endif

  delay(100);
  if (bootCount == 1) {
    sendWarrningIfNeeded(false, true);
  }

  pinMode(MANUAL_BTN_PIN, INPUT_PULLUP);
  attachInterrupt(MANUAL_BTN_PIN, btnItr, HIGH);

  //delay(3000);
  record.start();


  //delay(2000);

 // getCompresedSpectr();
}
int timer = 0;
bool prevSend = false;
void loop() {
#ifdef IS_DEV
  ifSerialAvailable();


#endif

  ifCanCompare();
  delay(50);

  static int coreTime = 0;

  if (millis() - coreTime > 1000) {
    coreTime = millis();
    int core = xPortGetCoreID();
    Serial.printf("Loop Running on core %d\n", core);
  }
  // if (isInterrupt) {
  //   static int currentTime = 0;
  //   static int clicks = 0;

  //   currentTime = millis();
  //   if (currentTime - buttonTimer > 50) {
  //     buttonTimer = currentTime;
  //     isInterrupt = false;

  //     if (digitalRead(MANUAL_BTN_PIN)) {
  //       Serial.print("interrupts: ");
  //       Serial.print(clicksCounter);
  //       Serial.print(" cliks: ");
  //       Serial.println(++clicks);
  //     }
  //   }
  // }

  //  if(millis() - timer >= 1500) {
  //   timer = millis();
  //   sendWarrningIfNeeded(prevSend, true);
  //   prevSend = !prevSend;
  //  }
}