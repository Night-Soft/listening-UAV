#include <Arduino.h>
#include <math.h>

// Для ESP32 можно использовать встроенную библиотеку DSP
// или простую реализацию FFT

#define SAMPLE_RATE 8000
#define SAMPLE_SIZE 8000
#define FFT_SIZE 1024        // Размер FFT (должен быть степенью 2)
#define NUM_BINS 512         // FFT_SIZE / 2 (полезные частоты)

// Массивы для хранения аудио
// int16_t reference_audio[SAMPLE_SIZE];
// int16_t current_audio[SAMPLE_SIZE];
#include "referenceAudio.h"

int16_t* reference_audio = referenceAudio;
int16_t* current_audio = nullptr;

// Массивы для FFT (комплексные числа)
float fft_real[FFT_SIZE];
float fft_imag[FFT_SIZE];

// Спектры (магнитуда)
float reference_spectrum[NUM_BINS];
float current_spectrum[NUM_BINS];

// ============================================
// ПРОСТАЯ РЕАЛИЗАЦИЯ FFT (Cooley-Tukey)
// ============================================

// Проверка, является ли число степенью двойки
bool isPowerOfTwo(int n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

// Битовая инверсия для FFT
int reverseBits(int num, int bits) {
    int reversed = 0;
    for (int i = 0; i < bits; i++) {
        reversed = (reversed << 1) | (num & 1);
        num >>= 1;
    }
    return reversed;
}

// Быстрое преобразование Фурье (FFT)
void fft(float* real, float* imag, int n) {
    if (!isPowerOfTwo(n)) {
        Serial.println("Ошибка: размер FFT должен быть степенью 2!");
        return;
    }
    
    int bits = 0;
    int temp = n;
    while (temp > 1) {
        bits++;
        temp >>= 1;
    }
    
    // Битовая инверсия (bit-reversal)
    for (int i = 0; i < n; i++) {
        int j = reverseBits(i, bits);
        if (j > i) {
            float temp_real = real[i];
            float temp_imag = imag[i];
            real[i] = real[j];
            imag[i] = imag[j];
            real[j] = temp_real;
            imag[j] = temp_imag;
        }
    }
    
    // Алгоритм Кули-Тьюки
    for (int stage = 1; stage <= bits; stage++) {
        int m = 1 << stage;  // 2^stage
        int m2 = m >> 1;     // m/2
        
        float omega_real = cos(-2.0 * PI / m);
        float omega_imag = sin(-2.0 * PI / m);
        
        for (int k = 0; k < n; k += m) {
            float w_real = 1.0;
            float w_imag = 0.0;
            
            for (int j = 0; j < m2; j++) {
                int t_idx = k + j + m2;
                int u_idx = k + j;
                
                float t_real = w_real * real[t_idx] - w_imag * imag[t_idx];
                float t_imag = w_real * imag[t_idx] + w_imag * real[t_idx];
                
                real[t_idx] = real[u_idx] - t_real;
                imag[t_idx] = imag[u_idx] - t_imag;
                real[u_idx] = real[u_idx] + t_real;
                imag[u_idx] = imag[u_idx] + t_imag;
                
                float temp = w_real;
                w_real = w_real * omega_real - w_imag * omega_imag;
                w_imag = temp * omega_imag + w_imag * omega_real;
            }
        }
    }
}

// ============================================
// ОКОННЫЕ ФУНКЦИИ
// ============================================

// Окно Хэмминга (сглаживает края сигнала)
void applyHammingWindow(float* data, int length) {
    for (int i = 0; i < length; i++) {
        float window = 0.54 - 0.46 * cos(2.0 * PI * i / (length - 1));
        data[i] *= window;
    }
}

// Окно Ханна
void applyHannWindow(float* data, int length) {
    for (int i = 0; i < length; i++) {
        float window = 0.5 * (1.0 - cos(2.0 * PI * i / (length - 1)));
        data[i] *= window;
    }
}

// ============================================
// ВЫЧИСЛЕНИЕ СПЕКТРА
// ============================================

// Подготовка данных и вычисление FFT
void computeSpectrum(int16_t* audio, int audio_length, float* spectrum, int spectrum_length) {
    // Очистка буферов
    memset(fft_real, 0, sizeof(fft_real));
    memset(fft_imag, 0, sizeof(fft_imag));
    
    // Копирование и нормализация данных
    int samples_to_use = min(audio_length, FFT_SIZE);
    for (int i = 0; i < samples_to_use; i++) {
        fft_real[i] = audio[i] / 32768.0f;  // Нормализация к [-1, 1]
    }
    
    // Применение оконной функции
    applyHammingWindow(fft_real, FFT_SIZE);
    
    // Выполнение FFT
    fft(fft_real, fft_imag, FFT_SIZE);
    
    // Вычисление магнитуды (амплитуды) для каждой частоты
    for (int i = 0; i < spectrum_length; i++) {
        // Магнитуда = sqrt(real^2 + imag^2)
        spectrum[i] = sqrt(fft_real[i] * fft_real[i] + fft_imag[i] * fft_imag[i]);
        
        // Преобразование в децибелы (опционально)
        // spectrum[i] = 20 * log10(spectrum[i] + 1e-10);
    }
}

// ============================================
// МЕТОДЫ СРАВНЕНИЯ СПЕКТРОВ
// ============================================

// 1. Евклидово расстояние между спектрами
float spectrumEuclideanDistance(float* spec1, float* spec2, int length) {
    float sum = 0;
    for (int i = 0; i < length; i++) {
        float diff = spec1[i] - spec2[i];
        sum += diff * diff;
    }
    return sqrt(sum / length);
}

// 2. Косинусное сходство спектров
float spectrumCosineSimilarity(float* spec1, float* spec2, int length) {
    float dot_product = 0;
    float norm1 = 0;
    float norm2 = 0;
    
    for (int i = 0; i < length; i++) {
        dot_product += spec1[i] * spec2[i];
        norm1 += spec1[i] * spec1[i];
        norm2 += spec2[i] * spec2[i];
    }
    
    float denominator = sqrt(norm1 * norm2);
    if (denominator == 0) return 0;
    
    return dot_product / denominator;
}

// 3. Сравнение по частотным диапазонам (для двигателя)
struct FrequencyBands {
    float low;        // 0-200 Hz (основной тон двигателя)
    float mid_low;    // 200-500 Hz (первые гармоники)
    float mid;        // 500-1000 Hz (средние гармоники)
    float mid_high;   // 1000-2000 Hz (высокие гармоники)
    float high;       // 2000-4000 Hz (шум, свист)
};

FrequencyBands analyzeFrequencyBands(float* spectrum, int spectrum_length) {
    FrequencyBands bands = {0};
    float freq_resolution = (float)SAMPLE_RATE / FFT_SIZE;  // Гц на бин
    
    for (int i = 0; i < spectrum_length; i++) {
        float freq = i * freq_resolution;
        
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

float compareFrequencyBands(FrequencyBands bands1, FrequencyBands bands2) {
    // Нормализация
    float total1 = bands1.low + bands1.mid_low + bands1.mid + bands1.mid_high + bands1.high;
    float total2 = bands2.low + bands2.mid_low + bands2.mid + bands2.mid_high + bands2.high;
    
    if (total1 == 0 || total2 == 0) return 0;
    
    bands1.low /= total1;
    bands1.mid_low /= total1;
    bands1.mid /= total1;
    bands1.mid_high /= total1;
    bands1.high /= total1;
    
    bands2.low /= total2;
    bands2.mid_low /= total2;
    bands2.mid /= total2;
    bands2.mid_high /= total2;
    bands2.high /= total2;
    
    // Евклидово расстояние между распределениями
    float diff = 0;
    diff += pow(bands1.low - bands2.low, 2);
    diff += pow(bands1.mid_low - bands2.mid_low, 2);
    diff += pow(bands1.mid - bands2.mid, 2);
    diff += pow(bands1.mid_high - bands2.mid_high, 2);
    diff += pow(bands1.high - bands2.high, 2);
    
    // Преобразуем в меру схожести (0-1)
    return 1.0 - sqrt(diff);
}

// 4. Поиск пиковых частот (основной тон двигателя)
struct PeakFrequency {
    float frequency;  // Частота в Гц
    float magnitude;  // Амплитуда
    int bin_index;    // Индекс в спектре
};

PeakFrequency findPeakFrequency(float* spectrum, int spectrum_length, int start_bin, int end_bin) {
    PeakFrequency peak = {0, 0, 0};
    float freq_resolution = (float)SAMPLE_RATE / FFT_SIZE;
    
    for (int i = start_bin; i < end_bin && i < spectrum_length; i++) {
        if (spectrum[i] > peak.magnitude) {
            peak.magnitude = spectrum[i];
            peak.bin_index = i;
            peak.frequency = i * freq_resolution;
        }
    }
    
    return peak;
}

// Сравнение основных частот
float comparePeakFrequencies(float* spec1, float* spec2, int length) {
    // Ищем пик в диапазоне 50-500 Hz (типично для двигателей)
    int start_bin = (int)(50.0 * FFT_SIZE / SAMPLE_RATE);
    int end_bin = (int)(500.0 * FFT_SIZE / SAMPLE_RATE);
    
    PeakFrequency peak1 = findPeakFrequency(spec1, length, start_bin, end_bin);
    PeakFrequency peak2 = findPeakFrequency(spec2, length, start_bin, end_bin);
    
    // Сравниваем частоты и амплитуды
    float freq_diff = abs(peak1.frequency - peak2.frequency) / max(peak1.frequency, peak2.frequency);
    float mag_diff = abs(peak1.magnitude - peak2.magnitude) / max(peak1.magnitude, peak2.magnitude);
    
    // Комбинированная оценка
    return 1.0 - (freq_diff * 0.6 + mag_diff * 0.4);
}

// 5. Спектральный центроид (центр масс спектра)
float calculateSpectralCentroid(float* spectrum, int length) {
    float weighted_sum = 0;
    float total_sum = 0;
    float freq_resolution = (float)SAMPLE_RATE / FFT_SIZE;
    
    for (int i = 0; i < length; i++) {
        float freq = i * freq_resolution;
        weighted_sum += freq * spectrum[i];
        total_sum += spectrum[i];
    }
    
    if (total_sum == 0) return 0;
    return weighted_sum / total_sum;
}

// ============================================
// ГЛАВНАЯ ФУНКЦИЯ СРАВНЕНИЯ СПЕКТРОВ
// ============================================

struct SpectrumComparisonResult {
    float euclidean_distance;      // Евклидово расстояние
    float cosine_similarity;       // Косинусное сходство (0-1)
    float band_similarity;         // Сходство по диапазонам (0-1)
    float peak_similarity;         // Сходство пиковых частот (0-1)
    float centroid_similarity;     // Сходство центроидов (0-1)
    float overall_similarity;      // Общая оценка (0-100%)
    bool is_similar;               // Итоговый результат
    
    // Дополнительная информация
    PeakFrequency ref_peak;
    PeakFrequency cur_peak;
    FrequencyBands ref_bands;
    FrequencyBands cur_bands;
};

SpectrumComparisonResult compareSpectrums(int16_t* ref_audio, int16_t* cur_audio, int audio_length) {
    SpectrumComparisonResult result = {0};
    
    Serial.println("\n=== СПЕКТРАЛЬНЫЙ АНАЛИЗ ===");
    
    // 1. Вычисление спектров
    unsigned long start = micros();
    computeSpectrum(ref_audio, audio_length, reference_spectrum, NUM_BINS);
    computeSpectrum(cur_audio, audio_length, current_spectrum, NUM_BINS);
    unsigned long fft_time = micros() - start;
    Serial.printf("Время FFT: %lu мкс (%.2f мс)\n", fft_time, fft_time / 1000.0);
    
    // 2. Евклидово расстояние
    result.euclidean_distance = spectrumEuclideanDistance(reference_spectrum, current_spectrum, NUM_BINS);
    Serial.printf("Евклидово расстояние: %.4f\n", result.euclidean_distance);
    
    // 3. Косинусное сходство
    result.cosine_similarity = spectrumCosineSimilarity(reference_spectrum, current_spectrum, NUM_BINS);
    Serial.printf("Косинусное сходство: %.3f\n", result.cosine_similarity);
    
    // 4. Анализ частотных диапазонов
    result.ref_bands = analyzeFrequencyBands(reference_spectrum, NUM_BINS);
    result.cur_bands = analyzeFrequencyBands(current_spectrum, NUM_BINS);
    result.band_similarity = compareFrequencyBands(result.ref_bands, result.cur_bands);
    Serial.printf("Сходство диапазонов: %.3f\n", result.band_similarity);
    
    // 5. Сравнение пиковых частот
    result.peak_similarity = comparePeakFrequencies(reference_spectrum, current_spectrum, NUM_BINS);
    
    int start_bin = (int)(50.0 * FFT_SIZE / SAMPLE_RATE);
    int end_bin = (int)(500.0 * FFT_SIZE / SAMPLE_RATE);
    result.ref_peak = findPeakFrequency(reference_spectrum, NUM_BINS, start_bin, end_bin);
    result.cur_peak = findPeakFrequency(current_spectrum, NUM_BINS, start_bin, end_bin);
    
    Serial.printf("Пиковые частоты: %.1f Hz vs %.1f Hz\n", result.ref_peak.frequency, result.cur_peak.frequency);
    Serial.printf("Сходство пиков: %.3f\n", result.peak_similarity);
    
    // 6. Спектральный центроид
    float ref_centroid = calculateSpectralCentroid(reference_spectrum, NUM_BINS);
    float cur_centroid = calculateSpectralCentroid(current_spectrum, NUM_BINS);
    float centroid_diff = abs(ref_centroid - cur_centroid) / max(ref_centroid, cur_centroid);
    result.centroid_similarity = 1.0 - centroid_diff;
    Serial.printf("Центроиды: %.1f Hz vs %.1f Hz (сходство: %.3f)\n", 
                  ref_centroid, cur_centroid, result.centroid_similarity);
    
    // 7. Общая оценка (взвешенное среднее)
    result.overall_similarity = (
        result.cosine_similarity * 0.35 +      // Главный критерий
        result.band_similarity * 0.25 +        // Распределение энергии
        result.peak_similarity * 0.25 +        // Основной тон
        result.centroid_similarity * 0.15      // Общая тональность
    ) * 100.0;
    
    // 8. Определение похожести
    result.is_similar = (result.overall_similarity > 70.0) && 
                       (result.cosine_similarity > 0.7);
    
    Serial.printf("\n>>> ИТОГО: %.1f%% - %s\n", 
                  result.overall_similarity, 
                  result.is_similar ? "ПОХОЖ ✓" : "НЕ ПОХОЖ ✗");
    
    return result;
}

// ============================================
// ВИЗУАЛИЗАЦИЯ СПЕКТРА (ASCII)
// ============================================

void printSpectrum(float* spectrum, int length, const char* title) {
    Serial.printf("\n--- %s ---\n", title);
    
    int display_bins = 40;  // Количество полос для отображения
    int bins_per_display = length / display_bins;
    
    Serial.println("Freq(Hz)  |Spectrum");
    Serial.println("----------|--------------------");
    
    for (int i = 0; i < display_bins; i++) {
        float freq = (i * bins_per_display) * (float)SAMPLE_RATE / FFT_SIZE;
        
        // Усреднение по группе бинов
        float avg_magnitude = 0;
        for (int j = 0; j < bins_per_display; j++) {
            int idx = i * bins_per_display + j;
            if (idx < length) {
                avg_magnitude += spectrum[idx];
            }
        }
        avg_magnitude /= bins_per_display;
        
        // Нормализация для отображения
        int bar_length = (int)(avg_magnitude * 50);  // Масштабирование
        if (bar_length > 50) bar_length = 50;
        
        Serial.printf("%7.0f   |", freq);
        for (int k = 0; k < bar_length; k++) {
            Serial.print("█");
        }
        Serial.printf(" %.3f\n", avg_magnitude);
    }
}

// ============================================
// SETUP И LOOP
// ============================================

void compareSpectr() {
    //Serial.begin(115200);
   // delay(2000);
    
    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║  СПЕКТРАЛЬНОЕ СРАВНЕНИЕ АУДИО (FFT)       ║");
    Serial.println("╚════════════════════════════════════════════╝");
    Serial.printf("Sample Rate: %d Hz\n", SAMPLE_RATE);
    Serial.printf("FFT Size: %d\n", FFT_SIZE);
    Serial.printf("Frequency Resolution: %.2f Hz/bin\n", (float)SAMPLE_RATE / FFT_SIZE);
    Serial.printf("Max Frequency: %d Hz\n", SAMPLE_RATE / 2);
    
    // Генерация тестового сигнала (симуляция двигателя)
    Serial.println("\nГенерация тестовых данных...");
    // for (int i = 0; i < SAMPLE_SIZE; i++) {
    //     float t = i / (float)SAMPLE_RATE;
        
    //     // Эталон: двигатель на 100 Hz с гармониками
    //     float ref_signal = 
    //         sin(2 * PI * 100 * t) * 8000 +      // Основная частота
    //         sin(2 * PI * 200 * t) * 4000 +      // 2-я гармоника
    //         sin(2 * PI * 300 * t) * 2000 +      // 3-я гармоника
    //         (random(-500, 500));                // Шум
        
    //     // Текущий: похожий сигнал с небольшими изменениями
    //     float cur_signal = 
    //         sin(2 * PI * 105 * t) * 7500 +      // Немного выше обороты
    //         sin(2 * PI * 210 * t) * 3800 +      
    //         sin(2 * PI * 315 * t) * 1900 +      
    //         (random(-600, 600));
        
    //     reference_audio[i] = (int16_t)ref_signal;
    //     current_audio[i] = (int16_t)cur_signal;
    // }
    
    delay(100);
    
    // Выполнение сравнения
    SpectrumComparisonResult result = compareSpectrums(reference_audio, current_audio, SAMPLE_SIZE);
    
    // Визуализация спектров
   // printSpectrum(reference_spectrum, NUM_BINS, "ЭТАЛОННЫЙ СПЕКТР");
   // printSpectrum(current_spectrum, NUM_BINS, "ТЕКУЩИЙ СПЕКТР");
    
    Serial.println("\n✓ Анализ завершён!");
}
