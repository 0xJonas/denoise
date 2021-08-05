#ifndef NOISE_REDUCTION_H
#define NOISE_REDUCTION_H

#include <stdlib.h>

/**
 * Estimates the autocorrelation of the input data, assuming a uniform distribution of values.
 * This function uses the following formula:
 * 
 *   r(d) = sum(x[n] * [n + d]) / (N - D)
 * 
 * @param out Pointer to which the output autocorrelation is written.
 * @param max_lag Size of out. This is the maximum offset at which the autocorrelation is calculated.
 * @param data Pointer to the input data.
 * @param size Size of the input data.
 */ 
void estimate_autocorrelation(float *restrict out, size_t max_lag, const float *restrict data, size_t size);

/**
 * Calculates the coefficients for a Wiener filter using the Levinson algorithm.
 * 
 * @param coeffs Pointer at which to store the output coefficients.
 * @param autocorr Pointer to the autocorrelation of the signal.
 * @param noise_corr Pointer to the autocorrelation of just the noise signal.
 * @param num_taps Number of coefficients to generate. coeffs, autocorr and noise_corr must all have at least num_taps values.
 */ 
void calc_wiener_coeffs(float *coeffs, const float *autocorr, const float *noise_corr, size_t num_taps);

/**
 * Applies an FIR filter to a signal.
 * 
 * @param out Pointer to the output signal.
 * @param data Pointer to the input data.
 * @param size Size of the input data.
 * @param coeffs Pointer to the filter coefficients.
 * @param num_taps Number of filter coefficients.
 * @param state State of the delay line of the FIR filter. This will also be filled with the new state after filtering.
 */ 
void apply_filter(float *out,
                  const float *data,
                  size_t size, 
                  const float *coeffs, 
                  size_t num_taps, 
                  float *state);

#endif
