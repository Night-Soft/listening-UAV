#include <Arduino.h>
#include <math.h>

#include "referenceAudio.h"

#define SAMPLE_RATE 8000
#define SAMPLE_SIZE 8000  // 1 секунда

// Тестовые массивы
int16_t* reference_audio = referenceAudio;
int16_t* current_audio = nullptr;

// Структура для хранения результатов бенчмарка
struct BenchmarkResult {
    const char* method_name;
    unsigned long time_us;      // Время в микросекундах
    float time_ms;              // Время в миллисекундах
    unsigned long cycles;       // Количество CPU циклов
    float result = 0;
};

// ============================================
// МЕТОД 1: RMS
// ============================================
float calculateRMS(int16_t* audio, int length) {
    float sum = 0;
    for (int i = 0; i < length; i++) {
        float normalized = audio[i] / 32768.0f;
        sum += normalized * normalized;
    }
    return sqrt(sum / length);
}

BenchmarkResult benchmark_RMS() {
    BenchmarkResult result;
    result.method_name = "RMS расчёт";
    
    unsigned long start = micros();
    
    Serial.println("calculateRMS");
    float rms1 = calculateRMS(reference_audio, SAMPLE_SIZE);
    float rms2 = calculateRMS(current_audio, SAMPLE_SIZE);
    float difference = abs(rms1 - rms2) / max(rms1, rms2);
    Serial.println("End calculateRMS");
    
    unsigned long end = micros();
    
    result.time_us = end - start;
    result.time_ms = result.time_us / 1000.0;
    result.cycles = result.time_us * 240;  // ESP32 @ 240MHz
    result.result = difference;
    return result;
}

// ============================================
// МЕТОД 2: Zero Crossing Rate
// ============================================
float calculateZCR(int16_t* audio, int length) {
    int crossings = 0;
    for (int i = 1; i < length; i++) {
        if ((audio[i] >= 0 && audio[i-1] < 0) || 
            (audio[i] < 0 && audio[i-1] >= 0)) {
            crossings++;
        }
    }
    return (float)crossings / length;
}

BenchmarkResult benchmark_ZCR() {
    BenchmarkResult result;
    result.method_name = "Zero Crossing Rate";
    
    unsigned long start = micros();
    
    float zcr1 = calculateZCR(reference_audio, SAMPLE_SIZE);
    float zcr2 = calculateZCR(current_audio, SAMPLE_SIZE);
    float difference = abs(zcr1 - zcr2) / max(zcr1, zcr2);
    
    unsigned long end = micros();
    
    result.time_us = end - start;
    result.time_ms = result.time_us / 1000.0;
    result.cycles = result.time_us * 240;

    result.result = difference;
    return result;
}

// ============================================
// МЕТОД 3: Корреляция (полная)
// ============================================
void normalizeAudio(int16_t* audio, int length, float* output) {
    float mean = 0;
    for (int i = 0; i < length; i++) {
        mean += audio[i];
    }
    mean /= length;
    
    float variance = 0;
    for (int i = 0; i < length; i++) {
        float diff = audio[i] - mean;
        variance += diff * diff;
    }
    float stddev = sqrt(variance / length);
    
    if (stddev > 0) {
        for (int i = 0; i < length; i++) {
            output[i] = (audio[i] - mean) / stddev;
        }
    } else {
        for (int i = 0; i < length; i++) {
            output[i] = 0;
        }
    }
}

float calculateCorrelation(int16_t* audio1, int16_t* audio2, int length) {
    float* norm1 = (float*)malloc(length * sizeof(float));
    float* norm2 = (float*)malloc(length * sizeof(float));
    
    if (norm1 == NULL || norm2 == NULL) {
        free(norm1);
        free(norm2);
        return 0;
    }
    
    normalizeAudio(audio1, length, norm1);
    normalizeAudio(audio2, length, norm2);
    
    float correlation = 0;
    for (int i = 0; i < length; i++) {
        correlation += norm1[i] * norm2[i];
    }
    correlation /= length;
    
    free(norm1);
    free(norm2);
    
    return correlation;
}

BenchmarkResult benchmark_Correlation() {
    BenchmarkResult result;
    result.method_name = "Корреляция (полная)";
    
    unsigned long start = micros();
    
    float corr = calculateCorrelation(reference_audio, current_audio, SAMPLE_SIZE);
    
    unsigned long end = micros();
    
    result.time_us = end - start;
    result.time_ms = result.time_us / 1000.0;
    result.cycles = result.time_us * 240;
    
    result.result = corr;
    return result;
}

// ============================================
// МЕТОД 4: Корреляция (сегментированная)
// ============================================
float segmentCorrelation(int16_t* audio1, int16_t* audio2, int length, int segments) {
    int segment_size = length / segments;
    float total_correlation = 0;
    
    for (int seg = 0; seg < segments; seg++) {
        int start = seg * segment_size;
        float corr = calculateCorrelation(&audio1[start], &audio2[start], segment_size);
        total_correlation += corr;
    }
    
    return total_correlation / segments;
}

BenchmarkResult benchmark_SegmentCorrelation(int segments) {
    BenchmarkResult result;
    static char name[50];
    sprintf(name, "Корреляция (%d сегментов)", segments);
    result.method_name = name;
    
    unsigned long start = micros();
    
    float corr = segmentCorrelation(reference_audio, current_audio, SAMPLE_SIZE, segments);
    
    unsigned long end = micros();
    
    result.time_us = end - start;
    result.time_ms = result.time_us / 1000.0;
    result.cycles = result.time_us * 240;
    
    result.result = corr;
    return result;
}

// ============================================
// МЕТОД 5: DTW (упрощённый)
// ============================================
float simpleDTW(int16_t* audio1, int16_t* audio2, int length, int window_size) {
    float min_distance = INFINITY;
    
    for (int shift = -window_size; shift <= window_size; shift++) {
        float distance = 0;
        int count = 0;
        
        for (int i = 0; i < length; i++) {
            int j = i + shift;
            if (j >= 0 && j < length) {
                float diff = (audio1[i] - audio2[j]) / 32768.0f;
                distance += diff * diff;
                count++;
            }
        }
        
        if (count > 0) {
            distance = sqrt(distance / count);
            if (distance < min_distance) {
                min_distance = distance;
            }
        }
    }
    
    return 1.0 - min_distance;
}

BenchmarkResult benchmark_DTW(int window_size) {
    BenchmarkResult result;
    static char name[50];
    sprintf(name, "DTW (окно %d)", window_size);
    result.method_name = name;
    
    unsigned long start = micros();
    
    float dtw = simpleDTW(reference_audio, current_audio, SAMPLE_SIZE, window_size);
    
    unsigned long end = micros();
    
    result.time_us = end - start;
    result.time_ms = result.time_us / 1000.0;
    result.cycles = result.time_us * 240;
    
    result.result = dtw;
    return result;
}

// ============================================
// МЕТОД 6: Быстрая корреляция (без нормализации)
// ============================================
float fastCorrelation(int16_t* audio1, int16_t* audio2, int length) {
    long long sum_product = 0;
    long long sum1_sq = 0;
    long long sum2_sq = 0;
    
    for (int i = 0; i < length; i++) {
        sum_product += (long long)audio1[i] * audio2[i];
        sum1_sq += (long long)audio1[i] * audio1[i];
        sum2_sq += (long long)audio2[i] * audio2[i];
    }
    
    float denom = sqrt((float)sum1_sq * sum2_sq);
    if (denom == 0) return 0;
    
    return sum_product / denom;
}

BenchmarkResult benchmark_FastCorrelation() {
    BenchmarkResult result;
    result.method_name = "Корреляция (быстрая)";
    
    unsigned long start = micros();
    
    float corr = fastCorrelation(reference_audio, current_audio, SAMPLE_SIZE);
    
    unsigned long end = micros();
    
    result.time_us = end - start;
    result.time_ms = result.time_us / 1000.0;
    result.cycles = result.time_us * 240;
    
    result.result = corr;
    return result;
}

struct Res {
  float rms_diff;
  float corr;
  float zcr_diff;
  float similarity;
  const char* method_name;
};
// ============================================
// ПОЛНОЕ СРАВНЕНИЕ (все методы)
// ============================================
BenchmarkResult benchmark_FullComparison() {
  BenchmarkResult result;
  result.method_name = "ПОЛНОЕ сравнение";

  unsigned long start = micros();

  // RMS
  float rms1 = calculateRMS(reference_audio, SAMPLE_SIZE);
  float rms2 = calculateRMS(current_audio, SAMPLE_SIZE);
  float rms_diff = abs(rms1 - rms2) / max(rms1, rms2);

  // Корреляция
  float corr =
      calculateCorrelation(reference_audio, current_audio, SAMPLE_SIZE);

  // ZCR
  float zcr1 = calculateZCR(reference_audio, SAMPLE_SIZE);
  float zcr2 = calculateZCR(current_audio, SAMPLE_SIZE);
  float zcr_diff = abs(zcr1 - zcr2) / max(zcr1, zcr2);

  // Итоговая оценка
  float corr_score = (corr + 1.0) / 2.0;
  float rms_score = 1.0 - rms_diff;
  float zcr_score = 1.0 - zcr_diff;
  float similarity =
      (corr_score * 0.5 + rms_score * 0.3 + zcr_score * 0.2) * 100;

  unsigned long end = micros();

  result.time_us = end - start;
  result.time_ms = result.time_us / 1000.0;
  result.cycles = result.time_us * 240;

  result.result = similarity;
  return result;
}

Res fullComparison() {
  Res result;
  result.method_name = "ПОЛНОЕ сравнение";

  unsigned long start = micros();

  // RMS
  float rms1 = calculateRMS(reference_audio, SAMPLE_SIZE);
  float rms2 = calculateRMS(current_audio, SAMPLE_SIZE);
  float rms_diff = abs(rms1 - rms2) / max(rms1, rms2);

  // Корреляция
  float corr =
      calculateCorrelation(reference_audio, current_audio, SAMPLE_SIZE);

  // ZCR
  float zcr1 = calculateZCR(reference_audio, SAMPLE_SIZE);
  float zcr2 = calculateZCR(current_audio, SAMPLE_SIZE);
  float zcr_diff = abs(zcr1 - zcr2) / max(zcr1, zcr2);

  // Итоговая оценка
  float corr_score = (corr + 1.0) / 2.0;
  float rms_score = 1.0 - rms_diff;
  float zcr_score = 1.0 - zcr_diff;
  float similarity =
      (corr_score * 0.5 + rms_score * 0.3 + zcr_score * 0.2) * 100;

  unsigned long end = micros();


  result.rms_diff = rms_diff;
  result.corr = corr;
  result.zcr_diff = zcr_diff;
  result.similarity = similarity;

  return result;
}
// ============================================
// ОПТИМИЗИРОВАННОЕ СРАВНЕНИЕ (RMS + быстрая корреляция)
// ============================================
BenchmarkResult benchmark_OptimizedComparison() {
    BenchmarkResult result;
    result.method_name = "ОПТИМИЗИРОВАННОЕ сравнение";
    
    unsigned long start = micros();
    
    // Быстрая RMS проверка
    float rms1 = calculateRMS(reference_audio, SAMPLE_SIZE);
    float rms2 = calculateRMS(current_audio, SAMPLE_SIZE);
    float rms_diff = abs(rms1 - rms2) / max(rms1, rms2);
    
    // Если RMS сильно отличается - не тратим время на корреляцию
    if (rms_diff < 0.5) {
        float corr = fastCorrelation(reference_audio, current_audio, SAMPLE_SIZE);
        result.result = corr;
    }
    
    unsigned long end = micros();
    
    result.time_us = end - start;
    result.time_ms = result.time_us / 1000.0;
    result.cycles = result.time_us * 240;
    
    return result;
}

// ============================================
// ВЫВОД РЕЗУЛЬТАТОВ
// ============================================
void printResult(BenchmarkResult result) {
    Serial.println("─────────────────────────────────────────────────");
    Serial.printf("Метод: %s\n", result.method_name);
    // RESULT
    Serial.printf("RESULT: %.1f%% от времени записи\n", result.result);

    Serial.printf("Время: %lu мкс (%.2f мс)\n", result.time_us, result.time_ms);
    Serial.printf("CPU циклов: %lu (@ 240 MHz)\n", result.cycles);
    
    // Сколько раз можно выполнить за секунду
    float per_second = 1000000.0 / result.time_us;
    Serial.printf("Макс. частота: %.1f раз/сек\n", per_second);
    
    // Процент от времени записи (1 секунда)
    float percent = (result.time_ms / 1000.0) * 100;
    Serial.printf("Нагрузка: %.1f%% от времени записи\n", percent);

}

void printResult(Res result) {
    Serial.println("\n─────────────────────────────────────────────────\n");

    Serial.printf("Метод: %s\n", result.method_name);
    // RESULT
    Serial.printf("rms_diff: %.2f%%\n", result.rms_diff);
    Serial.printf("corr: %.2f%%\n", result.corr);
    Serial.printf("zcr_diff: %.2f%%\n", result.zcr_diff);
    Serial.printf("similarity: %.2f%%\n", result.similarity);

    Serial.println("\n─────────────────────────────────────────────────\n");
}

void printMemoryUsage() {
    Serial.println("\n═════════════════════════════════════════════════");
    Serial.println("ИСПОЛЬЗОВАНИЕ ПАМЯТИ:");
    Serial.println("═════════════════════════════════════════════════");
    
    // Массивы int16_t
    Serial.printf("Эталонный массив: %d байт (%.1f KB)\n", 
                  SAMPLE_SIZE * 2, SAMPLE_SIZE * 2 / 1024.0);
    Serial.printf("Текущий массив: %d байт (%.1f KB)\n", 
                  SAMPLE_SIZE * 2, SAMPLE_SIZE * 2 / 1024.0);
    
    // Временные массивы для корреляции
    Serial.printf("Temp массивы (корреляция): %d байт (%.1f KB)\n", 
                  SAMPLE_SIZE * 4 * 2, SAMPLE_SIZE * 4 * 2 / 1024.0);
    
    // Общее
    int total = SAMPLE_SIZE * 2 * 2 + SAMPLE_SIZE * 4 * 2;
    Serial.printf("ИТОГО (пик): %d байт (%.1f KB)\n", total, total / 1024.0);
    
    // Доступная память
    Serial.printf("\nСвободная SRAM: %d байт (%.1f KB)\n", 
                  ESP.getFreeHeap(), ESP.getFreeHeap() / 1024.0);
}

void compare() {
   // Serial.begin(115200);
    //delay(1000);
    
    // ============================================
    // ЗАПУСК БЕНЧМАРКОВ
    // ============================================
    
    Serial.println("\n█ ОТДЕЛЬНЫЕ МЕТОДЫ:\n");
    
    // printResult(benchmark_RMS());
    // delay(100);
    
    // printResult(benchmark_ZCR());
    // delay(100);
    
    // printResult(benchmark_FastCorrelation());
    // delay(100);
    
    // printResult(benchmark_Correlation());
    // delay(100);
    
    // printResult(benchmark_SegmentCorrelation(4));
    // delay(100);
    
    // printResult(benchmark_SegmentCorrelation(10));
    // delay(100);
    
    // printResult(benchmark_DTW(50));
    // delay(100);
    
    // printResult(benchmark_DTW(200));
    // delay(100);
    
    // Serial.println("\n\n█ КОМБИНИРОВАННЫЕ МЕТОДЫ:\n");
    
    // printResult(benchmark_OptimizedComparison());
    // delay(100);
    
    printResult(fullComparison());
    delay(100);
    
    // Вывод информации о памяти
   // printMemoryUsage();
}
