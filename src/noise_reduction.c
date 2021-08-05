#include "noise_reduction.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

void estimate_autocorrelation(float *restrict out, size_t max_lag, const float *restrict data, size_t size) {
    // Sum values
    for (size_t i = 0; i < size - max_lag; i++) {
        for (size_t lag = 0; lag < max_lag; lag++) {
            out[lag] += data[i] * data[i + lag];
        }
    }

    // Normalize values
    float norm_factor = 1.0 / (size - max_lag);
    for (size_t i = 0; i < max_lag; i++) {
        out[i] *= norm_factor;
    }
}

/**
 * Helper function to compute a vector dot product between a vector and a row of a
 * symmetric Toeplitz matrix. This function is used in the Levinson algorithm
 * to calculate the Wiener filter coefficients.
 * 
 * This function does not take an actual matrix as a parameter but instead uses the autocorrelation
 * to form the requested row of the matrix.
 * 
 * @param autocorr Pointer to the autocorrelation data from which the Toeplitz matrix is constructed.
 * @param vec Pointer to the vector.
 * @param row_index Row of the matrix to use for the dot product.
 * @param row_length Length of the matrix row. This is used to simulate a smaller matrix which is required
 *                   during the Levinson algorithm
 * @returns The dot product between the matrix row and the vector.
 */ 
static inline float vecdot_with_toeplitz_row(const float *autocorr,
                                             const float *vec,
                                             size_t row_index,
                                             size_t row_length)
{
    float out = 0.0;
    for (size_t i = 0; i < row_length; i++) {
        out += autocorr[abs(i - row_index)] * vec[i];
    }
    
    return out;
}

void calc_wiener_coeffs(float *coeffs, const float *signal_corr, const float *noise_corr, size_t num_taps) {
    if (num_taps == 0) {
        return;
    }

    static float *vec = NULL;
    static size_t vec_size = 0;

    if (num_taps > vec_size) {
        free(vec);
        vec = malloc(sizeof(float) * num_taps);
        vec_size = num_taps;
    }

    vec[0] = 1.0 / signal_corr[0];
    coeffs[0] = (signal_corr[0] - noise_corr[0]) / signal_corr[0];

    for (size_t i = 1; i < num_taps; i++) {
        vec[i] = 0;

        float forward_error = vecdot_with_toeplitz_row(signal_corr, vec, i, i + 1);
        // For symmetric toeplitz matrices, the backward error is the same as the forward error.
        float error_mult = forward_error * forward_error;

        // Compute the new backwards vector in-place.
        size_t half_size = i / 2 + 1;
        for (size_t j = 0; j < half_size; j++) {
            float val_upper = 1.0f / (1.0f - error_mult) * vec[    j] - forward_error / (1.0f - error_mult) * vec[i - j];
            float val_lower = 1.0f / (1.0f - error_mult) * vec[i - j] - forward_error / (1.0f - error_mult) * vec[    j];
            vec[j] = val_upper;
            vec[i - j] = val_lower;
        }

        coeffs[i] = 0;
        float y_error = vecdot_with_toeplitz_row(signal_corr, coeffs, i, i + 1);
        for (size_t j = 0; j <= i; j++) {
            coeffs[j] += (signal_corr[i] - noise_corr[i] - y_error) * vec[i - j];
        }
    }
}

void apply_filter(float *out,
                  const float *data,
                  size_t size, 
                  const float *coeffs, 
                  size_t num_taps, 
                  float *state)
{
    // Part where both the input data and the delay line are used.
    for (size_t i = 0; i < num_taps - 1; i++) {
        float y = 0.0;

        // First, process the input data part.
        for (size_t j = 0; j < i + 1; j++) {
            y += data[i - j] * coeffs[j];
        }

        // Then add the contents of the delay line.
        for (size_t j = i + 1; j < num_taps; j++) {
            y += state[i - j + num_taps - 1] * coeffs[j];
        }

        out[i] = y;
    }

    // Part where only the input data is used.
    for (size_t i = num_taps - 1; i < size; i++) {
        float y = 0.0;
        for (size_t j = 0; j < num_taps; j++) {
            y += data[i - j] * coeffs[j];
        }
        out[i] = y;
    }

    // Write the new contents of the delay line to state.
    for (size_t i = 0; i < num_taps - 1; i++) {
        state[i] = data[size - num_taps - 1 + i];
    }
}
