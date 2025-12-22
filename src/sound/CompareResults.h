#ifndef COMPARE_RESULTS_H
#define COMPARE_RESULTS_H

#include <Arduino.h>

struct CompareResult {
    bool isUav = false;
    String json = "";
};

extern CompareResult *compareResult;   // extern — только объявление
 
#endif