#include "audio.h"

#include <stdio.h>

int ra_audio_init() {
    int err = Pa_Initialize();
    if (err) {
        fprintf(stderr, "Failed to initialize audio library: ");
        fprintf(stderr, "(%d) %s\n", err, Pa_GetErrorText(err));
    }
    return err;
}

void ra_audio_deinit() {
    Pa_Terminate();
}
