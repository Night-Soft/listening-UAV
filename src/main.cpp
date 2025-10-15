
#include "RecordSound.h"
// #include "sound/CompareUtils.h"
#include "sleep.h"
#include "sound/compareFFT.h"
//#include "utils.h

#define CURRENT_AUDIO_SIZE 12288  // 8192 // 1024*8 : 24576 = 3s, 12288 = 1.536s
#define AUDIO_RATE 8000

#define IS_DEV false

int16_t currentAudio[CURRENT_AUDIO_SIZE] = {0};

RecordSound record(GPIO35_ADC1, AUDIO_RATE);

void ifSerialAvailable() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "test") {
      Serial.println("Запуск теста чистой частоты...");
      Serial.println("Тест завершен. Продолжаем обычную работу.\n");
      return;  // Пропускаем остальную обработку в этом цикле
    }
    if (command == "start") {
      record.isAutoSend = false;
      delay(100);

      Serial.println("\n\n\n try dis ena isAutosend");
      delay(100);
      Serial.println("Record start");
      Serial.flush();
      delay(100);
      record.isAutoSend = true;
      if (record.isRunning == false) record.start();
      return;  // Пропускаем остальную обработку в этом циклеc
    }

    if (command == "stop") {
      Serial.println("\n\n\ntry stop");

      static int startTime = 0;
      startTime = micros();
      record.pause();
      Serial.print("\n\n\nTime to stop: ");
      Serial.println(micros() - startTime);

      Serial.flush();
      Serial.println("Record stop");
      return;  // Пропускаем остальную обработку в этом цикле
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
  }
}

bool canCompare = false;
void ifCanCompare() {
  if (canCompare) {
    canCompare = false;
    delay(50);
    Serial.println("Start compare");

    sendWarrningIfNeeded(fullCompare());

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

void setup() {
  Serial.begin(576000);
  //setupSleep();

  Serial.println("Start recording after 500ms");
  delay(500);
  record.enableI2S();

  setupRecordToBuffer(); // comment for record to pc
  record.start();
}

void loop() {
   ifCanCompare();
   delay(50);
 //  ifSerialAvailable();
  //sendWarrningIfNeeded(!prevSendUAV);
  //delay(1000);
}