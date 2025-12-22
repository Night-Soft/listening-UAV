#ifndef SPECTR_H
#define SPECTR_H
#include <Arduino.h>

double getSumMax(double* array, int arrayLength, uint8_t numberOfSlice = 3) {
  double maxs[numberOfSlice] = {0};

  for (int i = 0; i < arrayLength; i++) {
    double minValue = maxs[0];
    uint8_t minIndex = 0;

    for (int k = 0; k < numberOfSlice; k++) {  // max
      if (minValue > maxs[k]) {
        minValue = maxs[k];
        minIndex = k;
      }
    }

    if (array[i] > minValue && minIndex > -1) {
      maxs[minIndex] = array[i];
    }
  }

  double sum = 0;
  for (int i = 0; i < numberOfSlice; i++) {
    sum += maxs[i];
  }

  return sum;
}

float getAvergeMax(double* array, int arrayLength, uint8_t numberOfSlice) {
    double sumMax = getSumMax(array, arrayLength, numberOfSlice);
    return float(sumMax / numberOfSlice);
}

void muteSmallPeaks (double *spectr, int spectrSize, float avergeMax)  {
    for (int i = 0; i < spectrSize; i++) {
        if ((spectr[i] / avergeMax) > 1) continue; // всё что больше среднего оставляем
        spectr[i] = spectr[i] * (spectr[i] / avergeMax);
    }
}

double getSumBuffer(double* spectr, int spectrSize) {
  double sum = 0;
  for (int i = 0; i < spectrSize; i++) {
    sum += spectr[i];
  }
  return sum;
}

void reduceTo(double* spectr, int spectrSize, int to) {
  int sum = getSumBuffer(spectr, spectrSize);

  for (int i = 0; i < spectrSize; i++) {
    if(spectr[i] == 0) continue;
    spectr[i] = spectr[i] / sum * to;
  }
}

void lowPassIIR(double *spectr, int spectrSize, const float alpha = 0.95) {
 // static const float alpha = 0.1;  // коэффициент сглаживания (0..1)
  float filtered = 0.0f;

  for (int i = 0; i < spectrSize; i++) {
    float raw = (float)spectr[i];

    filtered = alpha * raw + (1.0f - alpha) * filtered;
    spectr[i] = filtered;
  }

}



#endif