#include <Arduino.h>
#include <arduinoFFT.h>

// Параметры аудио
#define SAMPLE_RATE 8000

#define CURRENT_AUDIO_SIZE 16384  // 8192 // 1024*8 : 24576 = 3s, 12288 = 1.536s, 16384 = 2.048s

#define REFERENCE_SIZE 4096

#define FFT_SIZE 1024  // Должно быть степенью 2
#define NUM_BINS (FFT_SIZE / 2)

#define COEF_SIMILARUTY 47

#include "reference/awayRef17_10_25.h"
#include "reference/midddleRef117_10_25.h"
#include "reference/midddleRef217_10_25.h"
#include "reference/startRef217_10_25.h"

const char* nameReference[4] = {"start", "midddle",
                                "midddle2", "awayRef"};

int16_t* current_audio = nullptr;

struct SpectrumInfo {
  float energy;
  float distance;
  uint8_t* frequency;
};

struct Similarity {
  float energy;
  float distance;
  float frequency;
  float general;
};

double vReal[FFT_SIZE];
double vImag[FFT_SIZE];

// Спектры (магнитуда после FFT)
double reference_spectrum[NUM_BINS];
double current_spectrum[NUM_BINS];

// ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal_ref, vImag_ref, FFT_SIZE,
// SAMPLE_RATE);
ArduinoFFT<double> FFT = ArduinoFFT<double>();

void createSpecRef(const int16_t* samples) {
  for (int i = 0; i < FFT_SIZE; i++) {
    vReal[i] = samples[i] / 32768.0;  // Нормализация к [-1, 1]
    vImag[i] = 0;
  }

  FFT.windowing(vReal, FFT_SIZE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
  FFT.compute(vReal, vImag, FFT_SIZE, FFT_FORWARD);
  FFT.complexToMagnitude(vReal, vImag, FFT_SIZE);
}

double getCompresedSpectrr(const int16_t* samples, int size) {
  Serial.println("Inside getCompresedSpectrr");
  double avgSpectrum[NUM_BINS] = {0};
  for (int i = 0; i < size / FFT_SIZE; i++) {
    createSpecRef(&samples[i * FFT_SIZE]);
    // Serial.println("createSpecRef");

    for (int i = 0; i < NUM_BINS; i++) {
      avgSpectrum[i] = avgSpectrum[i] + vReal[i];
    }
  }

  float hzInOneSample = -7.8125;  // SAMPLE_RATE / FFT_SIZE;  // 7,8125

  for (int i = 0; i < NUM_BINS; i++) {
    avgSpectrum[i] = avgSpectrum[i] / 4;
    Serial.print("hz: ");
    Serial.print(hzInOneSample += 7.8125);
    Serial.print(" | ");
    Serial.println(avgSpectrum[i]);
  }

  return *avgSpectrum;
}

// double getCompresedSpectr() {
//   const int16_t* ref = refsAudio[0];
//    getCompresedSpectrr(&ref[1], 4096);
//    return 0.1;
// }

void createSpecMic(const int16_t* samples) {
  for (int i = 0; i < FFT_SIZE; i++) {
    vReal[i] = samples[i] / 32768.0;  // Нормализация к [-1, 1]
    vImag[i] = 0;
  }

  FFT.windowing(vReal, FFT_SIZE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
  FFT.compute(vReal, vImag, FFT_SIZE, FFT_FORWARD);
  FFT.complexToMagnitude(vReal, vImag, FFT_SIZE);
}

void printResult(Similarity* results, uint8_t size) {
  Serial.println("\n════════════════════════════════════════\n");
  uint8_t indexName = 0;
  float currentSimularuty = 0;
  float maxSimularuty = 0;

  for (uint8_t i = 0; i < size; i++) {
    if (true) {//(i % 2 == 0)
      Serial.print("════");
      Serial.print(nameReference[indexName++]);
      //currentSimularuty = ((results[i].general + results[i + 1].general) /
     //                      2);  // + resultsInt[i + 2]) / 3;
      currentSimularuty = (results[i].general); 
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
    Serial.print("F: ");
    Serial.print(results[i].frequency);
    Serial.print(", ");
    Serial.print("G: ");
    Serial.println(results[i].general);
  }

  Serial.println("════════════════════════════════════════\n");
}

void addToJSON(String &str, const String &field, const String &value, bool isEnd = false) {
  str += "\"";
  str += field;
  str += "\"";
  str += ":";
  str += value;
  if(!isEnd) str += ",";
}

String resultsToJSON(Similarity* results, uint8_t size) {
  // String json = "{\"temp\":25, \"humidity\":60}";
  // String json = "{\"\":25, \"humidity\":60}";
  String json = "{\n";
  addToJSON(json, String("COEF"), String(COEF_SIMILARUTY));

  uint8_t indexName = 0;
  float currentSimularuty = 0;
  float maxSimularuty = 0;

  for (uint8_t i = 0; i < size; i++) {
    addToJSON(json, String(nameReference[indexName]), String("{\n"), true);

    if (true) {
      currentSimularuty = results[i].general;
      if (currentSimularuty > maxSimularuty) {
        maxSimularuty = currentSimularuty;
      }
      indexName++;
    }

    addToJSON(json, String("energy"), String(results[i].energy, 3));
    addToJSON(json, String("distance"), String(results[i].distance, 3));
    addToJSON(json, String("frequency"), String(results[i].frequency, 3));
    addToJSON(json, String("general"), String(results[i].general, 3), true);

    json += "\n},\n";
  }

  addToJSON(json, String("similarity"), String(maxSimularuty, 3), true);
  json += "\n}";

  return json;
}

// #define NUM_WINDOWS 4
#define SAMPLING_FREQ 8000
#include <cmath>  // Required for std::isnan()
#include <iostream>

#include "reference/SilenceSpectr.h"
#include "sound/spectr.h"

uint8_t *micHz;
uint8_t *refHz;
// uint8_t micHz[NUM_BINS] = {0};
// uint8_t refHz[NUM_BINS] = {0};
void computeFrequency(uint8_t* hzArr, double* spectr, int binStart,
                      int binEnd) {
  for (int i = 0; i <= binEnd; i++) {
    if (spectr[i] <= 0.9) continue;
    hzArr[i + binStart] = 1;
  }
}

void clearHzArr() {
  static bool isCreated = false;
  if(isCreated == false) {
    isCreated = true;
    micHz = (uint8_t*)malloc(sizeof(uint8_t) * NUM_BINS);
    refHz = (uint8_t*)malloc(sizeof(uint8_t) * NUM_BINS);
  }
  memset(micHz, 0, sizeof(uint8_t) * NUM_BINS);
  memset(refHz, 0, sizeof(uint8_t) * NUM_BINS);
}

float compareFrequency(int binStart, int binEnd) {
  int size = binEnd - binStart;
  float similarity = 0;

  for (int i = binStart; i <= binEnd; i++) {
    similarity += float(micHz[i] == refHz[i]);
  }

  return similarity / size * 100;
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
SpectrumInfo computeSpectrumInfo(const int16_t* samples, const int16_t* hzRange,
                                 uint8_t NUM_WINDOWS) {
  double avgSpectrum[NUM_BINS] = {0};

  for (int w = 0; w < NUM_WINDOWS; w++) {
    int start = w * FFT_SIZE;  // без перекрытия
    // Подготовка данных и нормализация
    for (int i = 0; i < FFT_SIZE; i++) {
      vReal[i] = (double)samples[start + i] / 32768.0;
      vImag[i] = 0.0;
    }

    // Окно Хэмминга + FFT
    FFT.windowing(vReal, FFT_SIZE, FFT_WIN_TYP_BLACKMAN, FFT_FORWARD);
    FFT.compute(vReal, vImag, FFT_SIZE, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, FFT_SIZE);

    for (int i = 0; i < NUM_BINS; i++) {
      avgSpectrum[i] += vReal[i];
    }
  }

  // Делим на количество окон
  for (int i = 0; i < NUM_BINS; i++) {
    avgSpectrum[i] /= NUM_WINDOWS;
  }

  float hzInOneSample = SAMPLE_RATE / FFT_SIZE;  // 7,8125
  float hzInOneSample256 = SAMPLE_RATE / 256;    // 7,8125
  double energy = 0;

  int binStart = (hzRange[0] / hzInOneSample);
  int binEnd = (hzRange[1] / hzInOneSample);

  double* sliceSpectr = &avgSpectrum[binStart];
  int spectrEnd = binEnd;

  // noise subtraction using spectrum
  if (NUM_WINDOWS == 8) {
    for (int i = binStart; i < binEnd; i++) {
      double averge = silenceSpectr[i];
      if (avgSpectrum[i] - averge < 0) {
        avgSpectrum[i] = 0;
      } else {
        avgSpectrum[i] = avgSpectrum[i] - averge;
      }
    }
  }

  lowPassIIR(sliceSpectr, spectrEnd, 0.8);  // work
  float avergeMax = getAvergeMax(sliceSpectr, spectrEnd, 20);
  muteSmallPeaks(sliceSpectr, spectrEnd, avergeMax);

  float distance = computeDistance(sliceSpectr, spectrEnd);

  uint8_t* hzData = nullptr;
  if (NUM_WINDOWS == 8) {
    hzData = micHz;
  } else {
    hzData = refHz;
  }

  computeFrequency(hzData, sliceSpectr, binStart, binEnd);
  reduceTo(sliceSpectr, spectrEnd, 1);

  for (int i = binStart; i < binEnd; i++) {
    energy += avgSpectrum[i] * avgSpectrum[i];
  }

  SpectrumInfo result;
  result.energy = energy;
  result.distance = distance;
  result.frequency = hzData;

  return result;
}

float compareEngineSound(float& energyRef, float& energyMic) {
  float diff =
      fabs(energyRef - energyMic) / energyRef;  // нормированная разница
  return diff;
}

float compareEngineSoundInt(float& energyRef, float& energyMic) {
  if (energyRef == 0) return 0;  // защита от деления на ноль
  float diff = fabs(energyRef - energyMic) / energyRef;
  float similarity = fabs((1.0f - diff) * 100.0f);
  // float diff = fabs((energyRef - energyMic) * 100 / energyRef);
  // float similarity = (1.0f - diff) * 100.0f; // переводим в проценты
  // Serial.println(similarity);

  if (similarity > 200) similarity = 0;
  if (similarity > 100) similarity = 100 - (similarity - 100);

  return similarity;
}

float compareData(float& dataRef, float& dataMic) {
  if (dataRef == 0) return 0;  // защита от деления на ноль
  float diff = fabs(dataRef - dataMic) / dataRef;
  float similarity = fabs((1.0f - diff) * 100.0f);

  if (similarity > 200) similarity = 0;
  if (similarity > 100) similarity = 100 - (similarity - 100);

  return similarity;
}

Similarity getSimilarity(SpectrumInfo& refED, SpectrumInfo& micED,
                         const int16_t* hzRange) {
  float hzInOneSample = SAMPLE_RATE / FFT_SIZE;  // 7,8125

  int binStart = (hzRange[0] / hzInOneSample);
  int binEnd = (hzRange[1] / hzInOneSample);

  Similarity similarity;

  // below returned data with percent similarity
  similarity.energy = compareData(refED.energy, micED.energy);
  similarity.distance = compareData(refED.distance, micED.distance);
  similarity.frequency = compareFrequency(binStart, binEnd);
  // similarity.general =
  //     (similarity.energy * 0.7 + similarity.frequency * 0.9 +
  //     similarity.distance * 0.9) / 3;
  float res100 = (similarity.energy + similarity.distance) / 2;
  similarity.general =
      (similarity.energy * 0.7 + similarity.distance * 1.3) / 2;
  return similarity;
}

#include "sound/CompareResults.h"
// 330-1460
bool fullCompareHz() {
  const int16_t* refsAudio[4] = {startRef217_10_25, midddleRef117_10_25,
                                 midddleRef217_10_25, awayRef17_10_25};
  const int16_t* hzRanges[4] = {
      hzRange_startRef217_10_25,
      hzRange_midddleRef117_10_25,
      hzRange_midddleRef217_10_25,
      hzRange_awayRef17_10_25,
  };

  Similarity results[8] = {};

  unsigned long start = micros();

  uint8_t size = 0;
  bool stop = false;
  for (uint8_t k = 0; k < 4 && !stop; k++) {
    const int16_t* ref = refsAudio[k];
    const int16_t* hzRange = hzRanges[k];

    // Serial.print("\nCompare: ");
    // Serial.print(nameReference[k]);
    // Serial.println(k);
    clearHzArr();
    SpectrumInfo resultRef = computeSpectrumInfo(ref, hzRange, 4);

    for (uint8_t j = 0; j < 1; j++) {  // CURRENT_AUDIO_SIZE / 8192;
      // const int16_t * mic = &current_audio[j * FFT_SIZE];
      const int16_t* mic = &current_audio[j * 8192];
      SpectrumInfo resultMic = computeSpectrumInfo(mic, hzRange, 8);
      Similarity simularity = getSimilarity(resultRef, resultMic, hzRange);
      results[size] = simularity;
      size++;
    }
  }

  float currentSimularuty = 0;
  float maxSimularuty = 0;
  for (uint8_t i = 0; i < size; i++) { //start midddle midddle2 awayRef
      currentSimularuty = results[i].general;//((results[i].general + results[i + 1].general) / 2);
      if (currentSimularuty > maxSimularuty) {
        maxSimularuty = currentSimularuty;
      }
  }

  printResult(results, size);

  compareResult->isUav = maxSimularuty > COEF_SIMILARUTY;
  compareResult->json = resultsToJSON(results, size);


  unsigned long elapsed = micros() - start;
  Serial.printf("Время вычисления FFT: %lu мкс (%.2f мс)\n", elapsed,
                elapsed / 1000.0);


  return maxSimularuty > COEF_SIMILARUTY;
}