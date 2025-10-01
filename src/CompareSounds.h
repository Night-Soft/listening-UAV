#ifndef COMPARE_SOUNDS_H
#define COMPARE_SOUNDS_H

#include <Arduino.h>
#include <arduinoFFT.h>


class CompareSounds {
 private:
  void computeFFTandMagnitude();
  double compareSpectra(const double* s1, const double* s2, int len);

 public:
  CompareSounds(const uint16_t * referenceData, float degreeOfSimilarity);

  boolean comapre(uint16_t & audioData1, uint16_t& audioData2);
  const uint16_t * referenceData;
  float degreeOfSimilarity;
};

#endif