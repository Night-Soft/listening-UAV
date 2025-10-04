
#include "RecordSound.h"
//#include "sound/CompareUtils.h"
#include "sound/compareFFT.h"

int16_t currentAudio[8000] = {0};
// RecordSound::instance = nullptr;
RecordSound record(GPIO35_ADC1, 8000);
void callback(int16_t* samples) {
  // int size = sizeof(samples)/2;
  // Serial.println();
  // Serial.print("size ");
  // Serial.println(size);
  // delay(2000);

  // for (int i = 0; i <10; i++) {
  //   Serial.println(samples[i]);
  // }
  // delay(2000);
  // Serial.println("TryDisable ");

  // record.pause();
  // record.disableI2S();
  Serial.println("Send in callback");

  record.send();

  Serial.println("Record stop");
  Serial.println("Record stopInCallback");

  // record.pause();
}

unsigned long timeBeetwenCompare = 0;
int lastCompare = 0;
bool canCompare = false;
void dataReady(int16_t* samples) {
  Serial.println("Data ready");

  lastCompare = millis();
  canCompare = true;
}

void setup() {
  Serial.begin(576000);
  int16_t t1[8] = {0};
  int16_t t2[8];
  Serial.println("Start recording after 3000ms");
  delay(3000);
  Serial.print("Size of t1");
  Serial.printf("%zu\n", sizeof(t1));
  Serial.print("Size of t2");
  Serial.printf("%zu\n", sizeof(t2));

  delay(1000);
  record.enableI2S();
  // delay(1000);
  // record.onReady(dataReady);
  record.isAutoSend = true;
  //record.pauseAfterCb = true;
  // record.pause();
  // delay(500);
 // current_audio = currentAudio;
 // record.setBuffer(currentAudio, 8000);
  //record.start();
  lastCompare = millis();
  // delay(1200);
  // record.start(5000);
  // Serial.println("Start recording");
  // record.start(5000);
}

unsigned long tim = 0;
void loop() {
  if (canCompare) {
    static unsigned long now = 0;
    canCompare = false;
    Serial.print("Time beetwen compare");
    now = millis();
    //tim = now;
    Serial.println(now - timeBeetwenCompare);

    delay(500);
    Serial.println("Start compare");
    compareFFT();
    delay(500);
    Serial.println("End compare");
    delay(3000);
    Serial.println("Start new record after 3s");

    now = millis();
    timeBeetwenCompare = now;

    record.start(false);
  }

  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "t" || command == "test") {
      Serial.println("Запуск теста чистой частоты...");
      Serial.println("Тест завершен. Продолжаем обычную работу.\n");
      return;  // Пропускаем остальную обработку в этом цикле
    }
    if (command == "t" || command == "start") {
      record.isAutoSend = false;
      delay(25);
      Serial.println("Record start");
      delay(25);
      Serial.flush();
      record.isAutoSend = true;
      if(record.isRunning == false) record.start();
      return;  // Пропускаем остальную обработку в этом циклеc
    }
    if (command == "t" || command == "stop") {
      record.pause();
      Serial.flush();
      Serial.println("Record stop");
      return;  // Пропускаем остальную обработку в этом цикле
    }
    if (command == "t" || command == "senE") {
      record.isAutoSend = true;
      // record.pause();
      Serial.println("Record will send");
      return;  // Пропускаем остальную обработку в этом цикле
    }
    if (command == "t" || command == "senD") {
      record.isAutoSend = false;
      // record.pause();
      Serial.println("Record send stop");
      return;  // Пропускаем остальную обработку в этом цикле
    }

     if (command == "start test") {;
      Serial.println("Test send");
      delay(1000);
      Serial.println("Test send");
      delay(1000);
      Serial.println("Test send");
      delay(1000);
      Serial.println("Test send");
      delay(1000);
      return;  // Пропускаем остальную обработку в этом цикле
    }
  }
  
  delay(100);
}