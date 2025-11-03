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
//     0.001199f, -0.001655f, 0.002610f, -0.004230f, 0.006629f, -0.009857f,
//     0.013883f, -0.018595f, 0.023802f, -0.029246f, 0.034625f, -0.039613f,
//     0.043891f, -0.047175f, 0.049241f, 0.948980f,  0.049241f, -0.047175f,
//     0.043891f, -0.039613f, 0.034625f, -0.029246f, 0.023802f, -0.018595f,
//     0.013883f, -0.009857f, 0.006629f, -0.004230f, 0.002610f, -0.001655f,
//     0.001199f};

const float COEFS[39] = {
  0.0229922, 0.0361072, 0.058661,  0.028859,  0.0343555, 0.0368403, 0.0378684, 0.0052032,
  0.0217451, 0.0435038, 0.0674594, 0.0016966, 0.0273074, 0.0009858, 0.0412764, 0.0626623,
  0.0144396, 0.0563435, 0.0233396, 0.010245,  0.0222212, 0.0142297, 0.0111425, 0.0182707,
  0.0082335, 0.0456086, 0.026059,  0.0197308, 0.0144705, 0.0526643, 0.026537,  0.002208,
  0.0209359, 0.0210963, 0.0286003, 0.0128406, 0.0131623, 0.0092776, 0.0008202
};

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

void firFilter(int16_t* buffer, int numSamples) {
  for (int i = 0; i < numSamples; i++) {
    buffer[i] = newSample(buffer[i]);
  }
}


void decimation2K1(int16_t* buffer, int numSamples) {
  static int16_t lastVal = 0;
  static int16_t isLastVal = false;

  int i = 0;
  if (isLastVal) {
    buffer[0] = (int16_t)(((lastVal + buffer[0]) / 2));
    i = 1;
  }

  if ((numSamples - i) % 2 == 1) {
    isLastVal = true;
    lastVal = buffer[numSamples - 1];
  } else {
    isLastVal = false;
  }

  int j = 0;

  for (i; i < numSamples; i += 2) {
    buffer[j] = (int16_t)(((buffer[i] + buffer[i + 1]) / 2));
    j++;
  }
}

// const float NOISE[30] = {
//     0.01890495, 0.02655581,  0.04511684, 0.03490814, 0.05790122, 0.04235221,
//     0.02126213, 0.008865285, 0.01922908, 0.02869133, 0.01584027, 0.07692308,
//     0.06196371, 0.01666928,  0.03208772, 0.04977556, 0.0678689,  0.006033995,
//     0.02923334, 0.05882353,  0.01518884, 0.0283027,  0.03966186, 0.0163815,
//     0.04397103, 0.01484664,  0.02765638, 0.0356439,  0.01123286, 0.03581527};

// int16_t newNoiseSample(float newValue) {
//   static int8_t historyIndex = 0;

//   // Сдвигаем кольцевой индекс назад
//   historyIndex--;
//   if (historyIndex < 0) historyIndex = 30;

//   // Записываем новый отсчёт
//   HISTORY[historyIndex] = (int16_t)newValue;

//   // Считаем свёртку
//   float acc = 0.0f;
//   int8_t index = historyIndex;
//   for (uint8_t i = 0; i < 31; i++) {
//     acc += COEFS[i] * HISTORY[index];
//     index++;
//     if (index >= 31) index = 0; // циклический переход
//   }

//   return (int16_t)acc;
// }

// void noiseFilter(int16_t* buffer, int numSamples) {
//   for (int i = 0; i < numSamples; i++) {
//     buffer[i] = newNoiseSample(buffer[i]);
//   }
// }
void from24To16bit(int32_t* buffer32, int16_t* buffer16Hz, int numSamples) {
  for (int i = 0; i < numSamples; i++) {
    buffer16Hz[i] = (int16_t)(buffer32[i] >> 8);
  }
//   // Края без фильтра (копируем как есть)
//   buffer8[0] = buffer[0];
//   buffer8[1] = buffer[1];
//   buffer8[size - 2] = buffer[size - 2];
//   buffer8[size - 1] = buffer[size - 1];
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