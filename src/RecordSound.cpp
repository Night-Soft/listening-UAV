
#include "RecordSound.h"

#include "sound/SoundNoises.h"
#include "sound/utils.h"
#include "sound/utlis-claude.h"
//#include "sound/ClearSpectr.h"

//#include <arduinoFFT.h>

#define READ_BUFFER_SIZE 2048  

// #define PAUSE 0
// #define START 1

enum {
  PAUSE = 0,
  START,
  PAUSE_BLOCKED,
  START_BLOCKED
};

int32_t readBuffer[READ_BUFFER_SIZE * 2];
//int16_t readBuffer[READ_BUFFER_SIZE * 2];
int16_t buffer8[READ_BUFFER_SIZE];
int16_t bufferSpectr[READ_BUFFER_SIZE];
struct AudioTaskConfig;  // forward declaration

TaskHandle_t mainTaskHandle = NULL;

struct Task {
  TaskHandle_t handle = NULL;
  AudioTaskConfig& cfg;

  Task(AudioTaskConfig& cfg) : cfg(cfg) {}

  void resume();  // только объявление
  void pause();   // только объявление
};

struct Buffer {
  bool is = false;
  bool isFull = false;
  int16_t* data = nullptr;
  size_t size = 0;
  int index = 0;
};

struct RecordingControl {
  bool isRunning = false;
  bool isAutoSend = false;
  bool pauseAfterCb = false;
  bool isOwnBuffer = false;
};

struct AudioTaskConfig {
  Task task;
  Buffer buffer;
  RecordingControl control;
  typedef void (*TypeReadyCallback)(int16_t*);
  TypeReadyCallback readyCallback = nullptr;
  bool isReadyCallbackEnabled = false;

  typedef void (*TypeReadCallback)(int16_t*, int*);
  TypeReadCallback readCallback = nullptr;
  bool isReadCallbackEnabled = false;

  AudioTaskConfig() : task(*this) {}
};

// ------------------ Реализация методов Task ------------------

void Task::resume() {
  // if (handle == NULL) {
  //   Serial.println("Задача не запущена или удалена");
  //   return;
  // }
  // if (eTaskGetState(handle) == eSuspended) {
  //   Serial.println("AudioTask resume");
  //   vTaskResume(handle);
  // }

  
  xTaskNotify(handle, START, eSetValueWithOverwrite);
  cfg.control.isRunning = true;

}

void Task::pause() {
  xTaskNotify(handle, PAUSE, eSetValueWithOverwrite);
  cfg.control.isRunning = false;
  // Serial.println("\nAudioTask pause");
  // Serial.flush();

  // if (handle != NULL) {
  //   vTaskSuspend(handle);
  //   Serial.println("AudioTask pause");

  // }
}

// ------------------ Глобальная переменная ------------------

AudioTaskConfig taskConfig;
RecordingControl recordingControl;

float lowIIR = 0.25, highIRR = 0.65, volume = 0;
//float lowIIR = 0, highIRR = 0, volume = 0;
bool isAverge = false, isFir = true, isSpectr = false;

void changeFilter(String& cmd, String& v) {
  Serial.print("\nSet");
  float value = v.toFloat();

  if (cmd.startsWith("AVE")) {
    isAverge = value;
    Serial.print(" movingAvrge: ");
  }

  if (cmd.startsWith("FIR")) {
    isFir = value;
    Serial.print(" FIR: ");
  }

  if (cmd.startsWith("VOL")) {
    volume = value;
    Serial.print(" volume: ");
  }

  if (cmd.startsWith("LO")) {
    lowIIR = value;
    Serial.print(" lowIIR: ");
  }

  if (cmd.startsWith("HI")) {
    highIRR = value;
    Serial.print(" highIIR: ");
  }

  Serial.println(value);
}

int totalSamples(int& numSamples16) {
  from24To16bit(readBuffer, buffer8, numSamples16);
  int numSamples8 =
      decimation2K1(buffer8, buffer8, numSamples16);  // rate 16 > 8



  if (isFir) {
    firFilter(buffer8, numSamples8);
  }

  if (isSpectr) {
    firFilter(buffer8, numSamples8);
  }

  // decrFilter(buffer8, numSamples8);
  if (lowIIR > 0) {
    lowPassIIR(buffer8, numSamples8, lowIIR);  // reference record in 0.1
  }
  if (highIRR > 0) {
    highPassIIR(buffer8, numSamples8, highIRR);  // reference record in 0.95
  }
  if (volume > 0) {
    increaseVolume(buffer8, numSamples8, volume);  // reference record in 0.7
  }

  if (isAverge) {
    movingAverage(buffer8, numSamples8);
  }

  return numSamples8;
}

void sendSamples(int16_t* buffer, int& numSamples8) {
  for (int i = 0; i < numSamples8; i++) {
    // Serial.write((uint8_t)(buffer8[i] & 0xFF));  // младший байт
    // Serial.write((uint8_t)(buffer8[i] >> 8));
    Serial.write((uint8_t*)&buffer[i], 2);
  }
}

void ownBuffer(int& numSamples8) {
  int16_t* buffer = taskConfig.buffer.data;
  int& bufIndex = taskConfig.buffer.index;
  bool& isFull = taskConfig.buffer.isFull;

  if (isFull) isFull = false;

  for (int i = 0; i < numSamples8; i++) {
    buffer[bufIndex++] = buffer8[i];  // пишем в буфер
    if (bufIndex >= taskConfig.buffer.size) {
      bufIndex = 0;  // переполняем по кругу
      isFull = true;

      if (taskConfig.isReadyCallbackEnabled &&
          taskConfig.readyCallback != nullptr) {

        taskConfig.readyCallback(buffer);

        if (taskConfig.control.pauseAfterCb) {
          taskConfig.task.pause();
        }

      }
    }
  }
}

SemaphoreHandle_t xSemaphore = NULL;

bool  isTaskRun = false;
void recordTask(void* parameter) {
  uint32_t cmd = 30;
  for (;;) {
    if (xTaskNotifyWait(0, 0, &cmd, portMAX_DELAY) == pdTRUE) {
      xSemaphoreTake(xSemaphore, portMAX_DELAY);

      // Serial.print("cmd.isRunning ");
      // Serial.println(cmd);
      const bool &isAutoSend = taskConfig.control.isAutoSend;
      const bool &isOwnBuffer = taskConfig.control.isOwnBuffer;
      const bool &is = taskConfig.buffer.is;

      const bool isReadCallback = (taskConfig.isReadCallbackEnabled) &&
                                  (taskConfig.readCallback != nullptr);
      isTaskRun = true;

      if (cmd == START) {
        memset(HISTORY, 0, sizeof(HISTORY));
        int numSamples8 = 0;
        while (true) {
          // static unsigned long startTime = 0;
          // static unsigned long secondTime = millis();
          // startTime = micros();
          
          size_t bytes_read = 0;
          esp_err_t result = i2s_read(I2S_NUM_0, readBuffer, READ_BUFFER_SIZE,
                                      &bytes_read, pdMS_TO_TICKS(10));
          if (result == ESP_OK && bytes_read > 0) {
            int numSamples16 = bytes_read / 4;
            //numSamples8 = numSamples16 / 2;// / 2;

            int numSamples8 = totalSamples(numSamples16);

            if (isAutoSend == true) {  //
              // if (isSetFirst == false) {
              //   firstLast[0] = buffer8[0];
              //   isSetFirst = true;
              // }
              sendSamples(buffer8, numSamples8);
            } else {
              // for (int i = 0; i < numSamples16; i++) {
              //   Serial.println(readBuffer[i]);
              // }
            }

            if (is == true) { //ownbuffer
              ownBuffer(numSamples8);
            }

            if (isReadCallback == true) {
              taskConfig.readCallback(buffer8, &numSamples8);
            }
            // if(millis() - secondTime>= 1000) {
            //   secondTime = millis();
            //   static int res = micros() - startTime;
            //   Serial.print("\n\nTime: ");
            //   Serial.println(res);
            //   vTaskDelay(3000);
            // }
          }

          if (xTaskNotifyWait(0, 0, &cmd, 0) == pdTRUE && cmd == PAUSE) {
            Serial.flush();
            Serial.println("\n\n\n i2s_read paused");
            Serial.flush();
            isTaskRun = false;
            xTaskNotify(mainTaskHandle, PAUSE_BLOCKED, eSetValueWithOverwrite);
            xSemaphoreGive(xSemaphore);
            break;
          }
          xSemaphoreGive(xSemaphore);
          vTaskDelay(pdMS_TO_TICKS(9));
        }
      }
    }
    xSemaphoreGive(xSemaphore);
  }
}

RecordSound::RecordSound(gpioAdc1Channel micPin, uint16_t rate)
    : isAutoSend(taskConfig.control.isAutoSend),
      pauseAfterCb(taskConfig.control.pauseAfterCb),
      isRunning(taskConfig.control.isRunning) {
  this->micPin = (adc1_channel_t)micPin;
  this->rate = rate;

  taskConfig.buffer = Buffer();
  mainTaskHandle = xTaskGetCurrentTaskHandle();

  //initHighPassFilter(100.0);  // Срез на 100 Гц
  //initNoiseGate(500.0);
}

RecordSound::~RecordSound() {
  // delete[] buffer8;
  // buffer = nullptr;
}

void RecordSound::start(int milliseconds) {
  if (taskConfig.buffer.data == nullptr &&
      this->milliseconds != milliseconds) {
    this->bufferSize = (uint32_t)(milliseconds / 1000 * this->rate);
    this->milliseconds = milliseconds;

    taskConfig.buffer.data = new int16_t[this->bufferSize]();
    taskConfig.buffer.size = this->bufferSize;

    this->buffer = taskConfig.buffer.data;
  }

  taskConfig.buffer.is = true;
  taskConfig.task.resume();
}

void RecordSound::setBuffer(int16_t* buffer, size_t numSamples) {
    Buffer buf;
    buf.is = true;
    buf.data = buffer;
    buf.size = numSamples;
    taskConfig.buffer = buf;
}

void RecordSound::start(bool disableBuffer) {
  if (disableBuffer) {
    if (taskConfig.buffer.is) {
      // disabling ownBuffer
      Buffer buffer;
      taskConfig.buffer = buffer;
    }
  }

  taskConfig.buffer.index = 0;
  taskConfig.buffer.isFull = false;

  taskConfig.task.resume();
}

// pause in callback always must be on end of function
void RecordSound::pause() { 
  taskConfig.task.pause(); 
}

void RecordSound::onReady(void (*callback)(int16_t*)) {
  taskConfig.readyCallback = callback;
  taskConfig.isReadyCallbackEnabled = true;
}

void RecordSound::toggleOnReady(bool isEnable) {
  taskConfig.isReadyCallbackEnabled = isEnable;
}

void RecordSound::toggleOnRead(bool isEnable) {
  taskConfig.isReadCallbackEnabled = isEnable;
}

void RecordSound::onRead(void (*callback)(int16_t*, int*)) {
  taskConfig.readCallback = callback;
  taskConfig.isReadCallbackEnabled = true;
}

void RecordSound::init() {
  Serial.println("RecordSound init start");

  Serial.println("RecordSound init complete");
}

void RecordSound::send() {
  Serial.println("Start send audio samples");

  int size = (uint32_t)this->bufferSize;
  sendSamples(this->buffer, size);

  Serial.println("Audio samples sended");
}

void RecordSound::enableI2S() {
  // setupADC(this->micPin);
  // setupI2S(this->micPin);
  setupI2S();
  xSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(xSemaphore);
  // Создаём задачу для аудио
  xTaskCreatePinnedToCore(
      recordTask,               // функция задачи
      "Audio Task",             // имя задачи
      4096,                     // размер стека
      NULL,                     // параметр
      1,                        // приоритет
      &taskConfig.task.handle,  // handle для управления задачей
      1                         // запускать на core 1 (обычно свободнее)
  );

  this->isI2SEnabled = true;
}

void RecordSound::disableI2S() {
  i2s_stop(I2S_NUM_0);
  i2s_driver_uninstall(I2S_NUM_0);

  vTaskDelete(taskConfig.task.handle);
  taskConfig.task.handle = NULL;  // лучше сбросить
  this->isI2SEnabled = false;
}