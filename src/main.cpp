
#include "RecordSound.h"
// #include "sound/CompareUtils.h"
#include "sleep.h"
#include "sound/compareFFT.h"
#include "sound/ClearSpectr.h"
//#include "utils.h

#define CURRENT_AUDIO_SIZE 16384  // 8192 // 1024*8 : 24576 = 3s, 12288 = 1.536s, 16384 = 2.048s
#define AUDIO_RATE 8000

//#define isRecordToPC true
#define IS_DEV true

int16_t currentAudio[CURRENT_AUDIO_SIZE] = {0};

RecordSound record(GPIO35_ADC1, AUDIO_RATE);

struct Command {
  String command;
  String value;
};

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
      delay(100);

      for (int i = 0; i < 128; i++) {
        Serial.write((uint8_t*)&noiseFloor[i], 8);
      }

      Serial.println("EndSpectr");

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
      getCompresedSpectr();
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
void ifCanCompare() {
  if (canCompare) {
    canCompare = false;
    delay(50);
    Serial.println("Start compare");

    //sendWarrningIfNeeded(fullCompare());
    //fullCompare(true);

#ifdef IS_DEV
    fullCompareHz();
#else
    sendWarrningIfNeeded(fullCompareHz());
#endif

    Serial.println("End compare");

#ifdef IS_DEV
    Serial.println("New Record after 1s");
    delay(1000);
    record.start();
#else
    goToSleep();// go to sleep to 10s
#endif

  }
}

void dataReady(int16_t* samples) {
  Serial.println("Data ready");

  canCompare = true;
}

void setupRecordToBuffer() {
  record.onReady(dataReady);
  record.pauseAfterCb = true;
  current_audio = currentAudio;
  record.setBuffer(currentAudio, CURRENT_AUDIO_SIZE);
}

void setup() {
  Serial.begin(230400);
  
  #ifdef IS_DEV
  #else
  setupSleep();
  #endif
  
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

  //delay(3000);
  record.start();

  //delay(2000);

 // getCompresedSpectr();
}

void loop() {
  #ifdef IS_DEV
   ifSerialAvailable();
  #endif
   ifCanCompare();
   delay(50);
}