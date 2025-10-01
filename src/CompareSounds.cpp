#include "CompareSounds.h"

#define SAMPLE_RATE 8000
#define FFT_SIZE    1024

double vReal[FFT_SIZE];
double vImag[FFT_SIZE];
double referenceSpectrum[FFT_SIZE/2];

ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, FFT_SIZE, (double)SAMPLE_RATE);

void CompareSounds::computeFFTandMagnitude() {
  FFT.windowing(vReal, FFT_SIZE, FFT_WIN_TYP_HAMMING, FFT_FORWARD);  // окно
  FFT.compute(vReal, vImag, FFT_SIZE, FFT_FORWARD);                  // FFT
  FFT.complexToMagnitude(vReal, vImag, FFT_SIZE);
}

double CompareSounds::compareSpectra(const double * s1, const double * s2, int len){
  double cross = 0, n1 = 0, n2 = 0;
  for (int i = 0; i < len; ++i) {
    cross += s1[i] * s2[i];
    n1 += s1[i] * s1[i];
    n2 += s2[i] * s2[i];
  }
  double denom = sqrt(n1) * sqrt(n2) + 1e-12;
  return cross / denom; // ~1.0 = очень похоже
}

boolean CompareSounds::comapre(uint16_t & audioData1, uint16_t & audioData2){
  return true;
}

CompareSounds::CompareSounds(const uint16_t * referenceData, float degreeOfSimilarity) {
    this->referenceData = referenceData;
    this->degreeOfSimilarity = degreeOfSimilarity;
}