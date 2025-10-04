#ifndef SOUND_UTILS_H
#define SOUND_UTILS_H
#include <Arduino.h>

void removeDCOffset(int16_t* buffer, int numSamples) {
    long sum = 0;
    for (int i = 0; i < numSamples; i++) {
        sum += buffer[i];
    }

    int16_t dc = (int16_t)(sum / numSamples);  // среднее значение (DC смещение)

    for (int i = 0; i < numSamples; i++) {
        buffer[i] -= dc;  // вычитаем смещение
    }
}

// Преобразуем из диапазона -32768...32767 в 0...4095
uint16_t convertTo12Bit(int16_t sample) {
    int32_t shifted = sample + 32768; // Сдвигаем в диапазон 0...65535
    return (shifted >> 4); // Делим на 16 для получения 12-битного значения (0-4095)
}

// setBuffer16To8 уменьшение частоты дискретизации
void decimation(int16_t* targetBuffer, const int16_t* sourceBuffer16, int numSamples16) {
  //removeDCOffset(buffer16, numSamples16);

    // --- Децимация (16 кГц → 8 кГц) ---
    int j = 0;
    for (int i = 0; i < numSamples16; i += 2) {
        targetBuffer[j] = (int16_t)((sourceBuffer16[i] + sourceBuffer16[i + 1]) / 2);
        j++;
    }
}

// --- Фильтр: скользящее среднее на 4 точки ---
void movingAverage(int16_t* buffer, int numSamples) {
  for (int i = 0; i < numSamples - 2; i++) {
    buffer[i] =
        (buffer[i] + buffer[i + 1]) / 2;
  }
//   // Края без фильтра (копируем как есть)
//   buffer8[0] = buffer[0];
//   buffer8[1] = buffer[1];
//   buffer8[size - 2] = buffer[size - 2];
//   buffer8[size - 1] = buffer[size - 1];
}

void shiftDown(int16_t* buffer, int numSamples) {
  for (int i = 0; i < numSamples; i++) {
    buffer[i] = ((int16_t)(buffer[i] - 2048)) << 4;
  }
}

void noramlize(int16_t* buffer, int numSamples) {
    int16_t maxVal = buffer[0];
    int16_t minVal = buffer[0];

    for (int i = 1; i < numSamples; i++) {
        if (buffer[i] > maxVal) maxVal = buffer[i];
        if (buffer[i] < minVal) minVal = buffer[i];
    }

 //   int16_t max = (maxVal > minVal ? maxVal : minVal);
    int peak = (abs(maxVal) > abs(minVal)) ? abs(maxVal) : abs(minVal);
    int16_t free = 32767 - peak;

    

}

float searchMaxIncreaseVolume(int16_t* buffer, int numSamples) {
    int16_t maxVal = buffer[0];
    int16_t minVal = buffer[0];

    for (int i = 1; i < numSamples; i++) {
        if (buffer[i] > maxVal) maxVal = buffer[i];
        if (buffer[i] < minVal) minVal = buffer[i];
    }

    // ищем пиковое значение по модулю
    int peak = (abs(maxVal) > abs(minVal)) ? abs(maxVal) : abs(minVal);

    if (peak == 0) return 1.0f;  // тишина, масштабировать бессмысленно

    return 32767.0f / peak;
}

void increaseVolume(int16_t* buffer, int numSamples, float factor = 0.5) {
  float scale = searchMaxIncreaseVolume(buffer, numSamples) * factor;
  
  for (int i = 0; i < numSamples ; i++) {
   int32_t amplified = (int32_t)(buffer[i] * scale);  // увеличиваем на 30% // todo

    // Ограничим, чтобы не вылезти за пределы int16_t
    if (amplified > 32767) amplified = 32767;
    if (amplified < -32768) amplified = -32768;

    buffer[i] = (int16_t)amplified;
  }
}

// float searchMaxIncreaseVolume(int16_t* buffer, int numSamples) {
//   int max = buffer[0];
//   int min = buffer[0];
//   for (int i = 1; i < numSamples; i++) {
//     if (buffer[i] > max) max = buffer[i];
//     if (buffer[i] < min) min = buffer[i];
//   }

//   float maxIncrease = 1 + (32767 - max) / 32767;
//   float minIncrease = 0;

//   if (min < 0) minIncrease = 1 + (-32768 - min) / -32768;

//   float scale = maxIncrease;
//   if (minIncrease < maxIncrease) scale = minIncrease;

//   return scale;
// }

#endif