
#include "RecordSound.h"
// #include "sound/CompareUtils.h"
#include "sleep.h"
#include "sound/compareFFT.h"
//#include "utils.h

#define CURRENT_AUDIO_SIZE 12288  // 8192 // 1024*8 : 24576 = 3s, 12288 = 1.536s
#define AUDIO_RATE 8000

#define IS_DEV

int16_t currentAudio[CURRENT_AUDIO_SIZE] = {0};

RecordSound record(GPIO35_ADC1, AUDIO_RATE);

void ifSerialAvailable() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
command.substring(2);
    Serial.println("wait semafore");
Serial.printf("Начальное значение ДВОИЧНОГО семафора: %u\n", 
                  uxSemaphoreGetCount(xSemaphore));
    xSemaphoreTake(xSemaphore, portMAX_DELAY);

    Serial.println("semafore given");

    Serial.flush();

    if (command.startsWith("VOL")) {
      float vol = command.substring(command.length() - 1).toFloat();
      volume = vol;
      if (vol == 0) {
        Serial.println("IncreaseVolume disabled");
      } else {
        Serial.print("\n Volume set to: ");
        Serial.println(volume);
      }
    }

    if (command.startsWith("setLow")) {
      bool isSet = command.substring(command.length() - 1).toInt();
      isLowPassIIR = isSet;
      Serial.print("\n LowPassIIR set to: ");
      Serial.println(isSet);
    }

    if (command.startsWith("setHigh")) {
      bool isSet = command.substring(command.length() - 1).toInt();
      isHighIRR = isSet;
      Serial.print("\n HighIRR set to: ");
      Serial.println(isSet);
    }

    if (command.startsWith("LO")) {
      float scale = command.substring(2).toFloat();
      changeFilter(true, scale);
    }

    if (command.startsWith("HI")) {
      float scale = command.substring(2).toFloat();
      changeFilter(false, scale);
    }

    if (command == "start") {
      Serial.println("\n\n\n set isAutosend to true");
      Serial.println("Record start");
      Serial.flush();
      record.isAutoSend = true;
      if (record.isRunning == false) record.start();

      xSemaphoreGive(xSemaphore);
      return;
    }

    if (command == "stop") {
      Serial.println("\n\n\ntry stop");

      static int startTime = 0;
      startTime = micros();
     //record.pauseBlocked();
      record.pause();
      Serial.print("\n\n\nTime to stop: ");
      Serial.println(micros() - startTime);

      Serial.println("Record stop");
      Serial.flush();

      xSemaphoreGive(xSemaphore);
      return;  // Пропускаем остальную обработку в этом цикле
    }

    if (command == "getFirstLast") {
      Serial.print("\n\n\nFirstLast: ");
      Serial.print(firstLast[0]);
      Serial.print(", ");
      Serial.println(firstLast[1]);
      xSemaphoreGive(xSemaphore);
      return;
    }

    if (command == "senE") {
      record.isAutoSend = true;
      // record.pause();
      Serial.println("Record will send");
      return;  // Пропускаем остальную обработку в этом цикле
    }
    if (command == "senD") {
      record.isAutoSend = false;
      // record.pause();
      Serial.println("Record send stop");
      return;  // Пропускаем остальную обработку в этом цикле
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
    fullCompareHz();

    delay(50);
    Serial.println("End compare");
    delay(30);

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

void setupRecordToPc() { 
  //record.isAutoSend = true; 
}

void setupRecordToBuffer() {
  record.onReady(dataReady);
  record.pauseAfterCb = true;
  current_audio = currentAudio;
  record.setBuffer(currentAudio, CURRENT_AUDIO_SIZE);
}

//#define isRecordToPC true
void setup() {
  Serial.begin(230400);
  // setupSleep();

  Serial.println("Start recording after 500ms");
  delay(500);
  record.enableI2S();
  #ifdef isRecordToPC

  #else
  setupRecordToBuffer();  // comment this for record to pc

  #endif
  record.start();

  delay(100);
  if (bootCount == 1) {
    sendWarrningIfNeeded(false, true);
  }
}

void loop() {
  #ifdef isRecordToPC
   ifSerialAvailable();
   #else
   ifCanCompare();
   #endif
   delay(50);

   //sendWarrningIfNeeded(!prevSendUAV);
  //delay(1000);
}