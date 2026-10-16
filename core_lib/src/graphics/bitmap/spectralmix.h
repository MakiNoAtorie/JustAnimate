/*
Kubelka-Munk pigment mixing, ported from spectral.js (MIT, see spectralmix.cpp).
*/
#ifndef SPECTRALMIX_H
#define SPECTRALMIX_H

#include <array>

/** Mixes two sRGB colours (channels 0..1, not premultiplied) as paint. Each weight is the amount
 *  of that paint; blue and yellow give green. Pure and thread-safe. */
std::array<float, 3> spectralMix(const std::array<float, 3>& a, float weightA, const std::array<float, 3>& b, float weightB);

#endif // SPECTRALMIX_H