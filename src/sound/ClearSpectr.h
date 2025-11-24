#include <Arduino.h>
#include <arduinoFFT.h>


#define FFT_SIZEE 256
#define SAMPLE_RATE 8000

ArduinoFFT<double> FFTT = ArduinoFFT<double>();


double vReal[FFT_SIZEE];
double vImag[FFT_SIZEE];

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
    vReal[i] = samples[i];
    vImag[i] = 0;
  }

  FFTT.windowing(vReal, FFT_SIZEE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
  FFTT.compute(vReal, vImag, FFT_SIZEE, FFT_FORWARD);
  FFTT.complexToMagnitude(vReal, vImag, FFT_SIZEE);

  for (int i = 0; i < FFT_SIZEE / 2; i++) {
    noiseFloor[i] = vReal[i];
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
    vReal[i] = samples[i];
    vImag[i] = 0;
  }

  FFTT.windowing(vReal, FFT_SIZEE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
  FFTT.compute(vReal, vImag, FFT_SIZEE, FFT_FORWARD);

  // Step 2: Magnitude
  double mag[FFT_SIZEE / 2];
  for (int i = 0; i < FFT_SIZEE / 2; i++) {
    mag[i] = sqrt(vReal[i] * vReal[i] + vImag[i] * vImag[i]);
  }

  // Step 3: Voice/Noise Gate
  for (int i = 0; i < FFT_SIZEE / 2; i++) {
    double threshold = noiseFloor[i] * GATE_THRESHOLD;

    if (mag[i] < threshold) {
      // подавляем шум
      vReal[i] *= ATTENUATION;
      vImag[i] *= ATTENUATION;
      vReal[FFT_SIZEE - i - 1] *= ATTENUATION;
      vImag[FFT_SIZEE - i - 1] *= ATTENUATION;
    } else {
      // обновление noise floor (slow update)
      noiseFloor[i] = noiseFloor[i] * 0.97 + mag[i] * 0.03;
    }
  }

  // Step 4: IFFT
  FFTT.compute(vReal, vImag, FFT_SIZEE, FFT_REVERSE);

  // Step 5: Перезаписываем очищенный сигнал
  for (int i = 0; i < FFT_SIZEE; i++) {
    samples[i] = vReal[i] / FFT_SIZEE;
  }
}

void applySpectrumSymmetry(double* vReal, double* vImag, int size) {
    int half = size / 2;

    // Бегаем по положительным частотам 1..half-1
    for (int k = 1; k < half; k++) {
        int mirror = size - k;

        vReal[mirror] =  vReal[k];
        vImag[mirror] = -vImag[k];   // комплексное сопряжение
    }

    // DC (0) и Nyquist (size/2) — всегда чисто действительные
    vImag[0] = 0;
    vImag[half] = 0;
}

void spectralGate(int16_t* samples, int numSamples) {
  for (int i = 0; i < FFT_SIZEE; i++) {
    vReal[i] = samples[i];
    vImag[i] = 0;
  }

  FFTT.windowing(vReal, FFT_SIZEE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
  FFTT.compute(vReal, vImag, FFT_SIZEE, FFT_FORWARD);

  // Step 2: Magnitude
  double mag[FFT_SIZEE / 2];
  for (int i = 0; i < FFT_SIZEE / 2; i++) {
    mag[i] = sqrt(vReal[i] * vReal[i] + vImag[i] * vImag[i]);
  }

  // Step 3: Voice/Noise Gate
  for (int i = 0; i < FFT_SIZEE / 2; i++) {
    double threshold = noiseFloor[i] * GATE_THRESHOLD;
    if (mag[i] < threshold) {
      // подавляем шум
      vReal[i] *= ATTENUATION;
      vImag[i] *= ATTENUATION;
      vReal[FFT_SIZEE - i - 1] *= ATTENUATION;
      vImag[FFT_SIZEE - i - 1] *= ATTENUATION;
    }
  }

  applySpectrumSymmetry(vReal, vImag, FFT_SIZEE);
  FFTT.compute(vReal, vImag, FFT_SIZEE, FFT_REVERSE);

  // Step 5: Перезаписываем очищенный сигнал
  for (int i = 0; i < FFT_SIZEE; i++) {
    samples[i] = vReal[i] / FFT_SIZEE;
  }
}

void getNoiseSpectr(const int16_t* samples) {
#define NUM_WINDOWSS 94
#define NUM_BINSS (FFT_SIZEE / 2)

  double avgSpectrum[NUM_BINSS] = {0};

  for (int w = 0; w < NUM_WINDOWSS; w++) {
    int start = w * FFT_SIZEE;  // без перекрытия
    // Подготовка данных и нормализация
    for (int i = 0; i < FFT_SIZEE; i++) {
      vReal[i] = (double)samples[start + i] / 32768.0;
      vImag[i] = 0.0;
    }

    // Окно Хэмминга + FFT
    FFT.windowing(vReal, FFT_SIZEE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
    FFT.compute(vReal, vImag, FFT_SIZEE, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, FFT_SIZEE);

    for (int i = 0; i < NUM_BINSS; i++) {
      avgSpectrum[i] += vReal[i];
    }
  }

  // Делим на количество окон
  for (int i = 0; i < NUM_BINSS; i++) {
    avgSpectrum[i] /= NUM_WINDOWSS;
    noiseFloor[i] = avgSpectrum[i];
  }

//   // next
//   for (int i = 0; i < FFT_SIZEE; i++) {
//     vReal[i] = samples[i];
//     vImag[i] = 0;
//   }

//   FFTT.windowing(vReal, FFT_SIZEE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
//   FFTT.compute(vReal, vImag, FFT_SIZEE, FFT_FORWARD);
//   FFTT.complexToMagnitude(vReal, vImag, FFT_SIZEE);

//   for (int i = 0; i < FFT_SIZEE / 2; i++) {
//     noiseFloor[i] = vReal[i];
//   }
}