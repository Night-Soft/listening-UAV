#include "Arduino.h"

// ============ HIGH-PASS ФИЛЬТР ============
// Глобальные переменные для хранения состояния фильтра
float hpf_x1 = 0, hpf_x2 = 0, hpf_y1 = 0, hpf_y2 = 0;
float hpf_b0, hpf_b1, hpf_b2, hpf_a1, hpf_a2;

// Инициализация high-pass фильтра (вызвать один раз в setup)
void initHighPassFilter(float cutoffFreq) {
  float sampleRate = 8000.0;
  float Q = 0.707;
  float w0 = 2.0 * PI * cutoffFreq / sampleRate;
  float cosw0 = cos(w0);
  float sinw0 = sin(w0);
  float alpha = sinw0 / (2.0 * Q);
  
  float a0 = 1.0 + alpha;
  hpf_b0 = ((1.0 + cosw0) / 2.0) / a0;
  hpf_b1 = (-(1.0 + cosw0)) / a0;
  hpf_b2 = ((1.0 + cosw0) / 2.0) / a0;
  hpf_a1 = (-2.0 * cosw0) / a0;
  hpf_a2 = (1.0 - alpha) / a0;
  
  hpf_x1 = hpf_x2 = hpf_y1 = hpf_y2 = 0;
}

// Применить high-pass фильтр к буферу
void applyHighPassFilter(int16_t* buffer, int numSamples) {
  for (int i = 0; i < numSamples; i++) {
    float input = (float)buffer[i];
    
    float output = hpf_b0 * input + hpf_b1 * hpf_x1 + hpf_b2 * hpf_x2 
                   - hpf_a1 * hpf_y1 - hpf_a2 * hpf_y2;
    
    hpf_x2 = hpf_x1;
    hpf_x1 = input;
    hpf_y2 = hpf_y1;
    hpf_y1 = output;
    
    buffer[i] = (int16_t)output;
  }
}

// ============ NOISE GATE ============
float gate_envelope = 0;
float gate_threshold = 500.0;  // Порог
float gate_attackCoeff = 0.9;  // ~1мс на 8кГц
float gate_releaseCoeff = 0.01; // ~100мс на 8кГц

void initNoiseGate(float threshold) {
  gate_threshold = threshold;
  gate_envelope = 0;
  gate_attackCoeff = 1.0 - exp(-1.0 / (0.001 * 8000.0));
  gate_releaseCoeff = 1.0 - exp(-1.0 / (0.1 * 8000.0));
}

void applyNoiseGate(int16_t* buffer, int numSamples) {
  for (int i = 0; i < numSamples; i++) {
    float input = abs((float)buffer[i]);
    
    // Обновляем envelope
    if (input > gate_envelope) {
      gate_envelope += (input - gate_envelope) * gate_attackCoeff;
    } else {
      gate_envelope += (input - gate_envelope) * gate_releaseCoeff;
    }
    
    // Применяем gate
    if (gate_envelope < gate_threshold) {
      buffer[i] = 0;
    }
  }
}

// ============ УДАЛЕНИЕ КЛИППИНГА (ЛИМИТЕР) ============
void applyLimiter(int16_t* buffer, int numSamples, float threshold) {
  int16_t maxVal = (int16_t)(32767.0 * threshold); // 32767 = max int16_t
  
  for (int i = 0; i < numSamples; i++) {
    if (buffer[i] > maxVal) {
      buffer[i] = maxVal;
    } else if (buffer[i] < -maxVal) {
      buffer[i] = -maxVal;
    }
  }
}

// ============ SOFT CLIPPER (мягкое ограничение) ============
void applySoftClipper(int16_t* buffer, int numSamples) {
  for (int i = 0; i < numSamples; i++) {
    float x = (float)buffer[i] / 32767.0; // Нормализуем к [-1, 1]
    
    // Мягкое ограничение (tanh-подобная функция)
    if (x > 0.9) {
      x = 0.9 + (x - 0.9) * 0.1;
    } else if (x < -0.9) {
      x = -0.9 + (x + 0.9) * 0.1;
    }
    
    buffer[i] = (int16_t)(x * 32767.0);
  }
}

// ============ УДАЛЕНИЕ ЩЕЛЧКОВ (DE-CLICK) ============
int16_t declick_prevSample = 0;
const int16_t CLICK_THRESHOLD = 5000; // Порог для обнаружения щелчка

void applyDeclick(int16_t* buffer, int numSamples) {
  for (int i = 0; i < numSamples; i++) {
    int16_t diff = abs(buffer[i] - declick_prevSample);
    
    // Если резкий скачок - интерполируем
    if (diff > CLICK_THRESHOLD) {
      buffer[i] = (declick_prevSample + buffer[i]) / 2;
    }
    
    declick_prevSample = buffer[i];
  }
}

// ============ DC OFFSET REMOVAL (удаление постоянной составляющей) ============
float dc_accumulator = 0;

void removeDCOffset2(int16_t* buffer, int numSamples) {
  const float alpha = 0.995; // Коэффициент фильтра
  
  for (int i = 0; i < numSamples; i++) {
    float input = (float)buffer[i];
    dc_accumulator = alpha * dc_accumulator + input;
    buffer[i] = (int16_t)(input - dc_accumulator * (1.0 - alpha));
  }
}

// ============ НОРМАЛИЗАЦИЯ (автоматическая регулировка громкости) ============
void normalizeAudio(int16_t* buffer, int numSamples, float targetLevel) {
  // Находим максимальное значение
  int16_t maxAbs = 0;
  for (int i = 0; i < numSamples; i++) {
    int16_t absVal = abs(buffer[i]);
    if (absVal > maxAbs) {
      maxAbs = absVal;
    }
  }
  
  if (maxAbs == 0) return; // Избегаем деления на 0
  
  // Вычисляем коэффициент усиления
  float gain = (32767.0 * targetLevel) / maxAbs;
  
  // Ограничиваем усиление
  if (gain > 4.0) gain = 4.0; // Не усиливаем больше чем в 4 раза
  
  // Применяем усиление
  for (int i = 0; i < numSamples; i++) {
    int32_t temp = (int32_t)(buffer[i] * gain);
    if (temp > 32767) temp = 32767;
    if (temp < -32767) temp = -32767;
    buffer[i] = (int16_t)temp;
  }
}

// ============ ПРИМЕР ПОЛНОЙ ОБРАБОТКИ ============
void processAudioBuffer(int16_t* buffer, int numSamples) {
  //removeDCOffset2(buffer, numSamples);           // 1. Убираем DC offset
  applyHighPassFilter(buffer, numSamples);      // 2. High-pass фильтр
  applyDeclick(buffer, numSamples);             // 3. Удаляем щелчки
  applyNoiseGate(buffer, numSamples);           // 4. Noise gate
  applySoftClipper(buffer, numSamples);         // 5. Мягкое ограничение
  normalizeAudio(buffer, numSamples, 0.8);      // 6. Нормализация до 80%
}

// ============ SETUP ============
// void setup() {
//   Serial.begin(115200);
  
//   // Инициализируем фильтры
//   initHighPassFilter(100.0);  // Срез на 100 Гц
//   initNoiseGate(500.0);       // Порог 500
  
//   // Ваш код инициализации микрофона...
// }

// // ============ LOOP ============
// void loop() {
//   int16_t audioBuffer[512];  // Ваш буфер
//   int numSamples = 512;
  
//   // Читаем данные с микрофона
//   // readMicrophone(audioBuffer, numSamples);
  
//   // Обрабатываем
//   processAudioBuffer(audioBuffer, numSamples);
  
//   // Используем обработанный аудио
//   // sendAudio(audioBuffer, numSamples);
// }

// ============ АДАПТИВНОЕ СКОЛЬЗЯЩЕЕ СРЕДНЕЕ ============
// Автоматически подстраивает силу сглаживания
float adaptive_prev = 0;
bool adaptive_initialized = false;

void initAdaptiveMA() {
  adaptive_prev = 0;
  adaptive_initialized = false;
}

void applyAdaptiveMA(int16_t* buffer, int numSamples) {
  for (int i = 0; i < numSamples; i++) {
    if (!adaptive_initialized) {
      adaptive_prev = buffer[i];
      adaptive_initialized = true;
      continue;
    }
    
    float input = buffer[i];
    float diff = abs(input - adaptive_prev);
    
    // Если большая разница - меньше сглаживаем (быстрая реакция)
    // Если малая разница - больше сглаживаем (убираем шум)
    float alpha;
    if (diff > 1000) {
      alpha = 0.8; // Быстрая реакция на большие изменения
    } else {
      alpha = 0.2; // Сильное сглаживание для малых изменений
    }
    
    adaptive_prev = alpha * input + (1.0 - alpha) * adaptive_prev;
    buffer[i] = (int16_t)adaptive_prev;
  }
}

#include <cmath>
#include <algorithm>

// Линейная интерполяция между двумя сэмплами
inline int16_t linearInterpolate(int16_t sample1, int16_t sample2, float fraction) {
    return (int16_t)(sample1 + (sample2 - sample1) * fraction);
}

// Ресемплинг с линейной интерполяцией
// sourceSampleRate - частота исходного сигнала (например, 44000)
// targetSampleRate - целевая частота (например, 8000, 16000, 22050)
void resampleLinear(int16_t* targetBuffer, const int16_t* sourceBuffer,
                    int sourceNumSamples, int sourceSampleRate, int targetSampleRate) {
    
    float ratio = (float)sourceSampleRate / (float)targetSampleRate;
    int targetNumSamples = (int)(sourceNumSamples / ratio);
    
    for (int i = 0; i < targetNumSamples; i++) {
        float sourceIndex = i * ratio;
        int index1 = (int)sourceIndex;
        int index2 = index1 + 1;
        
        // Проверка границ
        if (index2 >= sourceNumSamples) {
            index2 = sourceNumSamples - 1;
        }
        
        float fraction = sourceIndex - index1;
        targetBuffer[i] = linearInterpolate(sourceBuffer[index1], sourceBuffer[index2], fraction);
    }
}

// Кубическая интерполяция (более качественная)
inline int16_t cubicInterpolate(int16_t y0, int16_t y1, int16_t y2, int16_t y3, float mu) {
    float mu2 = mu * mu;
    float a0 = y3 - y2 - y0 + y1;
    float a1 = y0 - y1 - a0;
    float a2 = y2 - y0;
    float a3 = y1;
    
    float result = a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3;
    
    // Ограничение диапазона int16_t
    if (result > 32767.0f) result = 32767.0f;
    if (result < -32768.0f) result = -32768.0f;
    
    return (int16_t)result;
}

// Ресемплинг с кубической интерполяцией
void resampleCubic(int16_t* targetBuffer, const int16_t* sourceBuffer,
                   int sourceNumSamples, int sourceSampleRate, int targetSampleRate) {
    
    float ratio = (float)sourceSampleRate / (float)targetSampleRate;
    int targetNumSamples = (int)(sourceNumSamples / ratio);
    
    for (int i = 0; i < targetNumSamples; i++) {
        float sourceIndex = i * ratio;
        int index = (int)sourceIndex;
        
        // Получаем 4 точки для кубической интерполяции
        int16_t y0 = (index - 1 >= 0) ? sourceBuffer[index - 1] : sourceBuffer[index];
        int16_t y1 = sourceBuffer[index];
        int16_t y2 = (index + 1 < sourceNumSamples) ? sourceBuffer[index + 1] : sourceBuffer[index];
        int16_t y3 = (index + 2 < sourceNumSamples) ? sourceBuffer[index + 2] : sourceBuffer[index];
        
        float fraction = sourceIndex - index;
        targetBuffer[i] = cubicInterpolate(y0, y1, y2, y3, fraction);
    }
}

// Простая версия с антиалиасинг фильтром (усреднение окна)
void resampleWithAA(int16_t* targetBuffer, const int16_t* sourceBuffer,
                    int sourceNumSamples, int sourceSampleRate, int targetSampleRate) {
    
    float ratio = (float)sourceSampleRate / (float)targetSampleRate;
    int targetNumSamples = (int)(sourceNumSamples / ratio);
    
    // Если уменьшаем частоту (downsampling), применяем сглаживание
    if (ratio > 1.0f) {
        int windowSize = (int)ratio;
        
        for (int i = 0; i < targetNumSamples; i++) {
            int centerIndex = (int)(i * ratio);
            int32_t sum = 0;
            int count = 0;
            
            // Усредняем окно вокруг центральной точки
            for (int j = -windowSize/2; j <= windowSize/2; j++) {
                int idx = centerIndex + j;
                if (idx >= 0 && idx < sourceNumSamples) {
                    sum += sourceBuffer[idx];
                    count++;
                }
            }
            
            targetBuffer[i] = (int16_t)(sum / count);
        }
    } else {
        // Если увеличиваем частоту (upsampling), используем линейную интерполяцию
        resampleLinear(targetBuffer, sourceBuffer, sourceNumSamples, sourceSampleRate, targetSampleRate);
    }
}

// Вспомогательная функция для вычисления размера выходного буфера
int getResampledSize(int sourceNumSamples, int sourceSampleRate, int targetSampleRate) {
    return (int)(sourceNumSamples * targetSampleRate / (float)sourceSampleRate);
}

// Пример использования
/*
int main() {
    const int SOURCE_RATE = 44000;
    const int TARGET_RATE = 8000;
    const int SOURCE_SAMPLES = 44000; // 1 секунда
    
    int16_t sourceBuffer[SOURCE_SAMPLES];
    // ... заполнение sourceBuffer данными ...
    
    int targetSamples = getResampledSize(SOURCE_SAMPLES, SOURCE_RATE, TARGET_RATE);
    int16_t* targetBuffer = new int16_t[targetSamples];
    
    // Выбираем метод:
    
    // 1. Линейная интерполяция (быстро, среднее качество)
    resampleLinear(targetBuffer, sourceBuffer, SOURCE_SAMPLES, SOURCE_RATE, TARGET_RATE);
    
    // 2. Кубическая интерполяция (медленнее, лучше качество)
    // resampleCubic(targetBuffer, sourceBuffer, SOURCE_SAMPLES, SOURCE_RATE, TARGET_RATE);
    
    // 3. С антиалиасингом (для downsampling)
    // resampleWithAA(targetBuffer, sourceBuffer, SOURCE_SAMPLES, SOURCE_RATE, TARGET_RATE);
    
    delete[] targetBuffer;
    return 0;
}
*/