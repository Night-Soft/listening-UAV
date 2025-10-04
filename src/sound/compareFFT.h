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
#define SAMPLE_SIZE 8000
#define FFT_SIZE 1024        // Должно быть степенью 2
#define NUM_BINS (FFT_SIZE / 2)

// Массивы для хранения аудио
#include "referenceAudio.h"
int16_t* reference_audio = referenceAudio;
int16_t* current_audio = nullptr;

// Массивы для FFT (double для лучшей точности)
double vReal_ref[FFT_SIZE];
double vImag_ref[FFT_SIZE];
double vReal_cur[FFT_SIZE];
double vImag_cur[FFT_SIZE];

// Спектры (магнитуда после FFT)
double reference_spectrum[NUM_BINS];
double current_spectrum[NUM_BINS];

// Создание объекта FFT
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal_ref, vImag_ref, FFT_SIZE, SAMPLE_RATE);

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

void computeSpectrum(int16_t* audio, int audio_length, double* vReal, double* vImag, double* spectrum) {
    // Шаг 1: Очистка массивов
    for (int i = 0; i < FFT_SIZE; i++) {
        vImag[i] = 0.0;
    }
    
    // Шаг 2: Копирование и нормализация данных
    int samples_to_use = min(audio_length, FFT_SIZE);
    for (int i = 0; i < samples_to_use; i++) {
        vReal[i] = audio[i] / 32768.0;  // Нормализация к [-1, 1]
    }
    
    // Заполняем остальное нулями (zero-padding)
    for (int i = samples_to_use; i < FFT_SIZE; i++) {
        vReal[i] = 0.0;
    }
    
    // Шаг 3: Применение оконной функции Хэмминга
    // arduinoFFT поддерживает несколько типов окон
    ArduinoFFT<double> fft_temp = ArduinoFFT<double>(vReal, vImag, FFT_SIZE, SAMPLE_RATE);
    fft_temp.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    
    // Шаг 4: Выполнение FFT
    fft_temp.compute(FFTDirection::Forward);
    
    // Шаг 5: Вычисление магнитуды
    fft_temp.complexToMagnitude();
    
    // Шаг 6: Копирование результата в спектр
    // Используем только первую половину (до частоты Найквиста)
    for (int i = 0; i < NUM_BINS; i++) {
        spectrum[i] = vReal[i];
    }
}

// Альтернативная функция для вычисления обоих спектров
void computeBothSpectrums() {
    unsigned long start = micros();
    
    // Спектр эталона
    Serial.println("Вычисление спектра эталона...");
    computeSpectrum(reference_audio, SAMPLE_SIZE, vReal_ref, vImag_ref, reference_spectrum);
    
    // Спектр текущего сигнала
    Serial.println("Вычисление спектра текущего сигнала...");
    computeSpectrum(current_audio, SAMPLE_SIZE, vReal_cur, vImag_cur, current_spectrum);
    
    unsigned long elapsed = micros() - start;
    Serial.printf("Время вычисления FFT: %lu мкс (%.2f мс)\n", elapsed, elapsed / 1000.0);
}

// ============================================
// ПОИСК ПИКОВОЙ ЧАСТОТЫ
// ============================================

PeakInfo findPeakFrequency(double* spectrum, int start_bin, int end_bin) {
    PeakInfo peak = {0, 0, 0};
    double freq_resolution = (double)SAMPLE_RATE / FFT_SIZE;
    
    for (int i = start_bin; i < end_bin && i < NUM_BINS; i++) {
        if (spectrum[i] > peak.magnitude) {
            peak.magnitude = spectrum[i];
            peak.bin_index = i;
            peak.frequency = i * freq_resolution;
        }
    }
    
    return peak;
}

// Использование arduinoFFT для поиска доминантной частоты
PeakInfo findPeakUsingArduinoFFT(double* vReal) {
    PeakInfo peak;
    
    // arduinoFFT имеет встроенную функцию для поиска пика
    ArduinoFFT<double> fft_temp = ArduinoFFT<double>(vReal, vImag_ref, FFT_SIZE, SAMPLE_RATE);
    peak.frequency = fft_temp.majorPeak();
    
    // Находим магнитуду и индекс
    double freq_resolution = (double)SAMPLE_RATE / FFT_SIZE;
    peak.bin_index = (int)(peak.frequency / freq_resolution);
    peak.magnitude = vReal[peak.bin_index];
    
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

SpectrumAnalysis analyzeSpectrums() {
    SpectrumAnalysis result = {0};
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║   СПЕКТРАЛЬНЫЙ АНАЛИЗ (arduinoFFT)     ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    // Вычисление спектров
    computeBothSpectrums();
    
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
        computeSpectrum(reference_audio, SAMPLE_SIZE, vReal_ref, vImag_ref, reference_spectrum);
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

void compareFFT() {
    // Serial.begin(115200);
    // delay(2000);
    
    // Serial.println("\n\n");
    // Serial.println("╔════════════════════════════════════════╗");
    // Serial.println("║  СПЕКТРАЛЬНОЕ СРАВНЕНИЕ АУДИО         ║");
    // Serial.println("║  с использованием arduinoFFT          ║");
    // Serial.println("╚════════════════════════════════════════╝");
    // Serial.printf("\nCPU: ESP32 @ %d MHz\n", getCpuFrequencyMhz());
    // Serial.printf("Свободная память: %d байт (%.1f KB)\n", 
    //               ESP.getFreeHeap(), ESP.getFreeHeap()/1024.0);
    
    // // Генерация тестовых данных
    // Serial.println("\nГенерация тестовых сигналов...");
    // Serial.println("Эталон: Двигатель 100 Hz + гармоники");
    // Serial.println("Текущий: Похожий сигнал с вариациями\n");
    
    // for (int i = 0; i < SAMPLE_SIZE; i++) {
    //     double t = i / (double)SAMPLE_RATE;
        
    //     // Эталонный сигнал: двигатель на 100 Hz
    //     double ref_signal = 
    //         sin(2 * PI * 100 * t) * 10000 +      // Основная частота
    //         sin(2 * PI * 200 * t) * 5000 +       // 2-я гармоника
    //         sin(2 * PI * 300 * t) * 2500 +       // 3-я гармоника
    //         sin(2 * PI * 400 * t) * 1000 +       // 4-я гармоника
    //         (random(-300, 300));                  // Шум
        
    //     // Текущий сигнал: немного выше обороты + больше шума
    //     double cur_signal = 
    //         sin(2 * PI * 105 * t) * 9500 +       // Чуть выше частота
    //         sin(2 * PI * 210 * t) * 4800 +       
    //         sin(2 * PI * 315 * t) * 2400 +       
    //         sin(2 * PI * 420 * t) * 950 +        
    //         (random(-400, 400));                  // Больше шума
        
    //     reference_audio[i] = (int16_t)ref_signal;
    //     current_audio[i] = (int16_t)cur_signal;
    // }
    
    // delay(1000);
    
    // Запуск бенчмарка
    benchmarkFFT();
    
    // Выполнение анализа
    SpectrumAnalysis result = analyzeSpectrums();
    
    // Визуализация спектров
   // printSpectrumGraph(reference_spectrum, "ЭТАЛОННЫЙ СПЕКТР");
    //printSpectrumGraph(current_spectrum, "ТЕКУЩИЙ СПЕКТР");
    
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
