#pragma once

#include "ofApp.h"

unsigned int trailing_zeros(unsigned int n);
bool IsPowerOfTwo(unsigned long x);
void inPlaceFastHaarWaveletTransform(std::vector<double> &sample);
void inPlaceFastHaarWaveletTransform_nSweeps(std::vector<double> &sample, int size, int nSweeps);
void inPlaceFastInverseHaarWaveletTransform(std::vector<double> &sample);
void IFHWT();

void displayOrderedFreqsFromInPlaceHaar(std::vector<double> &in_place_sample);
void drawTransform(std::vector<double> &transform);
double hanning(long windowLength, long windowPos);