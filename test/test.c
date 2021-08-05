#include "noise_reduction.h"
#include "quickcheck4c.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#define TEST_MAX_NUM_TAPS 25
#define ERROR_MARGIN 0.1

static QCC_GenValue *gen_test_array() {
    return QCC_genArrayFloatLR(TEST_MAX_NUM_TAPS, -1.0, 1.0);
}

static float vecdot_with_toeplitz_row(float *row, float *vec, size_t row_index, size_t row_length) {
    float out = 0.0;
    for (size_t i = 0; i < row_length; i++) {
        out += row[abs(i - row_index)] * vec[i];
    }
    return out;
}

QCC_TestStatus wiener_coeffs_solve_toeplitz_matrix(QCC_GenValue **vals, int len, QCC_Stamp **stamps) {
    float *signal_corr = QCC_getValue(vals, 0, float *);
    float *noise_corr = QCC_getValue(vals, 1, float *);
    size_t num_taps = vals[0]->n < vals[1]->n ? vals[0]->n: vals[1]->n;

    float *coeffs = malloc(sizeof(float) * num_taps);
    calc_wiener_coeffs(coeffs, signal_corr, noise_corr, num_taps);

    bool valid = true;
    for (size_t i = 0; i < num_taps; i++) {
        float expected = signal_corr[i] - noise_corr[i];
        float actual = vecdot_with_toeplitz_row(signal_corr, coeffs, i, num_taps);
        valid = valid && (fabsf(actual - expected) / actual < ERROR_MARGIN);
        if (!valid) {
            printf("actual: %f, expected: %f, numtaps: %d\n", actual, expected, (int) num_taps);
            break;
        }
    }

    free(coeffs);

    if (valid) {
        return QCC_OK;
    } else {
        return QCC_FAIL;
    }
}

int main(int argc, char **args) {
    QCC_init(0);

    printf("Testing calc_wiener_coeffs\n");
    QCC_testForAll(100, 1, wiener_coeffs_solve_toeplitz_matrix, 2, gen_test_array, gen_test_array);

    return 0;
}
