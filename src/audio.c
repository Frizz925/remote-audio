#include "audio.h"

#include <stdio.h>
#include <string.h>

static void print_pa_error(const char *cause, int err) {
    fprintf(stderr, "%s: (%d) %s\n", cause, err, Pa_GetErrorText(err));
}

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

PaStream *ra_audio_create_stream(const char *dev, ra_find_device_type type, PaStreamCallback *callback,
                                 void *userdata) {
    PaStreamParameters params = {0};
    params.device = ra_audio_find_device(dev, type);
    if (params.device == paNoDevice) {
        return NULL;
    }
    const PaDeviceInfo *info = Pa_GetDeviceInfo(params.device);
    if (type == RA_AUDIO_DEVICE_INPUT)
        printf("Using device input for source: %s\n", info->name);
    else
        printf("Using device output for sink: %s\n", info->name);
    params.channelCount = CHANNELS;
    params.sampleFormat = PA_SAMPLE_TYPE;
    params.suggestedLatency =
        type == RA_AUDIO_DEVICE_INPUT ? info->defaultLowInputLatency : info->defaultLowOutputLatency;

    PaStream *stream;
    PaStreamParameters *inparams = type == RA_AUDIO_DEVICE_INPUT ? &params : NULL;
    PaStreamParameters *outparams = type == RA_AUDIO_DEVICE_OUTPUT ? &params : NULL;
    int err = Pa_OpenStream(&stream, inparams, outparams, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, callback, userdata);
    if (err != paNoError) {
        print_pa_error("Failed to open stream", err);
        return NULL;
    }
    return stream;
}

PaDeviceIndex ra_audio_find_device(const char *dev, ra_find_device_type type) {
    const char *devtype = type == RA_AUDIO_DEVICE_INPUT ? "input" : "output";
    if (!dev) {
        PaDeviceIndex index = type == RA_AUDIO_DEVICE_INPUT ? Pa_GetDefaultInputDevice() : Pa_GetDefaultOutputDevice();
        if (index != paNoDevice) return index;
        fprintf(stderr, "No default %s device found\n", devtype);
        return paNoDevice;
    }

    PaDeviceIndex index;
    const PaDeviceInfo *info;
    int count = Pa_GetDeviceCount();
    for (index = 0; index < count; index++) {
        info = Pa_GetDeviceInfo(index);
        if (strncasecmp(dev, info->name, strlen(dev)))
            continue;
        else if (type == RA_AUDIO_DEVICE_INPUT && info->maxInputChannels < CHANNELS)
            continue;
        else if (type == RA_AUDIO_DEVICE_OUTPUT && info->maxOutputChannels < CHANNELS)
            continue;
        return index;
    }
    fprintf(stderr, "No %s device found: %s\n", devtype, dev);
    return paNoDevice;
}
