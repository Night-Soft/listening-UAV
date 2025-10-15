#ifndef RECORD_SOUND_H
#define RECORD_SOUND_H

#include <Arduino.h>
#include <math.h>

#include "sound/I2S_ADC.h"

typedef enum {
  GPIO36_ADC1 = ADC1_CHANNEL_0, /*!< GPIO36 is ADC1 channel 0 */
  GPIO37_ADC1 = ADC1_CHANNEL_1, /*!< GPIO37 is ADC1 channel 1 */
  GPIO38_ADC1 = ADC1_CHANNEL_2, /*!< GPIO38 is ADC1 channel 2 */
  GPIO39_ADC1 = ADC1_CHANNEL_3, /*!< GPIO39 is ADC1 channel 3 */
  GPIO32_ADC1 = ADC1_CHANNEL_4, /*!< GPIO32 is ADC1 channel 4 */
  GPIO33_ADC1 = ADC1_CHANNEL_5, /*!< GPIO33 is ADC1 channel 5 */
  GPIO34_ADC1 = ADC1_CHANNEL_6, /*!< GPIO34 is ADC1 channel 6 */
  GPIO35_ADC1 = ADC1_CHANNEL_7, /*!< GPIO35 is ADC1 channel 7 */
} gpioAdc1Channel;

class RecordSound {
 public:
  RecordSound(gpioAdc1Channel micPin, uint16_t rate);
  ~RecordSound();

  static RecordSound* instance;

  bool& isRunning;
  bool& isAutoSend;
  bool& pauseAfterCb;

  bool isFullBuffer = false;

  adc1_channel_t micPin;
  uint16_t rate;
  int milliseconds = 0;
  // static RecordSound* instance;

  int16_t* buffer = nullptr;
  u_int32_t bufferSize = 0;

  void start(int milliseconds);
  void setBuffer(int16_t* buffer, size_t numSamples);
  void start(bool disableBuffer = false);
  void pause();
  void pauseBlocked();

  void send();
  void init();
  void enableI2S();
  void disableI2S();

  // callback if buffer in millis is full
  void onReady(void (*callback)(int16_t*));

  // callback if I2S data conver and can read 
  void onRead(void (*callback)(int16_t*, int*));

 private:
  bool isI2SEnabled = false;
};

#endif