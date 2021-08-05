#include "noise_reduction.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <portaudio.h>

// Number of samples in one frame
#define BLOCK_SIZE 1024

// Samplerate used by portaudio
#define SAMPLE_RATE 48000.0

// Number of taps used for the Wiener filter
#define NUM_TAPS 127

// Factor by which the estimation for the input's autocorrelation is updated
// Higher values generally remove more noise but can also cause glitches in the
// audio. Lower values create less glitches, but will add a sort of 'noise reverb'
// to the audio.
#define LEARNING_FACTOR 0.3

// Number of buffers to discard at the start. This is useful
// to skip over the sound of mouse clicks or key presses when starting the 
// program.
#define NUM_FRAMES_DISCARD 10

// Number of frames to use for learning the autocorrelation of the noise.
// During this period there should be no speach signal in the input.
// The actual noise reduction starts after (NUM_FRAMES_DISCARD + NUM_FRAMES_LEARN_NOISE) frames.
#define NUM_FRAMES_LEARN_NOISE 5

struct stream_data {
    float *temp_buffer;
    float *wiener_coeffs;
    float *wiener_state;
    float *signal_corr;
    float *signal_corr_current;
    float *noise_corr;
    size_t num_taps;
    size_t buffer_size;
    size_t frame_index;
};

static float calc_mean(const float *data, size_t size) {
    float sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum += data[i];
    }
    return sum / size;
}

static int audio_callback(const void *input,
                   void *output,
                   unsigned long frame_count,
                   const PaStreamCallbackTimeInfo *time_info,
                   PaStreamCallbackFlags status_flags,
                   void *user_data)
{
    const float *input_f = input;
    float *output_f = output;
    struct stream_data *sdata = user_data;

    size_t num_taps = sdata->num_taps;

    if (sdata->frame_index >= NUM_FRAMES_DISCARD + NUM_FRAMES_LEARN_NOISE) {
        // Do Wiener filtering

        float *temp_buffer = sdata->temp_buffer;

        /*
        Make signal zero-mean. The formulas used assume a zero-mean signal.
        */
        float mean = calc_mean(input_f, frame_count);
        for (size_t i = 0; i < frame_count; i++) {
            temp_buffer[i] = input_f[i] - mean;
        }

        /*
        Do not completely override the autocorrelation but instead interpolate between the previous and current correlation.
        This will cause some noise to be present during speach, but avoids weird quacking noises when the autocorrelation
        changes rapidly (e.g. when starting to speak).
        */
        estimate_autocorrelation(sdata->signal_corr_current, sdata->num_taps, temp_buffer, frame_count);
        for (size_t i = 0; i < num_taps; i++) {
            sdata->signal_corr[i] = sdata->signal_corr[i] * (1.0 - LEARNING_FACTOR) + sdata->signal_corr_current[i] * LEARNING_FACTOR;
        }

        calc_wiener_coeffs(sdata->wiener_coeffs, sdata->signal_corr, sdata->noise_corr, sdata->num_taps);

        apply_filter(output_f, input_f, frame_count, sdata->wiener_coeffs, sdata->num_taps, sdata->wiener_state);

        /*
        Re-apply mean
        */
        for (size_t i = 0; i < frame_count; i++) {
            output_f[i] += mean;
        }
        
        sdata->frame_index++;
        return paContinue;
    } else if (sdata->frame_index >= NUM_FRAMES_DISCARD) {
        // Learn noise

        float *temp_buffer = sdata->temp_buffer;

        /*
        Make signal zero-mean
        */
        float mean = calc_mean(input_f, frame_count);
        for (size_t i = 0; i < frame_count; i++) {
            temp_buffer[i] = input_f[i] - mean;
        }

        /*
        Estimate autocorrelation. The noise autocorrelation is estimated as the mean correlation
        over a number of frames.
        */
        float *restrict signal_corr = sdata->signal_corr;
        float *restrict noise_corr = sdata->noise_corr;
        estimate_autocorrelation(signal_corr, num_taps, temp_buffer, frame_count);
        for (size_t i = 0; i < num_taps; i++) {
            noise_corr[i] += signal_corr[i] / NUM_FRAMES_LEARN_NOISE;
        }

        memcpy(output, input, frame_count * sizeof(float));
        sdata->frame_index++;
        return paContinue;
    } else {
        /*
        Discard frame. Some frames are intentionally discarded at the start (i.e. forwarded to the output without any processing).
        This avoids accidentally recording mic transients or key presses/clicks to start the program,
        which would mess up the noise correlation.
        */
        memcpy(output, input, frame_count * sizeof(float));
        sdata->frame_index++;
        return paContinue;
    }
}

static void list_available_devices() {
    printf("Available devices:\n");
    PaDeviceIndex device_count = Pa_GetDeviceCount();
    if (device_count < 0) {
        printf("Error querying devices: %s\n", Pa_GetErrorText(device_count));
    }

    for (int i = 0; i < device_count; i++) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (!info) {
            printf("  %2d: Error reading device info.\n", i);
            continue;
        }
        if (info->maxOutputChannels > 0) {
            printf("<");
        } else {
            printf(" ");
        }
        if (info->maxInputChannels > 0) {
            printf(">");
        } else {
            printf(" ");
        }
        printf(" %2d: %s (in %d, out %d)\n", i, info->name, info->maxInputChannels, info->maxOutputChannels);
    }
}

static struct stream_data *create_stream_data(size_t num_taps, size_t buffer_size) {
    struct stream_data *out = malloc(sizeof(struct stream_data));
    float *buffer = malloc(sizeof(float) * buffer_size);
    float *wiener_coeffs = malloc(sizeof(float) * num_taps);
    float *wiener_state = malloc(sizeof(float) * (num_taps - 1));
    float *signal_corr = malloc(sizeof(float) * num_taps);
    float *signal_corr_current = malloc(sizeof(float) * num_taps);
    float *noise_corr = malloc(sizeof(float) * num_taps);
    out->num_taps = num_taps;
    out->buffer_size = buffer_size;
    out->frame_index = 0;

    if (!(out && buffer && wiener_coeffs && wiener_state && signal_corr && signal_corr_current && noise_corr)) {
        free(noise_corr);
        free(signal_corr_current);
        free(signal_corr);
        free(wiener_state);
        free(buffer);
        free(out);
        return NULL;
    }

    memset(wiener_state, 0, sizeof(float) * (num_taps - 1));
    memset(signal_corr, 0, sizeof(float) * num_taps);
    memset(noise_corr, 0, sizeof(float) * num_taps);

    out->temp_buffer = buffer;
    out->wiener_coeffs = wiener_coeffs;
    out->wiener_state = wiener_state;
    out->signal_corr = signal_corr;
    out->signal_corr_current = signal_corr_current;
    out->noise_corr = noise_corr;
    return out;
}

static void free_stream_data(struct stream_data *sdata) {
    if (!sdata) {
        return;
    }

    free(sdata->noise_corr);
    free(sdata->signal_corr_current);
    free(sdata->signal_corr);
    free(sdata->wiener_state);
    free(sdata->wiener_coeffs);
    free(sdata->temp_buffer);
    free(sdata);
}

static int read_device_index_from_stdin(char *prompt) {
    printf("%s", prompt);
    int device;
    int args = scanf("%d", &device);
    if (args != 1) {
        int ch;
        do {
            ch = getchar();
        } while (ch != '\n' && ch != EOF);
        printf("Error: Please input a device index.\n");
        return -1;
    }
    if (device < 0 || device >= Pa_GetDeviceCount()) {
        printf("Error: Device %d does not exist.\n", device);
        return -1;
    }
    return device;
}

int main(int argc, char **args) {
    PaError err;

    err = Pa_Initialize();
    if (err != paNoError) {
        printf("Error initializing PortAudio: %s\n", Pa_GetErrorText(err));
        return EXIT_FAILURE;
    }

    list_available_devices();
    printf("\n");

    int in_device;
    do {
        in_device = read_device_index_from_stdin("Select input device: ");
    } while (in_device < 0);

    int out_device;
    do {
        out_device = read_device_index_from_stdin("Select output device: ");
    } while (out_device < 0);

    PaStreamParameters in_params = {
        .device = in_device,
        .channelCount = 1,
        .sampleFormat = paFloat32,
        .suggestedLatency = 0.1,
        .hostApiSpecificStreamInfo = NULL
    };

    PaStreamParameters out_params = {
        .device = out_device,
        .channelCount = 1,
        .sampleFormat = paFloat32,
        .suggestedLatency = 0.1,
        .hostApiSpecificStreamInfo = NULL
    };

    struct stream_data *sdata = create_stream_data(NUM_TAPS, BLOCK_SIZE);
    if (!sdata) {
        printf("Error initializing stream\n");
        return EXIT_FAILURE;
    }

    PaStream *stream;
    err = Pa_OpenStream(
        &stream,
        &in_params,
        &out_params,
        SAMPLE_RATE,
        BLOCK_SIZE,
        0,
        audio_callback,
        sdata
    );

    if (err != paNoError) {
        printf("Error opening stream: %s\n", Pa_GetErrorText(err));
        return EXIT_FAILURE;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        printf("Error starting stream: %s\n", Pa_GetErrorText(err));
        return EXIT_FAILURE;
    }

    printf("Press ENTER to stop processing...");
    getchar();
    getchar();

    err = Pa_Terminate();
    
    free_stream_data(sdata);

    if (err != paNoError) {
        printf("Error closing stream: %s\n", Pa_GetErrorText(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
