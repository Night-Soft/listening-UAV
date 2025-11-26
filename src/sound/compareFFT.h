/*
 * Спектральное сравнение аудио с использованием arduinoFFT
 * 
 * Установка библиотеки:
 * Sketch -> Include Library -> Manage Libraries -> arduinoFFT
 * или
 * https://github.com/kosme/arduinoFFT
 */

#include <Arduino.h>
#include <arduinoFFT.h>

// Параметры аудио
#define SAMPLE_RATE 8000

#define CURRENT_AUDIO_SIZE 16384 // 1024*8
#define REFERENCE_SIZE 4096

#define FFT_SIZE 1024        // Должно быть степенью 2
#define NUM_BINS (FFT_SIZE / 2)

// Массивы для хранения аудио
// #include "reference/uavApproachingt.h"
#include "reference/audioSamples1.h"
// #include "reference/audioSamples2.h"
// #include "reference/uavMovesAway.h"

// #include "reference/startRef.h"
// #include "reference/startRef2.h"
// #include "reference/midddleRef1.h"
// #include "reference/midddleRef2.h"

//#include "reference/startRef17_10_25.h"
#include "reference/startRef217_10_25.h"
#include "reference/midddleRef117_10_25.h"
#include "reference/midddleRef217_10_25.h"
#include "reference/awayRef17_10_25.h"

 const int16_t* reference_audio = referenceAudio1; // no need
// const int16_t* reference_audio2 = referenceAudio2;
const int16_t* refsAudio[4] = {
  startRef217_10_25, 
  midddleRef117_10_25,       
  midddleRef217_10_25,
  awayRef17_10_25
};// startRef2

const char* nameReference[4] = {
  "startRef17_10_25", 
  "midddleRef117_10_25",
  "midddleRef217_10_25", 
  "awayRef17_10_25"
};

int16_t* current_audio = nullptr;

const int16_t* hzRanges[4] = {
    hzRange_startRef217_10_25,
    hzRange_midddleRef117_10_25,
    hzRange_midddleRef217_10_25,
    hzRange_awayRef17_10_25,
};

// Массивы для FFT (double для лучшей точности)
double vReal_ref[FFT_SIZE];
double vImag_ref[FFT_SIZE];
double vReal_cur[FFT_SIZE];
double vImag_cur[FFT_SIZE];

// Спектры (магнитуда после FFT)
double reference_spectrum[NUM_BINS];
double current_spectrum[NUM_BINS];

// Создание объекта FFT
//ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal_ref, vImag_ref, FFT_SIZE, SAMPLE_RATE);
ArduinoFFT<double> FFT = ArduinoFFT<double>();
// ============================================
// СТРУКТУРЫ ДАННЫХ
// ============================================

struct FrequencyBands {
    double low;        // 0-200 Hz
    double mid_low;    // 200-500 Hz
    double mid;        // 500-1000 Hz
    double mid_high;   // 1000-2000 Hz
    double high;       // 2000-4000 Hz
};

struct PeakInfo {
    double frequency;
    double magnitude;
    int bin_index;
};

struct SpectrumAnalysis {
    double cosine_similarity;
    double euclidean_distance;
    double band_similarity;
    double peak_similarity;
    double centroid_similarity;
    double overall_score;
    bool is_similar;
    
    PeakInfo ref_peak;
    PeakInfo cur_peak;
    FrequencyBands ref_bands;
    FrequencyBands cur_bands;
    double ref_centroid;
    double cur_centroid;
};

// ============================================
// ВЫЧИСЛЕНИЕ СПЕКТРА С ПОМОЩЬЮ arduinoFFT
// ============================================

void computeSpectrum(const int16_t* audio, int audio_length, double* vReal_cur, double* vImag_cur, double* spectrum) {
    // Шаг 1: Очистка массивов
    for (int i = 0; i < FFT_SIZE; i++) {
        vImag_cur[i] = 0.0;
    }
    
    // Шаг 2: Копирование и нормализация данных
    int samples_to_use = min(audio_length, FFT_SIZE);
    for (int i = 0; i < samples_to_use; i++) {
        vReal_cur[i] = audio[i] / 32768.0;  // Нормализация к [-1, 1]
    }
    
    // Заполняем остальное нулями (zero-padding)
    for (int i = samples_to_use; i < FFT_SIZE; i++) {
        vReal_cur[i] = 0.0;
    }
    
    // Шаг 3: Применение оконной функции Хэмминга
    // arduinoFFT поддерживает несколько типов окон
    ArduinoFFT<double> fft_temp = ArduinoFFT<double>(vReal_cur, vImag_cur, FFT_SIZE, SAMPLE_RATE);
    fft_temp.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    
    // Шаг 4: Выполнение FFT
    fft_temp.compute(FFTDirection::Forward);
    
    // Шаг 5: Вычисление магнитуды
    fft_temp.complexToMagnitude();
    
    // Шаг 6: Копирование результата в спектр
    // Используем только первую половину (до частоты Найквиста)
    // for (int i = 0; i < NUM_BINS; i++) {
    //     spectrum[i] = vReal_cur[i];
    // }

    // below can remove, uncoment above
    float hzInOneSample = SAMPLE_RATE / FFT_SIZE;  // 7,8125

    double energy = 0;
    int binStart = floor(400 / hzInOneSample);
    int binEnd = 1500;
    for (int i = 0; i < NUM_BINS; i++) {
      if (i>=binStart && i<= binEnd) {
        spectrum[i] = vReal_cur[i];
        continue;
      }
        spectrum[i] = 0;
    }

}

// Альтернативная функция для вычисления обоих спектров
void computeBothSpectrums() {
    unsigned long start = micros();
    
    // Спектр эталона
    Serial.println("Вычисление спектра эталона...");
    computeSpectrum(reference_audio, REFERENCE_SIZE, vReal_ref, vImag_ref, reference_spectrum);
    
    // Спектр текущего сигнала
    Serial.println("Вычисление спектра текущего сигнала...");
    computeSpectrum(current_audio, REFERENCE_SIZE, vReal_cur, vImag_cur, current_spectrum);
    
    unsigned long elapsed = micros() - start;
    Serial.printf("Время вычисления FFT: %lu мкс (%.2f мс)\n", elapsed, elapsed / 1000.0);
}

// ============================================
// ПОИСК ПИКОВОЙ ЧАСТОТЫ
// ============================================

// PeakInfo findPeakFrequency(double* spectrum, int start_bin, int end_bin) {
//     PeakInfo peak = {0, 0, 0};
//     double freq_resolution = (double)SAMPLE_RATE / FFT_SIZE;
    
//     for (int i = start_bin; i < end_bin && i < NUM_BINS; i++) {
//         if (spectrum[i] > peak.magnitude) {
//             peak.magnitude = spectrum[i];
//             peak.bin_index = i;
//             peak.frequency = i * freq_resolution;
//         }
//     }
    
//     return peak;
// }

// Использование arduinoFFT для поиска доминантной частоты
PeakInfo findPeakUsingArduinoFFT(double* vReal_cur) {
    PeakInfo peak;
    
    // arduinoFFT имеет встроенную функцию для поиска пика
    ArduinoFFT<double> fft_temp = ArduinoFFT<double>(vReal_cur, vImag_ref, FFT_SIZE, SAMPLE_RATE);
    peak.frequency = fft_temp.majorPeak();
    
    // Находим магнитуду и индекс
    double freq_resolution = (double)SAMPLE_RATE / FFT_SIZE;
    peak.bin_index = (int)(peak.frequency / freq_resolution);
    peak.magnitude = vReal_cur[peak.bin_index];
    
    return peak;
}

// ============================================
// АНАЛИЗ ЧАСТОТНЫХ ДИАПАЗОНОВ
// ============================================

FrequencyBands analyzeFrequencyBands(double* spectrum) {
    FrequencyBands bands = {0};
    double freq_resolution = (double)SAMPLE_RATE / FFT_SIZE;
    
    for (int i = 0; i < NUM_BINS; i++) {
        double freq = i * freq_resolution;
        
        if (freq < 200) {
            bands.low += spectrum[i];
        } else if (freq < 500) {
            bands.mid_low += spectrum[i];
        } else if (freq < 1000) {
            bands.mid += spectrum[i];
        } else if (freq < 2000) {
            bands.mid_high += spectrum[i];
        } else if (freq < 4000) {
            bands.high += spectrum[i];
        }
    }
    
    return bands;
}

double compareFrequencyBands(FrequencyBands b1, FrequencyBands b2) {
    // Нормализация к процентам
    double total1 = b1.low + b1.mid_low + b1.mid + b1.mid_high + b1.high;
    double total2 = b2.low + b2.mid_low + b2.mid + b2.mid_high + b2.high;
    
    if (total1 == 0 || total2 == 0) return 0;
    
    b1.low /= total1;
    b1.mid_low /= total1;
    b1.mid /= total1;
    b1.mid_high /= total1;
    b1.high /= total1;
    
    b2.low /= total2;
    b2.mid_low /= total2;
    b2.mid /= total2;
    b2.mid_high /= total2;
    b2.high /= total2;
    
    // Евклидово расстояние
    double diff = 0;
    diff += pow(b1.low - b2.low, 2);
    diff += pow(b1.mid_low - b2.mid_low, 2);
    diff += pow(b1.mid - b2.mid, 2);
    diff += pow(b1.mid_high - b2.mid_high, 2);
    diff += pow(b1.high - b2.high, 2);
    
    return 1.0 - sqrt(diff);
}

// ============================================
// МЕТОДЫ СРАВНЕНИЯ СПЕКТРОВ
// ============================================

// 1. Косинусное сходство
double cosineSimilarity(double* spec1, double* spec2, int length) {
    double dot_product = 0;
    double norm1 = 0;
    double norm2 = 0;
    
    for (int i = 0; i < length; i++) {
        dot_product += spec1[i] * spec2[i];
        norm1 += spec1[i] * spec1[i];
        norm2 += spec2[i] * spec2[i];
    }
    
    double denominator = sqrt(norm1 * norm2);
    if (denominator == 0) return 0;
    
    return dot_product / denominator;
}

// 2. Евклидово расстояние
double euclideanDistance(double* spec1, double* spec2, int length) {
    double sum = 0;
    for (int i = 0; i < length; i++) {
        double diff = spec1[i] - spec2[i];
        sum += diff * diff;
    }
    return sqrt(sum / length);
}

// 3. Спектральный центроид
double spectralCentroid(double* spectrum, int length) {
    double weighted_sum = 0;
    double total_sum = 0;
    double freq_resolution = (double)SAMPLE_RATE / FFT_SIZE;
    
    for (int i = 0; i < length; i++) {
        double freq = i * freq_resolution;
        weighted_sum += freq * spectrum[i];
        total_sum += spectrum[i];
    }
    
    if (total_sum == 0) return 0;
    return weighted_sum / total_sum;
}

// 4. Сравнение пиков
double comparePeaks(PeakInfo p1, PeakInfo p2) {
    if (p1.frequency == 0 || p2.frequency == 0) return 0;
    
    double freq_diff = abs(p1.frequency - p2.frequency) / max(p1.frequency, p2.frequency);
    double mag_diff = abs(p1.magnitude - p2.magnitude) / max(p1.magnitude, p2.magnitude);
    
    return 1.0 - (freq_diff * 0.6 + mag_diff * 0.4);
}

// ============================================
// ГЛАВНАЯ ФУНКЦИЯ АНАЛИЗА
// ============================================

SpectrumAnalysis analyzeSpectrums(SpectrumAnalysis &result) {
    //SpectrumAnalysis result = {0};
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║   СПЕКТРАЛЬНЫЙ АНАЛИЗ (arduinoFFT)     ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    // Вычисление спектров
   // computeBothSpectrums();
    
    // 1. Косинусное сходство
    result.cosine_similarity = cosineSimilarity(reference_spectrum, current_spectrum, NUM_BINS);
    Serial.printf("✓ Косинусное сходство: %.4f\n", result.cosine_similarity);
    
    // 2. Евклидово расстояние
    result.euclidean_distance = euclideanDistance(reference_spectrum, current_spectrum, NUM_BINS);
    Serial.printf("✓ Евклидово расстояние: %.4f\n", result.euclidean_distance);
    
    // 3. Анализ частотных диапазонов
    result.ref_bands = analyzeFrequencyBands(reference_spectrum);
    result.cur_bands = analyzeFrequencyBands(current_spectrum);
    result.band_similarity = compareFrequencyBands(result.ref_bands, result.cur_bands);
    Serial.printf("✓ Сходство диапазонов: %.4f\n", result.band_similarity);
    
    // Детали по диапазонам
    double ref_total = result.ref_bands.low + result.ref_bands.mid_low + 
                      result.ref_bands.mid + result.ref_bands.mid_high + result.ref_bands.high;
    double cur_total = result.cur_bands.low + result.cur_bands.mid_low + 
                      result.cur_bands.mid + result.cur_bands.mid_high + result.cur_bands.high;
    
    Serial.println("\n  Распределение энергии по диапазонам:");
    Serial.printf("  0-200 Hz:    %.1f%% → %.1f%%\n", 
                  (result.ref_bands.low/ref_total)*100, 
                  (result.cur_bands.low/cur_total)*100);
    Serial.printf("  200-500 Hz:  %.1f%% → %.1f%%\n", 
                  (result.ref_bands.mid_low/ref_total)*100, 
                  (result.cur_bands.mid_low/cur_total)*100);
    Serial.printf("  500-1000 Hz: %.1f%% → %.1f%%\n", 
                  (result.ref_bands.mid/ref_total)*100, 
                  (result.cur_bands.mid/cur_total)*100);
    Serial.printf("  1000-2000Hz: %.1f%% → %.1f%%\n", 
                  (result.ref_bands.mid_high/ref_total)*100, 
                  (result.cur_bands.mid_high/cur_total)*100);
    Serial.printf("  2000-4000Hz: %.1f%% → %.1f%%\n\n", 
                  (result.ref_bands.high/ref_total)*100, 
                  (result.cur_bands.high/cur_total)*100);
    
    // 4. Поиск пиков (с использованием arduinoFFT majorPeak)
    result.ref_peak = findPeakUsingArduinoFFT(vReal_ref);
    result.cur_peak = findPeakUsingArduinoFFT(vReal_cur);
    result.peak_similarity = comparePeaks(result.ref_peak, result.cur_peak);
    
    Serial.printf("✓ Доминантные частоты:\n");
    Serial.printf("  Эталон:  %.1f Hz (магнитуда: %.2f)\n", 
                  result.ref_peak.frequency, result.ref_peak.magnitude);
    Serial.printf("  Текущий: %.1f Hz (магнитуда: %.2f)\n", 
                  result.cur_peak.frequency, result.cur_peak.magnitude);
    Serial.printf("  Сходство пиков: %.4f\n\n", result.peak_similarity);
    
    // 5. Спектральный центроид
    result.ref_centroid = spectralCentroid(reference_spectrum, NUM_BINS);
    result.cur_centroid = spectralCentroid(current_spectrum, NUM_BINS);
    double centroid_diff = abs(result.ref_centroid - result.cur_centroid) / 
                          max(result.ref_centroid, result.cur_centroid);
    result.centroid_similarity = 1.0 - centroid_diff;
    
    Serial.printf("✓ Спектральные центроиды:\n");
    Serial.printf("  Эталон:  %.1f Hz\n", result.ref_centroid);
    Serial.printf("  Текущий: %.1f Hz\n", result.cur_centroid);
    Serial.printf("  Сходство: %.4f\n\n", result.centroid_similarity);
    
    // 6. Комбинированная оценка
    result.overall_score = (
        result.cosine_similarity * 0.35 +
        result.band_similarity * 0.25 +
        result.peak_similarity * 0.25 +
        result.centroid_similarity * 0.15
    ) * 100.0;
    
    // 7. Определение похожести
    result.is_similar = (result.overall_score > 70.0) && 
                       (result.cosine_similarity > 0.65);
    
    // Вывод результата
    Serial.println("════════════════════════════════════════");
    Serial.printf(">>> ИТОГОВАЯ ОЦЕНКА: %.1f%%\n", result.overall_score);
    Serial.printf(">>> РЕЗУЛЬТАТ: %s %s\n", 
                  result.is_similar ? "ПОХОЖ" : "НЕ ПОХОЖ",
                  result.is_similar ? "✓" : "✗");
    Serial.println("════════════════════════════════════════\n");
    
    return result;
}

// ============================================
// ВИЗУАЛИЗАЦИЯ СПЕКТРА
// ============================================

void printSpectrumGraph(double* spectrum, const char* title) {
    Serial.printf("\n--- %s ---\n", title);
    
    int display_bins = 50;
    int bins_per_group = NUM_BINS / display_bins;
    double freq_resolution = (double)SAMPLE_RATE / FFT_SIZE;
    
    // Поиск максимума для нормализации
    double max_magnitude = 0;
    for (int i = 0; i < NUM_BINS; i++) {
        if (spectrum[i] > max_magnitude) {
            max_magnitude = spectrum[i];
        }
    }
    
    Serial.println("Freq(Hz)  │ Magnitude");
    Serial.println("──────────┼────────────────────────────────────────────────");
    
    for (int i = 0; i < display_bins; i++) {
        double freq_start = (i * bins_per_group) * freq_resolution;
        
        // Усреднение магнитуды в группе
        double avg_magnitude = 0;
        for (int j = 0; j < bins_per_group; j++) {
            int idx = i * bins_per_group + j;
            if (idx < NUM_BINS) {
                avg_magnitude += spectrum[idx];
            }
        }
        avg_magnitude /= bins_per_group;
        
        // Нормализация для отображения
        int bar_length = (int)((avg_magnitude / max_magnitude) * 40);
        
        Serial.printf("%7.0f   │ ", freq_start);
        for (int k = 0; k < bar_length; k++) {
            Serial.print("█");
        }
        Serial.printf(" %.3f\n", avg_magnitude);
    }
    Serial.println();
}

// ============================================
// БЕНЧМАРК
// ============================================

void benchmarkFFT() {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║        БЕНЧМАРК arduinoFFT             ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    unsigned long times[10];
    
    for (int i = 0; i < 10; i++) {
        unsigned long start = micros();
        computeSpectrum(reference_audio, REFERENCE_SIZE, vReal_ref, vImag_ref, reference_spectrum);
        times[i] = micros() - start;
    }
    
    // Статистика
    unsigned long sum = 0;
    unsigned long min_time = times[0];
    unsigned long max_time = times[0];
    
    for (int i = 0; i < 10; i++) {
        sum += times[i];
        if (times[i] < min_time) min_time = times[i];
        if (times[i] > max_time) max_time = times[i];
    }
    
    unsigned long avg = sum / 10;
    
    Serial.printf("Размер FFT: %d точек\n", FFT_SIZE);
    Serial.printf("Частота дискретизации: %d Hz\n", SAMPLE_RATE);
    Serial.printf("Частотное разрешение: %.2f Hz/bin\n\n", (double)SAMPLE_RATE / FFT_SIZE);
    
    Serial.println("Результаты 10 запусков:");
    for (int i = 0; i < 10; i++) {
        Serial.printf("  #%d: %lu мкс (%.2f мс)\n", i+1, times[i], times[i]/1000.0);
    }
    
    Serial.printf("\nМинимум: %lu мкс (%.2f мс)\n", min_time, min_time/1000.0);
    Serial.printf("Среднее: %lu мкс (%.2f мс)\n", avg, avg/1000.0);
    Serial.printf("Максимум: %lu мкс (%.2f мс)\n", max_time, max_time/1000.0);
    
    double fps = 1000000.0 / avg;
    Serial.printf("\nМаксимальная частота анализа: %.1f раз/сек\n", fps);
    
    double load_percent = (avg / 1000.0) / (1000.0) * 100;
    Serial.printf("Нагрузка при анализе 1 раз/сек: %.2f%%\n\n", load_percent);
}

// ============================================
// SETUP И LOOP
// ============================================

// void compareFFT() {
//     // Serial.begin(115200);
//     // delay(2000);
    
//     // Serial.println("\n\n");
//     // Serial.println("╔════════════════════════════════════════╗");
//     // Serial.println("║  СПЕКТРАЛЬНОЕ СРАВНЕНИЕ АУДИО         ║");
//     // Serial.println("║  с использованием arduinoFFT          ║");
//     // Serial.println("╚════════════════════════════════════════╝");
//     // Serial.printf("\nCPU: ESP32 @ %d MHz\n", getCpuFrequencyMhz());
//     // Serial.printf("Свободная память: %d байт (%.1f KB)\n", 
//     //               ESP.getFreeHeap(), ESP.getFreeHeap()/1024.0);
    
//     // // Генерация тестовых данных
//     // Serial.println("\nГенерация тестовых сигналов...");
//     // Serial.println("Эталон: Двигатель 100 Hz + гармоники");
//     // Serial.println("Текущий: Похожий сигнал с вариациями\n");
    
//     // for (int i = 0; i < REFERENCE_SIZE; i++) {
//     //     double t = i / (double)SAMPLE_RATE;
        
//     //     // Эталонный сигнал: двигатель на 100 Hz
//     //     double ref_signal = 
//     //         sin(2 * PI * 100 * t) * 10000 +      // Основная частота
//     //         sin(2 * PI * 200 * t) * 5000 +       // 2-я гармоника
//     //         sin(2 * PI * 300 * t) * 2500 +       // 3-я гармоника
//     //         sin(2 * PI * 400 * t) * 1000 +       // 4-я гармоника
//     //         (random(-300, 300));                  // Шум
        
//     //     // Текущий сигнал: немного выше обороты + больше шума
//     //     double cur_signal = 
//     //         sin(2 * PI * 105 * t) * 9500 +       // Чуть выше частота
//     //         sin(2 * PI * 210 * t) * 4800 +       
//     //         sin(2 * PI * 315 * t) * 2400 +       
//     //         sin(2 * PI * 420 * t) * 950 +        
//     //         (random(-400, 400));                  // Больше шума
        
//     //     reference_audio[i] = (int16_t)ref_signal;
//     //     current_audio[i] = (int16_t)cur_signal;
//     // }
    
//     // delay(1000);
    
//     // Запуск бенчмарка
//     //benchmarkFFT();
    
//     // Выполнение анализа
//     SpectrumAnalysis result = analyzeSpectrums();
    
//     // Визуализация спектров
//    // printSpectrumGraph(reference_spectrum, "ЭТАЛОННЫЙ СПЕКТР");
//     //printSpectrumGraph(current_spectrum, "ТЕКУЩИЙ СПЕКТР");
    
//     // Интерпретация результата
//     Serial.println("═══════════════════════════════════════════════════");
//     Serial.println("ИНТЕРПРЕТАЦИЯ РЕЗУЛЬТАТОВ:");
//     Serial.println("═══════════════════════════════════════════════════");
    
//     if (result.is_similar) {
//         Serial.println("✓ ЗВУК ПОХОЖ НА ЭТАЛОННЫЙ ДВИГАТЕЛЬ");
        
//         if (abs(result.ref_peak.frequency - result.cur_peak.frequency) > 10) {
//             double rpm_diff = (result.cur_peak.frequency - result.ref_peak.frequency) / 
//                             result.ref_peak.frequency * 100;
//             Serial.printf("  → Обороты изменились на %.1f%%\n", rpm_diff);
//         }
        
//         if (result.cur_bands.high / (result.cur_bands.low + 0.001) > 
//             result.ref_bands.high / (result.ref_bands.low + 0.001) * 1.5) {
//             Serial.println("  ⚠ Увеличился уровень высокочастотного шума");
//         }
//     } else {
//         Serial.println("✗ ЗВУК НЕ СООТВЕТСТВУЕТ ЭТАЛОНУ");
        
//         if (result.cosine_similarity < 0.5) {
//             Serial.println("  → Совершенно другая частотная структура");
//         } else if (result.peak_similarity < 0.6) {
//             Serial.println("  → Основная частота сильно отличается");
//         } else if (result.band_similarity < 0.6) {
//             Serial.println("  → Нарушено распределение энергии по частотам");
//         }
//     }
    
//     Serial.println("═══════════════════════════════════════════════════\n");
//     Serial.println("✓ Анализ завершён!");
// }

void printResult(SpectrumAnalysis &result) {
        // Интерпретация результата
    Serial.println("═══════════════════════════════════════════════════");
    Serial.println("ИНТЕРПРЕТАЦИЯ РЕЗУЛЬТАТОВ:");
    Serial.println("═══════════════════════════════════════════════════");
    
    if (result.is_similar) {
        Serial.println("✓ ЗВУК ПОХОЖ НА ЭТАЛОННЫЙ ДВИГАТЕЛЬ");
        
        if (abs(result.ref_peak.frequency - result.cur_peak.frequency) > 10) {
            double rpm_diff = (result.cur_peak.frequency - result.ref_peak.frequency) / 
                            result.ref_peak.frequency * 100;
            Serial.printf("  → Обороты изменились на %.1f%%\n", rpm_diff);
        }
        
        if (result.cur_bands.high / (result.cur_bands.low + 0.001) > 
            result.ref_bands.high / (result.ref_bands.low + 0.001) * 1.5) {
            Serial.println("  ⚠ Увеличился уровень высокочастотного шума");
        }
    } else {
        Serial.println("✗ ЗВУК НЕ СООТВЕТСТВУЕТ ЭТАЛОНУ");
        
        if (result.cosine_similarity < 0.5) {
            Serial.println("  → Совершенно другая частотная структура");
        } else if (result.peak_similarity < 0.6) {
            Serial.println("  → Основная частота сильно отличается");
        } else if (result.band_similarity < 0.6) {
            Serial.println("  → Нарушено распределение энергии по частотам");
        }
    }
    
    Serial.println("═══════════════════════════════════════════════════\n");
    Serial.println("✓ Анализ завершён!");
}

SpectrumAnalysis getResult() {
    SpectrumAnalysis result = {0};
    
    // Вычисление спектров
    // createBothSpectrums();
    
    // 1. Косинусное сходство
    result.cosine_similarity = cosineSimilarity(reference_spectrum, current_spectrum, NUM_BINS);
    
    // 2. Евклидово расстояние
    result.euclidean_distance = euclideanDistance(reference_spectrum, current_spectrum, NUM_BINS);
    
    // 3. Анализ частотных диапазонов
    result.ref_bands = analyzeFrequencyBands(reference_spectrum);
    result.cur_bands = analyzeFrequencyBands(current_spectrum);
    result.band_similarity = compareFrequencyBands(result.ref_bands, result.cur_bands);
    
    // Детали по диапазонам
    double ref_total = result.ref_bands.low + result.ref_bands.mid_low + 
                      result.ref_bands.mid + result.ref_bands.mid_high + result.ref_bands.high;
    double cur_total = result.cur_bands.low + result.cur_bands.mid_low + 
                      result.cur_bands.mid + result.cur_bands.mid_high + result.cur_bands.high;
    
    // 4. Поиск пиков (с использованием arduinoFFT majorPeak)
    result.ref_peak = findPeakUsingArduinoFFT(vReal_ref);
    result.cur_peak = findPeakUsingArduinoFFT(vReal_cur);
    result.peak_similarity = comparePeaks(result.ref_peak, result.cur_peak);

    // 5. Спектральный центроид
    result.ref_centroid = spectralCentroid(reference_spectrum, NUM_BINS);
    result.cur_centroid = spectralCentroid(current_spectrum, NUM_BINS);
    double centroid_diff = abs(result.ref_centroid - result.cur_centroid) / 
                          max(result.ref_centroid, result.cur_centroid);
    result.centroid_similarity = 1.0 - centroid_diff;
    
    // 6. Комбинированная оценка
    result.overall_score = (
        result.cosine_similarity * 0.35 +
        result.band_similarity * 0.25 +
        result.peak_similarity * 0.25 +
        result.centroid_similarity * 0.15
    ) * 100.0;
    
    // 7. Определение похожести
    result.is_similar = (result.overall_score >= 73.0); // && (result.cosine_similarity > 0.65);
    
    return result;
}

SpectrumAnalysis getFinalResult(SpectrumAnalysis& prevScore,
                                SpectrumAnalysis& prevCosine) {
  SpectrumAnalysis& finalResult = prevScore;
//   finalResult.overall_score =
//       (prevScore.overall_score + prevCosine.overall_score) / 2;
//   finalResult.cosine_similarity =
//       (prevScore.cosine_similarity + prevCosine.cosine_similarity) / 2;

  finalResult.overall_score;
  finalResult.cosine_similarity = prevCosine.cosine_similarity;

  finalResult.euclidean_distance =
      (prevScore.euclidean_distance + prevCosine.euclidean_distance) /
      2;
  finalResult.band_similarity =
      (prevScore.band_similarity + prevCosine.band_similarity) / 2;
  finalResult.peak_similarity =
      (prevScore.peak_similarity + prevCosine.peak_similarity) / 2;
  finalResult.centroid_similarity =
      (prevScore.centroid_similarity + prevCosine.centroid_similarity) /
      2;
  finalResult.ref_centroid =
      (prevScore.ref_centroid + prevCosine.ref_centroid) / 2;
  finalResult.cur_centroid =
      (prevScore.cur_centroid + prevCosine.cur_centroid) / 2;

  finalResult.is_similar = (finalResult.overall_score >= 73.0); // && (finalResult.cosine_similarity > 0.65);

  return finalResult;
}

bool fullCompare() {
  static SpectrumAnalysis prevScore, prevCosine;

  unsigned long start = micros();
  uint8_t index = 0;
  bool stop = false;
  for (uint8_t k = 0; k < 4 && !stop; k++) {
    const int16_t* ref = refsAudio[k];
    
    Serial.print("\nCompare: ");
    Serial.print(nameReference[k]);
    //Serial.println(k);

    for (uint8_t i = 0; i < REFERENCE_SIZE / FFT_SIZE && !stop; i++) {
      computeSpectrum(&ref[i * FFT_SIZE], FFT_SIZE, vReal_ref, vImag_ref,
                      reference_spectrum);

      for (uint8_t j = 0; j < CURRENT_AUDIO_SIZE / FFT_SIZE; j++) {
        computeSpectrum(&current_audio[j * FFT_SIZE], FFT_SIZE, vReal_cur,
                        vImag_cur, current_spectrum);
        SpectrumAnalysis currentResult = getResult();

        if (index == 0) {
          prevScore = currentResult;
          prevCosine = currentResult;
        }

        if (currentResult.overall_score > prevScore.overall_score) {
          prevScore = currentResult;
        }
        if (currentResult.cosine_similarity > prevCosine.cosine_similarity) {
          prevCosine = currentResult;
        }
        // if (currentResult.is_similar) {
        //   //return true;
        //   Serial.println("Break");
        //   stop = true;
        //   break;
        // }
      }
    }
  }

  SpectrumAnalysis finalResult = getFinalResult(prevScore, prevCosine);

  // Вывод результата
  Serial.println("════════════════════════════════════════");
  Serial.println("result");

  Serial.printf(">>> ИТОГОВАЯ ОЦЕНКА: %.1f%%\n", finalResult.overall_score);
  Serial.printf(">>> COSINE_SIMILARITY: %.1f%%\n",
                finalResult.cosine_similarity);
  Serial.printf(">>> РЕЗУЛЬТАТ: %s %s\n",
                finalResult.is_similar ? "ПОХОЖ" : "НЕ ПОХОЖ",
                finalResult.is_similar ? "✓" : "✗");
  Serial.println("════════════════════════════════════════\n");

  //printResult(finalResult);
  analyzeSpectrums(finalResult);
  unsigned long elapsed = micros() - start;
  Serial.printf("Время вычисления FFT: %lu мкс (%.2f мс)\n", elapsed,
                elapsed / 1000.0);
  return finalResult.is_similar;
}


//next compare


bool newComapre() {

    return true;
}

float getDiffCompare() {
  // --- Сравнение спектров ---
  double diff = 0;
  for (int i = 0; i < NUM_BINS; i++) {
    double d = vReal_ref[i] - vReal_cur[i];
    diff += d * d;
  }
  diff = sqrt(diff / (NUM_BINS));

  // avgSpectrum[i] = (spec1[i] + spec2[i] + spec3[i]) / 3.0;

  // Нормируем результат (0 = идеально, выше = хуже)
  return (float)diff;
}


void createSpecRef(const int16_t * samples) {
  for (int i = 0; i < FFT_SIZE; i++) {
    vReal_ref[i] = samples[i] / 32768.0;  // Нормализация к [-1, 1]
    vImag_ref[i] = 0;
  }
 
  FFT.windowing(vReal_ref, FFT_SIZE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
  FFT.compute(vReal_ref, vImag_ref, FFT_SIZE, FFT_FORWARD);
  FFT.complexToMagnitude(vReal_ref, vImag_ref, FFT_SIZE);
}

double getCompresedSpectrr(const int16_t* samples, int size) {
  Serial.println("Inside getCompresedSpectrr");
  double avgSpectrum[NUM_BINS] = {0};
  for (int i = 0; i < size / FFT_SIZE; i++) {
    createSpecRef(&samples[i * FFT_SIZE]);
   // Serial.println("createSpecRef");

    for (int i = 0; i < NUM_BINS; i++) {
      avgSpectrum[i] = avgSpectrum[i] + vReal_ref[i];
    }
  }

  float hzInOneSample = -7.8125;//SAMPLE_RATE / FFT_SIZE;  // 7,8125

  for (int i = 0; i < NUM_BINS; i++) {
    avgSpectrum[i] = avgSpectrum[i] / 4;
    Serial.print("hz: ");
    Serial.print(hzInOneSample += 7.8125);
    Serial.print(" | ");
    Serial.println(avgSpectrum[i]);
  }

  return *avgSpectrum;
}

double getCompresedSpectr() {
  const int16_t* ref = refsAudio[0];
   getCompresedSpectrr(&ref[1], 4096);
   return 0.1;
}

void createSpecMic(const int16_t * samples) {
  for (int i = 0; i < FFT_SIZE; i++) {
    vReal_cur[i] = samples[i] / 32768.0;  // Нормализация к [-1, 1]
    vImag_cur[i] = 0;
  }

  FFT.windowing(vReal_cur, FFT_SIZE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
  FFT.compute(vReal_cur, vImag_cur, FFT_SIZE, FFT_FORWARD);
  FFT.complexToMagnitude(vReal_cur, vImag_cur, FFT_SIZE);
}

bool fullCompare(bool ts) {
  static float results[192] = {};
  unsigned long start = micros();
  uint8_t index = 0;
  bool stop = false;
  for (uint8_t k = 0; k < 4 && !stop; k++) {
    const int16_t* ref = refsAudio[k];
    
    Serial.print("\nCompare: ");
    Serial.print(nameReference[k]);
    Serial.println(k);

    for (uint8_t i = 0; i < REFERENCE_SIZE / FFT_SIZE && !stop; i++) {
      createSpecRef(&ref[i * FFT_SIZE]);

      for (uint8_t j = 0; j < CURRENT_AUDIO_SIZE / FFT_SIZE; j++) {
        createSpecMic(&current_audio[j * FFT_SIZE]);
        float result = getDiffCompare();
        results[index++] = result;
      }
    }
  }

  // Вывод результата
  Serial.println("════════════════════════════════════════");

  for (uint8_t i = 0; i < index; i++) {
    Serial.println(results[i]);
  }

  Serial.println("════════════════════════════════════════\n");


  unsigned long elapsed = micros() - start;
  Serial.printf("Время вычисления FFT: %lu мкс (%.2f мс)\n", elapsed,
                elapsed / 1000.0);
  return true;
}

//#define NUM_WINDOWS 4
#define SAMPLING_FREQ 8000
#include "sound/spectr.h"
#include "reference/SilenceSpectr.h"
#include <cmath> // Required for std::isnan()
#include <iostream>

struct Result {
  float energy;
  float distance;
};

float micHz[512] = {0};
float refHz[512] = {0};

void computeFrequency(float* hzArr, double* spectr, int binStart, int binEnd) {
  int spectrSize = binEnd - binStart;

  float distance = 0;
  float currentBin = 0;
  int idxHz = 0;
  for (int i = 0; i < spectrSize; i++) {
    if (spectr[i] <= 0.9) continue;
    hzArr[idxHz++] = (binStart + i) * 7.8125;
  }
}

float compareFrequency(float* hzArr) {
  int sizeMic = 0;
  int sizeRef = 0;

  for (uint16_t j = 0; j < 512; j++) {
    if (micHz[j] == 0) sizeMic = j;
    if (refHz[j] == 0) sizeRef = j;

    if (sizeMic && sizeRef) break;
  }

  int size = sizeMic > sizeRef ? sizeMic : sizeRef;

  for (uint8_t i = 0; i < size; i++) {

  }

  memset(micHz, 0, sizeof(micHz));
  memset(refHz, 0, sizeof(refHz));

  return 0;
}

float computeDistance(double* spectr, int spectrSize) {
  float distance = 0;
  float currentBin = 0;
  for (int i = 0; i < spectrSize; i++) {
    if (spectr[i] <= 0.9) continue;
    distance += i;
  }

  return distance;
}

// Функция вычисления энергии двигателя в диапазоне 150-1600 Гц
Result computeEngineEnergy(const int16_t* samples, const int16_t* hzRange, uint8_t NUM_WINDOWS) {
  double avgSpectrum[NUM_BINS] = {0};

  for (int w = 0; w < NUM_WINDOWS; w++) {
    int start = w * FFT_SIZE; // без перекрытия
    // Подготовка данных и нормализация
    for (int i = 0; i < FFT_SIZE; i++) {
      vReal_cur[i] = (double)samples[start + i] / 32768.0; 
      vImag_cur[i] = 0.0;
    }

    // Окно Хэмминга + FFT
    FFT.windowing(vReal_cur, FFT_SIZE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
    FFT.compute(vReal_cur, vImag_cur, FFT_SIZE, FFT_FORWARD);
    FFT.complexToMagnitude(vReal_cur, vImag_cur, FFT_SIZE);

    for (int i = 0; i < NUM_BINS; i++) {
      avgSpectrum[i] += vReal_cur[i];
    }
  }

  // Делим на количество окон
  for (int i = 0; i < NUM_BINS; i++) {
    avgSpectrum[i] /= NUM_WINDOWS;
  }
 

  float hzInOneSample = SAMPLE_RATE / FFT_SIZE; // 7,8125
  float hzInOneSample256 = SAMPLE_RATE / 256; // 7,8125
  double energy = 0;

  int binStart = (hzRange[0] / hzInOneSample);
  int binEnd = (hzRange[1] / hzInOneSample);

  double *sliceSpectr = &avgSpectrum[binStart];
  int spectrEnd = binEnd;

  // noise subtraction using spectrum
  if (NUM_WINDOWS == 8) {
    int indexAverge = 0;
    for (int j = 0; j < 128; j++) {
      double averge = silenceSpectr[j] / 4;
      for (int i = 0; i < 4; i++) {
        if (avgSpectrum[indexAverge] - averge < 0) {
          avgSpectrum[indexAverge] = 0;
        } else {
          avgSpectrum[indexAverge] = avgSpectrum[indexAverge] - averge;
        }
        indexAverge++;
      }
    }
  }

  lowPassIIR(sliceSpectr, spectrEnd, 0.8);  // work
  float avergeMax = getAvergeMax(sliceSpectr, spectrEnd, 20);
  muteSmallPeaks(sliceSpectr, spectrEnd, avergeMax);
  float distance = computeDistance(sliceSpectr, spectrEnd);
  reduceTo(sliceSpectr, spectrEnd, 1);

  for (int i = binStart; i < binEnd; i++) {
    energy += avgSpectrum[i] * avgSpectrum[i];
  }

  Result result;
  result.energy = energy;
  result.distance = distance;
 // return energy; //sqrt(energy);
  return result;
}

float compareEngineSound(float &energyRef, float &energyMic) {
  float diff = fabs(energyRef - energyMic) / energyRef; // нормированная разница
  return diff;
}


float compareEngineSoundInt(float &energyRef, float &energyMic) {
  if (energyRef == 0) return 0; // защита от деления на ноль
  float diff = fabs(energyRef - energyMic) / energyRef;
  float similarity = fabs((1.0f - diff) * 100.0f);
  //float diff = fabs((energyRef - energyMic) * 100 / energyRef);
 // float similarity = (1.0f - diff) * 100.0f; // переводим в проценты
 // Serial.println(similarity);

  if (similarity > 200) similarity = 0;
  if (similarity > 100) similarity = 100 - (similarity - 100);

  return similarity;
}

float compareData(float &dataRef, float &dataMic) {
  if (dataRef == 0) return 0; // защита от деления на ноль
  float diff = fabs(dataRef - dataMic) / dataRef;
  float similarity = fabs((1.0f - diff) * 100.0f);


  if (similarity > 200) similarity = 0;
  if (similarity > 100) similarity = 100 - (similarity - 100);

  return similarity;
}

struct Similarity {
  float energy;
  float distance;
  float general;
};

Similarity compareEnergyAndDistance(Result& refED, Result& micED) {
  Similarity similarity;
  similarity.energy = compareData(refED.energy, micED.energy);
  similarity.distance = compareData(refED.distance, micED.distance);
  similarity.general = (similarity.energy + similarity.distance * 0.9) / 2;
  return similarity;
}

#define COEF_SIMILARUTY 45

// 330-1460
bool fullCompareHz() {
  static Similarity results[192] = {};

  unsigned long start = micros();
  
  uint8_t index = 0;
  bool stop = false;
  for (uint8_t k = 0; k < 4 && !stop; k++) {
    const int16_t* ref = refsAudio[k];
    const int16_t* hzRange = hzRanges[k];

    // Serial.print("\nCompare: ");
    // Serial.print(nameReference[k]);
    // Serial.println(k);

    Result resultRef = computeEngineEnergy(ref, hzRange, 4);
    
    for (uint8_t j = 0; j < CURRENT_AUDIO_SIZE / 8192; j++) {
      //const int16_t * mic = &current_audio[j * FFT_SIZE];
      const int16_t * mic = &current_audio[j * 8192];
      Result resultMic = computeEngineEnergy(mic, hzRange, 8);
      Similarity simularity = compareEnergyAndDistance(resultRef, resultMic);
      results[index] = simularity;
      index++;
    }
  }
  
  Serial.println("\n════════════════════════════════════════\n");
  uint8_t indexName = 0;
  float currentSimularuty = 0;
  float maxSimularuty = 0;
  for (uint8_t i = 0; i < index; i++) {
    if (i % 2 == 0) {
      Serial.print("════");
      Serial.print(nameReference[indexName++]);
      currentSimularuty =
          ((results[i].general + results[i + 1].general)  / 2 );// + resultsInt[i + 2]) / 3;

      if (currentSimularuty > COEF_SIMILARUTY) {
       Serial.print("   ✓ ПОХОЖ ");
      } else {
        Serial.print("   ✗ ЗВУК НЕ ПОХОЖ ");
      }

      Serial.print(currentSimularuty);
      Serial.print("%   ");
      Serial.println("════");

      if (currentSimularuty > maxSimularuty) {
        maxSimularuty = currentSimularuty;
      }
    }

    Serial.print("E: ");
    Serial.print(results[i].energy);
    Serial.print(", ");
    Serial.print("D: ");
    Serial.print(results[i].distance);
    Serial.print(", ");
    Serial.print("G: ");
    Serial.println(results[i].general);
  }

  Serial.println("════════════════════════════════════════\n");

  unsigned long elapsed = micros() - start;
  Serial.printf("Время вычисления FFT: %lu мкс (%.2f мс)\n", elapsed,
                elapsed / 1000.0);

  return maxSimularuty > COEF_SIMILARUTY;
}