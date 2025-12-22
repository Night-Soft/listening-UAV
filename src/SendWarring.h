#ifndef SEND_WARRNING_H
#define SEND_WARRNING_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "sound/CompareResults.h"
#include "env.h" // ssid, password, url

bool setupWifi() {
  static bool isConnected = false;
  if(isConnected) return isConnected;
  WiFi.begin(ssid, password);
  Serial.print("Connecting");

  isConnected = (WiFi.status() == WL_CONNECTED);
  int current = millis();
  while (isConnected == false) {
    delay(300);
    isConnected = (WiFi.status() == WL_CONNECTED);
    Serial.print(".");
    if (millis() - current > 5000) {
      isConnected = false;
      break;
    }
  }

  if (isConnected) {
    Serial.println("\nConnected!");
  } else {
    Serial.println("\nFailed to connected!");
  }

  return isConnected;
}

struct PostTask {
  TaskHandle_t handle = NULL;
  SemaphoreHandle_t semaphore = NULL;
  bool isSemaphoreCreated = false, isSemaphoreFree = true;
  void giveSemaphore();
  void takeSemaphore(TickType_t time = portMAX_DELAY);
  bool isTaskCreated();
  void create();
  void remove();
};

extern struct PostTask postTask;

void endPostTask() {
  if (compareResult->isUav == false) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
  postTask.remove();
}

static WiFiClientSecure *globalClient = nullptr;

void initSecureClient() {
  if (globalClient == nullptr) { 
    globalClient = new WiFiClientSecure();
    globalClient->setInsecure();
    globalClient->setHandshakeTimeout(10000);
  }
}

void postTaskFn(void* parameter) {      
  
  Serial.println("Start POOST Task.");
  bool isConnected = setupWifi();
  if (isConnected == false || compareResult->json == "") {
    endPostTask();
    return;
  }
  digitalWrite(2, HIGH);

  Serial.print("getMaxAllocHeap: ");
  Serial.println(ESP.getMaxAllocHeap());
  
  initSecureClient();
  
  HTTPClient http;

  http.begin(*globalClient, url);  // Указываем URL
  http.addHeader("Content-Type", "application/json");
  //http.addHeader("User-Agent", "ESP32");
  http.setTimeout(10000); // 1 секунд

  String &json = compareResult->json;
  Serial.println("Post json: ");
  Serial.println(json);

  Serial.println("\nTry Post;");

  int httpResponseCode = http.POST(json);

  Serial.print("Response: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println(response);
  }

  http.end();
  digitalWrite(2, LOW);
  endPostTask();
}

void PostTask::giveSemaphore() {
  if (this->semaphore != NULL) {
    Serial.println("PostTask Semaphore Given");
    xSemaphoreGive(this->semaphore);
    this->isSemaphoreFree = true;
  }
}

void PostTask::takeSemaphore(TickType_t time) {
  if (this->semaphore != NULL) {
    xSemaphoreTake(this->semaphore, time);
  }
}

bool PostTask::isTaskCreated() { return this->handle != NULL; }

void PostTask::create() {
  if (this->isTaskCreated()) {
    Serial.println("Post Task already created");
    return;
  }
  if(this->isSemaphoreCreated == false) {
    this->semaphore = xSemaphoreCreateBinary();
    this->isSemaphoreFree = false;
  } else {
    this->takeSemaphore();
    this->isSemaphoreFree = false;
  }
  xTaskCreatePinnedToCore(postTaskFn,     // функция задачи
                          "POST Task",    // имя задачи
                          10240,          // размер стека
                          NULL,           // параметр
                          2,              // приоритет
                          &this->handle,  // handle для управления задачей
                          0  // запускать на core 1 (обычно свободнее)
  );
  Serial.println("POST task was created.");
}

void PostTask::remove() {
  this->giveSemaphore();
  Serial.println("PostTask removed");
  this->handle = NULL;
  vTaskDelete(NULL);
}

struct PostTask postTask;

#endif
