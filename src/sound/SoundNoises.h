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

// void lowPassIIR(int16_t* samples, int& num_samples, boolean needShift = false) {
//   static const float alpha = 0.1;  // коэффициент сглаживания (0..1)
//   static float filtered = 0.0f;
//   if (needShift) {
//     for (int i = 0; i < num_samples; i++) {
//       float raw = (float)samples[i];
//       filtered = alpha * raw + (1.0f - alpha) * filtered;
//       samples[i] = ((int16_t)(filtered - 2048)) << 4;
//     }
//   } else {
//     for (int i = 0; i < num_samples; i++) {
//       float raw = (float)samples[i];
//       filtered = alpha * raw + (1.0f - alpha) * filtered;
//       samples[i] = (int16_t)filtered;
//     }
//   }
// }

#endif