#include <Arduino.h>
#include <arduinoFFT.h>


#define FFT_SIZEE 1024
#define SAMPLE_RATE 8000

ArduinoFFT<double> FFTT = ArduinoFFT<double>();


double vReall[FFT_SIZEE];
double vImagg[FFT_SIZEE];

double noiseFloor[FFT_SIZEE / 2];  // Оценка шума
bool noiseInit = false;

// Коэффициенты спектрального гейта
const float GATE_THRESHOLD = 1.7;   // 1.5–3 оптимально
const float ATTENUATION = 0.08;     // Уровень подавления (0…1)


// ------------------------------------------------------------
//                 ИНИЦИАЛИЗАЦИЯ ШУМА (шумовая пауза)
// ------------------------------------------------------------
void initNoiseEstimate(double* samples) {
  for (int i = 0; i < FFT_SIZEE; i++) {
    vReall[i] = samples[i];
    vImagg[i] = 0;
  }

  FFTT.windowing(vReall, FFT_SIZEE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
  FFTT.compute(vReall, vImagg, FFT_SIZEE, FFT_FORWARD);
  FFTT.complexToMagnitude(vReall, vImagg, FFT_SIZEE);

  for (int i = 0; i < FFT_SIZEE / 2; i++) {
    noiseFloor[i] = vReall[i];
  }
  noiseInit = true;
}


// ------------------------------------------------------------
//               SPRECTRAL GATE PROCESS
// ------------------------------------------------------------
void spectralGate(double* samples) {
  if (!noiseInit) return;  // нужно вызвать initNoiseEstimate()

  // Step 1: FFTT
  for (int i = 0; i < FFT_SIZEE; i++) {
    vReall[i] = samples[i];
    vImagg[i] = 0;
  }

  FFTT.windowing(vReall, FFT_SIZEE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
  FFTT.compute(vReall, vImagg, FFT_SIZEE, FFT_FORWARD);

  // Step 2: Magnitude
  double mag[FFT_SIZEE / 2];
  for (int i = 0; i < FFT_SIZEE / 2; i++) {
    mag[i] = sqrt(vReall[i] * vReall[i] + vImagg[i] * vImagg[i]);
  }

  // Step 3: Voice/Noise Gate
  for (int i = 0; i < FFT_SIZEE / 2; i++) {
    double threshold = noiseFloor[i] * GATE_THRESHOLD;

    if (mag[i] < threshold) {
      // подавляем шум
      vReall[i] *= ATTENUATION;
      vImagg[i] *= ATTENUATION;
      vReall[FFT_SIZEE - i - 1] *= ATTENUATION;
      vImagg[FFT_SIZEE - i - 1] *= ATTENUATION;
    } else {
      // обновление noise floor (slow update)
      noiseFloor[i] = noiseFloor[i] * 0.97 + mag[i] * 0.03;
    }
  }

  // Step 4: IFFT
  FFTT.compute(vReall, vImagg, FFT_SIZEE, FFT_REVERSE);

  // Step 5: Перезаписываем очищенный сигнал
  for (int i = 0; i < FFT_SIZEE; i++) {
    samples[i] = vReall[i] / FFT_SIZEE;
  }
}

void applySpectrumSymmetry(double* vReall, double* vImagg, int size) {
    int half = size / 2;

    // Бегаем по положительным частотам 1..half-1
    for (int k = 1; k < half; k++) {
        int mirror = size - k;

        vReall[mirror] =  vReall[k];
        vImagg[mirror] = -vImagg[k];   // комплексное сопряжение
    }

    // DC (0) и Nyquist (size/2) — всегда чисто действительные
    vImagg[0] = 0;
    vImagg[half] = 0;
}

void spectralGate(int16_t* samples, int numSamples) {
  for (int i = 0; i < FFT_SIZEE; i++) {
    vReall[i] = samples[i];
    vImagg[i] = 0;
  }

  FFTT.windowing(vReall, FFT_SIZEE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
  FFTT.compute(vReall, vImagg, FFT_SIZEE, FFT_FORWARD);

  // Step 2: Magnitude
  double mag[FFT_SIZEE / 2];
  for (int i = 0; i < FFT_SIZEE / 2; i++) {
    mag[i] = sqrt(vReall[i] * vReall[i] + vImagg[i] * vImagg[i]);
  }

  // Step 3: Voice/Noise Gate
  for (int i = 0; i < FFT_SIZEE / 2; i++) {
    double threshold = noiseFloor[i] * GATE_THRESHOLD;
    if (mag[i] < threshold) {
      // подавляем шум
      vReall[i] *= ATTENUATION;
      vImagg[i] *= ATTENUATION;
      vReall[FFT_SIZEE - i - 1] *= ATTENUATION;
      vImagg[FFT_SIZEE - i - 1] *= ATTENUATION;
    }
  }

  applySpectrumSymmetry(vReall, vImagg, FFT_SIZEE);
  FFTT.compute(vReall, vImagg, FFT_SIZEE, FFT_REVERSE);

  // Step 5: Перезаписываем очищенный сигнал
  for (int i = 0; i < FFT_SIZEE; i++) {
    samples[i] = vReall[i] / FFT_SIZEE;
  }
}

void getNoiseSpectr(const int16_t* samples) {
#define NUM_WINDOWSS 24
#define NUM_BINSS (FFT_SIZEE / 2)

  double avgSpectrum[NUM_BINSS] = {0};

  for (int w = 0; w < NUM_WINDOWSS; w++) {
    int start = w * FFT_SIZEE;  // без перекрытия
    // Подготовка данных и нормализация
    for (int i = 0; i < FFT_SIZEE; i++) {
      vReall[i] = (double)samples[start + i] / 32768.0;
      vImagg[i] = 0.0;
    }

    // Окно Хэмминга + FFT
    FFT.windowing(vReall, FFT_SIZEE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
    FFT.compute(vReall, vImagg, FFT_SIZEE, FFT_FORWARD);
    FFT.complexToMagnitude(vReall, vImagg, FFT_SIZEE);

    for (int i = 0; i < NUM_BINSS; i++) {
      avgSpectrum[i] += vReall[i];
    }
  }

  // Делим на количество окон
  for (int i = 0; i < NUM_BINSS; i++) {
    avgSpectrum[i] /= NUM_WINDOWSS;
    noiseFloor[i] = avgSpectrum[i];
  }

//   // next
//   for (int i = 0; i < FFT_SIZEE; i++) {
//     vReall[i] = samples[i];
//     vImagg[i] = 0;
//   }

//   FFTT.windowing(vReall, FFT_SIZEE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
//   FFTT.compute(vReall, vImagg, FFT_SIZEE, FFT_FORWARD);
//   FFTT.complexToMagnitude(vReall, vImagg, FFT_SIZEE);

//   for (int i = 0; i < FFT_SIZEE / 2; i++) {
//     noiseFloor[i] = vReall[i];
//   }
}