#ifndef SLEEP_H
#define SLEEP_H

#include <Arduino.h>

#define G433_SPEED 500
#include <Gyver433.h>
//Gyver433_RX<пин> tx;
Gyver433_TX<22> tx;

#include "SendWarring.h"

#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  10           // Time ESP32 will go to sleep (in seconds)

RTC_DATA_ATTR int bootCount = 0;

// Method to print the reason by which ESP32 has been awaken from sleep
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void setupSleep(){
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  // Print the wakeup reason for ESP32
  print_wakeup_reason();

  // First we configure the wake up source We set our ESP32 to wake up every 5 seconds
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
  " Seconds");
}
void send () {
  static char data[] = "Hello from #xx"; // строка для отправки
  static byte count = 0;                 // строка для отправки
 // static const char isUAWTxt[] = "isUAW"; // строка для отправки
 
  // data[12] = (count / 10) + '0';
  // data[13] = (count % 10) + '0';

   tx.sendData(data);
   // добавляем счётчик в строку

  // if (++count >= 100) count = 0;

   //Serial.print("Send: ");
   //Serial.println(data);
}

RTC_DATA_ATTR bool prevSendUAV = false;
void sendWarrningIfNeeded(const bool& isUAV, bool forceSend = false) {
  if (forceSend == false && prevSendUAV == false && isUAV == false) {
    return;
  }

  const char isUAWTxt[] = "isUAW";  // строка для отправки
  const char noUAWTxt[] = "noUAW";  // строка для отправки

  const char* whatSend;
  if (isUAV) {
    whatSend = isUAWTxt;
  } else {
    whatSend = noUAWTxt;
  }

  prevSendUAV = isUAV;

  for (uint8_t i = 0; i < 3; i++) {
    if (isUAV) {
      tx.sendData(isUAWTxt);
    } else {
      tx.sendData(noUAWTxt);
    }
    delay(250);
  }
  Serial.print("\nSended ");
  Serial.print(whatSend);
  Serial.println(" 3 times");

  if (!forceSend) {
    Serial.print("isPostTaskCreated: ");
    Serial.println(postTask.isTaskCreated());

    if (postTask.isTaskCreated() == false) postTask.create();

    Serial.println("Sleep take port 15000ms.");
    int core = xPortGetCoreID();
    Serial.printf("Running on core %d\n", core);
    postTask.takeSemaphore(pdMS_TO_TICKS(15000));

    if (postTask.isSemaphoreFree == false) {
      Serial.println("postTask Semaphore error, isTaked.");
    } else {
      Serial.println("xSPost is free, continue.");
    }
  }
}

void goToSleep() {
  
  for (uint8_t i = 0; i < 15; i++) {
    digitalWrite(2, HIGH);
    delay(50);
    digitalWrite(2, LOW);
    delay(50);
  }

  Serial.println("Going to sleep now");
  delay(50);
  Serial.flush();
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}

#endif