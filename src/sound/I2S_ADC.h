#ifndef I2S_ADC_H
#define I2S_ADC_H

#include "Arduino.h"
#include "driver/adc.h"
#include "driver/i2s.h"

void clearI2SBuffers();

void setupADC(adc1_channel_t adcPin);

void setAdcPin(esp_err_t & ret, adc1_channel_t pin);

esp_err_t setupI2S(adc1_channel_t adcPin, uint32_t rate = 16000);
void setupI2S();
#endif