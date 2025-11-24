#ifndef SOUND_NOISES_H
#define SOUND_NOISES_H

#include <Arduino.h>

void lowPassIIR(uint16_t& value) {
  static const float alpha = 0.1;  // коэффициент сглаживания (0..1)
  static float filtered = 0;

  filtered = alpha * value + (1 - alpha) * filtered;
  value = (int16_t)filtered;
}

void lowPassIIR(int16_t* samples, int& numSamples, const float alpha = 0.95) {
 // static const float alpha = 0.1;  // коэффициент сглаживания (0..1)
  static float filtered = 0.0f;

  for (int i = 0; i < numSamples; i++) {
    float raw = (float)samples[i];

    filtered = alpha * raw + (1.0f - alpha) * filtered;
    samples[i] = (int16_t)filtered;
  }
}


// new lowPassIIR
// void lowPassIIR(int16_t* samples, int& numSamples, const float alpha = 0.95) {
//   static float filtered = 0.0f;
//   for (int i = 0; i < numSamples; i++) {
//     float raw = (float)samples[i];
//     filtered = alpha * filtered + (1.0f - alpha) * raw;
//     samples[i] = (int16_t)filtered;
//   }
// }

// new highPassIIR
void highPassIIR(int16_t* samples, int& numSamples, const float alpha = 0.95) {
  static float prevOut = 0.0f;
  static float prevIn = 0.0f;

  for (int i = 0; i < numSamples; i++) {
    float raw = (float)samples[i];
    prevOut = alpha * (prevOut + raw - prevIn);

    if (prevOut > 32767.0f) {
      prevOut = 32767.0f;
    } else if (prevOut < -32768.0f) {
      prevOut = -32768.0f;
    }

    prevIn = raw;
    samples[i] = (int16_t)prevOut;
  }
}

// void highPassIIR(int16_t* samples, int& numSamples, const float alpha = 0.95) {
//   // Коэффициент 'alpha' здесь имеет то же значение, что и для ФНЧ:
//   // чем ближе к 1, тем выше частота среза (меньше сглаживание ФНЧ)
//   // что эквивалентно более агрессивному ФВЧ.
//   static float filtered_lpf = 0.0f; // Сохраняем последнее значение ФНЧ
  
//   for (int i = 0; i < numSamples; i++) {
//     float raw = (float)samples[i];
    
//     // 1. Расчет выходного значения ФНЧ (как в вашем исходном коде)
//     //    y_LPF[n] = alpha * x[n] + (1.0f - alpha) * y_LPF[n-1]
//     filtered_lpf = alpha * raw + (1.0f - alpha) * filtered_lpf;
    
//     // 2. Расчет выходного значения ФВЧ: y_HPF[n] = x[n] - y_LPF[n]
//     float filtered_hpf = raw - filtered_lpf;
    
//     // 3. Сохранение результата ФВЧ
//     // Мы используем raw (исходный отсчет) для расчета, 
//     // но перезаписываем массив samples результатом ФВЧ.
//     samples[i] = (int16_t)filtered_hpf;
//   }
// }

#endif