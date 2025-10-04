
#include "RecordSound.h"

#include "sound/SoundNoises.h"
#include "sound/utils.h"

//#include <arduinoFFT.h>

#define READ_BUFFER_SIZE 2048  // 256 семплов

#define STOP 0
#define START 1

int16_t readBuffer[READ_BUFFER_SIZE * 2];
int16_t buffer8[READ_BUFFER_SIZE];
struct AudioTaskConfig;  // forward declaration

// ArduinoFFT<float> FFT = ArduinoFFT<float>();

// float vReal[READ_BUFFER_SIZE];
// float vImag[READ_BUFFER_SIZE];

// void disD() {
//   // Прямое FFT
//   FFT.windowing(vReal, READ_BUFFER_SIZE, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
//   FFT.compute(vReal, vImag, READ_BUFFER_SIZE, FFT_FORWARD);

//   // Обнуление низких частот (например, <80 Гц)
//   int cutoffIndex = 80 * READ_BUFFER_SIZE / 8000; // индекс частоты 80 Гц
//   for (int i = 0; i < cutoffIndex; i++) {
//     vReal[i] = 0;
//     vImag[i] = 0;
//   }

//   // Обратное FFT
//   FFT.compute(vReal, vImag, READ_BUFFER_SIZE, FFT_BACKWARD);
//   FFT.complexToMagnitude(vReal, vImag, READ_BUFFER_SIZE);

//   // vReal теперь содержит очищенный сигнал
//   for (int i = 0; i < READ_BUFFER_SIZE; i++) {
//     Serial.println(vReal[i]);
//   }

//   delay(10);
// }

//const int BUFFER_SIZE = 512;
// const int CHUNK_SIZE = 64;

// byte bigBuffer[BUFFER_SIZE];

// // Пример: заполним буфер тестовыми данными
// void fillBuffer() {
//   for (int i = 0; i < BUFFER_SIZE; i++) {
//     bigBuffer[i] = i % 256;
//   }
// }

// void sendBufferInChunks() {
//   for (int i = 0; i < BUFFER_SIZE; i += CHUNK_SIZE) {
//     int bytesToSend = min(CHUNK_SIZE, BUFFER_SIZE - i);
//     Serial.write(bigBuffer + i, bytesToSend);

//     // ⚠️ Добавим небольшую паузу, если нужно (зависит от скорости порта и приёмника)
//     // delayMicroseconds(100); // настраивается по ситуации
//   }
// }

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

  typedef void (*TypeReadCallback)(int16_t*, int*);
  TypeReadCallback readCallback = nullptr;
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
  xTaskNotify(handle, STOP, eSetValueWithOverwrite);
  cfg.control.isRunning = false;
  Serial.println("\nAudioTask pause");
  Serial.flush();

  // if (handle != NULL) {
  //   vTaskSuspend(handle);
  //   Serial.println("AudioTask pause");

  // }
}

// ------------------ Глобальная переменная ------------------

AudioTaskConfig taskConfig;
RecordingControl recordingControl;

void totalSamples(int& numSamples8, int& numSamples16) {
  movingAverage(readBuffer, numSamples16);
  decimation(buffer8, readBuffer, numSamples16);
  // shiftDown(buffer8, numSamples8);
  removeDCOffset(buffer8, numSamples8);
  highPassIIR(buffer8, numSamples8, 0.95); // reference record in 0.95
  lowPassIIR(buffer8, numSamples8, 0.1); // reference record in 0.1
  increaseVolume(buffer8, numSamples8, 0.7); // reference record in 0.7

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

      if (taskConfig.readyCallback != nullptr) {
        taskConfig.readyCallback(buffer);
      }

      if (taskConfig.control.pauseAfterCb) {
        taskConfig.task.pause();
      }

    }
  }
}

void recordTask(void* parameter) {
  uint32_t cmd = 30;
  for (;;) {
    if (xTaskNotifyWait(0, 0, &cmd, portMAX_DELAY) == pdTRUE) {
     // Serial.print("cmd.isRunning ");
      //Serial.println(cmd);
      const bool &isAutoSend = taskConfig.control.isAutoSend;
      const bool &isOwnBuffer = taskConfig.control.isOwnBuffer;
      const bool &is = taskConfig.buffer.is;

      const bool isReadCallback = (taskConfig.readCallback != nullptr);

      if (cmd == START) {
        while (true) {
          static unsigned long startTime = 0;
          static unsigned long secondTime = millis();
          startTime = micros();
          
          size_t bytes_read = 0;
          esp_err_t result = i2s_read(I2S_NUM_0, readBuffer, READ_BUFFER_SIZE,
                                      &bytes_read, pdMS_TO_TICKS(10));

          if (result == ESP_OK && bytes_read > 0) {
            int numSamples16 = bytes_read / 2;
            int numSamples8 = bytes_read / 4;
            totalSamples(numSamples8, numSamples16);

            if (isAutoSend == true) {  //
              sendSamples(buffer8, numSamples8);
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

          if (xTaskNotifyWait(0, 0, &cmd, 0) == pdTRUE && cmd == STOP) {
            break;
          }
         //vTaskDelay(1 / portTICK_PERIOD_MS);
        }
      }
    }
  }
}

RecordSound::RecordSound(gpioAdc1Channel micPin, uint16_t rate)
    : isAutoSend(taskConfig.control.isAutoSend),
      pauseAfterCb(taskConfig.control.pauseAfterCb),
      isRunning(taskConfig.control.isRunning) {
  this->micPin = (adc1_channel_t)micPin;
  this->rate = rate;

  taskConfig.buffer = Buffer();
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

    // Serial.print("Size of buffer");
    // Serial.println((long)sizeof(buffer));
    // delay(3000);
    //taskConfig.task.resume();
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
}

void RecordSound::onRead(void (*callback)(int16_t*, int*)) {
  taskConfig.readCallback = callback;
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
  setupADC(this->micPin);
  setupI2S(this->micPin);

  // Создаём задачу для аудио
  xTaskCreatePinnedToCore(
      recordTask,                    // функция задачи
      "Audio Task",                  // имя задачи
      4096,                          // размер стека
      NULL,                          // параметр
      1,                             // приоритет
      &taskConfig.task.handle,  // handle для управления задачей
      1                              // запускать на core 1 (обычно свободнее)
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