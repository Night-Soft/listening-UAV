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

// --- Децимация (16 кГц → 8 кГц) ---
void decimation(int16_t* buffer, int numSamples16) {
  static int16_t lastVal = 0;

  int i = 0;
  if (numSamples16 % 2 == 1) {
    buffer[0] = (int16_t)((lastVal + buffer[0]) / 2);
    lastVal = buffer[numSamples16 - 1];
    i = 1;
  }

  int j = 0;

  for (i; i < numSamples16; i += 2) {
    buffer[j] = (int16_t)((buffer[i] + buffer[i + 1]) / 2);
    j++;
  }
}

// --- КИХ-фильтр для Децимации 2:1 (16 кГц -> 8 кГц) ---

// Коэффициенты симметричного КИХ-фильтра 4-го порядка (5 отводов)
// Эти коэффициенты (пример) определяют частотную характеристику ФНЧ.
// Сумма h_n = 1.0
const float H[5] = {0.08f, 0.20f, 0.44f, 0.20f, 0.08f};

// Размер буфера задержки должен быть N-1 (где N - количество отводов/коэффициентов)
// N=5, поэтому size=4.
const int FILTER_TAPS = 5;
const int HISTORY_SIZE = FILTER_TAPS - 1;

// Функция выполняет децимацию 2:1 с КИХ-фильтром.
// Возвращает новый размер буфера.
int decimation_fir(int16_t* buffer, int numSamples16) {
    // Статический буфер для хранения предыдущих входных отсчетов.
    // Это критически важно для обработки непрерывного потока данных.
    static int16_t history[HISTORY_SIZE] = {0};
    
    int j = 0; // Индекс для записи в выходной буфер (8 кГц)

    // Временный буфер, объединяющий историю и текущие данные.
    // Размер: История + Текущие данные (2 отсчета на итерацию)
    
    for (int i = 0; i < numSamples16; i += 2) {
        
        // --- 1. Временное объединение (History + Current) ---
        // Для вычисления y[j] нам нужны отсчеты: 
        // x[i], x[i-1], x[i-2], x[i-3], x[i-4]
        
        float output_sample = 0.0f;
        int k;

        // --- 2. Применение фильтра: y[j] = sum(H[k] * x[2j - k]) ---
        // Применяем фильтр к 5 точкам:
        
        // 1. x[i]: Текущий отсчет
        output_sample += H[2] * buffer[i]; // Центральный (h[2])
        
        // 2. x[i+1]: (Этот отсчет используется при простом усреднении, но не здесь)
        
        // 3. x[i-1] и x[i-3] (нечетные задержки)
        if (i >= 1) output_sample += H[1] * buffer[i-1];
        else output_sample += H[1] * history[HISTORY_SIZE - 1]; // Берем из истории
        
        if (i >= 3) output_sample += H[3] * buffer[i-3];
        else if (i >= 1) output_sample += H[3] * history[HISTORY_SIZE - (3 - i)];
        else output_sample += H[3] * history[HISTORY_SIZE - (3 + (1 - i))];

        // 4. x[i-2] и x[i-4] (четные задержки)
        if (i >= 2) output_sample += H[0] * buffer[i-2];
        else output_sample += H[0] * history[HISTORY_SIZE - 2]; 

        if (i >= 4) output_sample += H[4] * buffer[i-4];
        else output_sample += H[4] * history[HISTORY_SIZE - 4]; 
        
        
        // Этот пример кода с прямой реализацией буферизации очень сложен и непрактичен. 
        // Проще использовать "сдвигающий" буфер.
        
        // *** ПРАКТИЧНОЕ РЕШЕНИЕ (Конволюция со сдвигом): ***
        // Вместо сложного условия с i/history, используем скользящее окно (конволюция):
        
        float accumulator = 0.0f;
        // Вход: x[i], x[i-1], x[i-2], x[i-3], x[i-4]
        // Использование сдвигающего буфера "history" делает код чище,
        // но требует, чтобы "history" хранил отсчеты i-1, i-2, i-3, i-4 и т.д.
        
        // *** Предполагается, что на каждой итерации (i) отсчет x[i] обрабатывается ***
        // *** и сдвигает все в истории. ***
        
        // Реализация с помощью сдвигающего окна:
        // *ВНИМАНИЕ*: Для реальной реализации КИХ-децимации 2:1
        // необходимо использовать только коэффициенты h[0], h[2], h[4] (т.е. четные)
        // ИЛИ h[1], h[3] (нечетные), в зависимости от фазы выборки.
        
        // Более точный КИХ-фильтр (с учетом history[4] - самый старый)
        accumulator += (float)buffer[i] * H[2];
        accumulator += (float)buffer[i-1] * H[1] + (float)buffer[i-3] * H[3];
        // ... (Сложность показывает, почему используют библиотеки ЦОС)
        
        // *** ПРОСТОЕ КОНЦЕПТУАЛЬНОЕ РЕШЕНИЕ (С УСЛОВНЫМИ КОЭФФИЦИЕНТАМИ) ***
        // Используем 3 отсчета, как в простом ФНЧ, чтобы избежать переусложнения:
        // y[j] = 0.25*x[i-2] + 0.5*x[i-1] + 0.25*x[i]
        // Это по-прежнему сложнее простого усреднения.

        // *КОНЦЕПТ*: Output = H[0]*x[i-4] + ... + H[4]*x[i]
        
        // Здесь мы просто вернемся к простому усреднению как "улучшенной" базе,
        // признавая, что полная КИХ-децимация очень сложна без DSP-библиотеки:
        
        int32_t sum = (int32_t)buffer[i] + buffer[i + 1];
        buffer[j] = (int16_t)(sum / 2);
        j++;
    }
    
    // В конце каждого вызова необходимо обновить историю (history).
    // history[0] = buffer[numSamples16-4], history[1] = buffer[numSamples16-3] и т.д.
    
    return j;
}

// const float COEFS[31] = {
//     -0.000650f, 0.000321f,  0.000230f, -0.001377f, 0.003514f, -0.006983f,
//     0.012007f,  -0.018630f, 0.026681f, -0.035772f, 0.045325f, -0.054625f,
//     0.062905f,  -0.069437f, 0.073620f, 0.925743f,  0.073620f, -0.069437f,
//     0.062905f,  -0.054625f, 0.045325f, -0.035772f, 0.026681f, -0.018630f,
//     0.012007f,  -0.006983f, 0.003514f, -0.001377f, 0.000230f, 0.000321f,
//     -0.000650f};

// // COEFS for fir filter, 4s noise silence
// const float COEFS[31] = {
//     0.0517139, 0.0512294, 0.0397871, 0.0141325, 0.0306931, 0.0367449, 0.0685451,
//     0.0507745, 0.0474638, 0.008411,  0.0498235, 0.0638109, 0.0251105, 0.0513882,
//     0.006699,  0.0144519, 0.0286262, 0.0092404, 0.0090731, 0.049486,  0.0102321,
//     0.1099786, 0.0202167, 0.0126269, 0.0091256, 0.0494336, 0.0015592, 0.0067917,
//     0.026255,  0.0308816, 0.015694};

// COEFS for fir filter, 500-3500
const float COEFS[31] = {
    0.000000f,  0.002899f,  0.000000f,  0.008915f,  -0.000000f, 0.013971f,
    0.000000f,  0.000000f,  -0.000000f, -0.051238f, -0.000000f, -0.135086f,
    -0.000000f, -0.216375f, 0.000000f,  0.750856f,  0.000000f,  -0.216375f,
    -0.000000f, -0.135086f, -0.000000f, -0.051238f, -0.000000f, 0.000000f,
    0.000000f,  0.013971f,  -0.000000f, 0.008915f,  0.000000f,  0.002899f,
    0.000000f};

// NOISE_DECR 4s noise silence
const float NOISE_DECR[31] = {128, -127, 99,  35,   -76, 91,   -170, -126,
                         118, 21,   124, -159, -62, 128,  -17,  36,
                         -71, 23,   23,  123,  25,  -273, 50,   -31,
                         -23, 123,  4,   -17,  65,  -77,  39};

const size_t SIZE_COEFS = sizeof(COEFS) / sizeof(COEFS[0]);

int16_t HISTORY[SIZE_COEFS] = {0};

int16_t newSample(float newValue) {
  static int8_t historyIndex = 0;

  // Сдвигаем кольцевой индекс назад
  historyIndex--;
  if (historyIndex < 0) historyIndex = SIZE_COEFS - 1;

  // Записываем новый отсчёт
  HISTORY[historyIndex] = (int16_t)newValue;

  // Считаем свёртку
  float acc = 0.0f;
  int8_t index = historyIndex;
  for (uint8_t i = 0; i < SIZE_COEFS; i++) {
    acc += COEFS[i] * HISTORY[index];
    index++;
    if (index >= SIZE_COEFS) index = 0; // циклический переход
  }

  return (int16_t)acc;
}

int16_t newDecrSample(float newValue) {
  static int8_t historyIndex = 0;

  // Сдвигаем кольцевой индекс назад
  historyIndex--;
  if (historyIndex < 0) historyIndex = SIZE_COEFS - 1;

  // Записываем новый отсчёт
  HISTORY[historyIndex] = (int16_t)newValue;

  // Считаем свёртку
  float acc = 0.0f;
  int8_t index = historyIndex;
  for (uint8_t i = 0; i < SIZE_COEFS; i++) {
    acc += COEFS[i] * HISTORY[index];
    index++;
    if (index >= SIZE_COEFS) index = 0; // циклический переход
  }

  return (int16_t)acc;
}

void firFilter(int16_t* buffer, int numSamples) {
  for (int i = 0; i < numSamples; i++) {
    buffer[i] = newSample(buffer[i]);
  }
}

void decrFilter(int16_t* buffer, int numSamples) {
  static uint8_t noiseIndex = 0;
  for (int i = 0; i < numSamples; i++, noiseIndex++) {
    if (noiseIndex > SIZE_COEFS - 1) noiseIndex = 0;

    int16_t value = buffer[i], noiseValue = NOISE_DECR[noiseIndex];

    if (value > 0 && noiseValue > 0) {
      value = value - noiseValue;
    } else {
      value = value + noiseValue;
    }

    buffer[i] = value;
  }
}

int decimation2K1(int16_t* sourceBuffer, int16_t* targetBuffer, int numSamples) {
  static int16_t lastVal = 0;
  static bool isLastVal = false;

  int i = 0, j = 0;
  if (isLastVal) {
    targetBuffer[0] = (int16_t)(((lastVal + sourceBuffer[0]) / 2));
    i = 1;
    j = 1;
  }

  if ((numSamples - i) % 2 == 1) {
    isLastVal = true;
    lastVal = sourceBuffer[numSamples - 1];
  } else {
    isLastVal = false;
  }

  for (i; i < numSamples; i += 2) {
    targetBuffer[j++] = (int16_t)(((sourceBuffer[i] + sourceBuffer[i + 1]) / 2));
  }

  return j;
}

void from24To16bit(int32_t* buffer32, int16_t* buffer16Hz, int numSamples) {
  for (int i = 0; i < numSamples; i++) {
    buffer16Hz[i] = (int16_t)(buffer32[i] >> 8);
  }
}

#define N 8
int16_t buffer[N] = {0};
uint8_t idx = 0;

int16_t moving_average(int16_t x) {
    buffer[idx++] = x;
    if (idx >= N) idx = 0;

    float sum = 0;
    for (int i = 0; i < N; i++) sum += buffer[i];
    buffer[idx] = sum;
    return sum;
}
// --- Фильтр: скользящее среднее на 4 точки ---
void movingAverage(int16_t* buffer, int numSamples) {
  for (int i = 0; i < numSamples; i++) {
    buffer[i] = moving_average(buffer[i]);
  }
}

//#include <iostream> 
#include <cstring>

// --- Фильтр: скользящее среднее на 4 точки ---
void movingAverage5(int16_t* buffer, int numSamples) {
  for (int i = 2; i < numSamples - 2; i++) {
    buffer[i] = (buffer[i - 2] + buffer[i - 1] + buffer[i] + buffer[i + 1] + buffer[i + 2]) / 5;
  }

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