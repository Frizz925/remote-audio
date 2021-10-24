#include "audio.h"

#include <opus/opus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AudioStream {
    PaStreamParameters params;
    PaStream *stream;
};

static char *audio_err_message = NULL;

static atomic_bool pa_initialized = false;

static bool strcaseprefix(const char *str, const char *substr) {
    return !strncasecmp(str, substr, strlen(substr));
}

int Pa_Panic(const char *message, PaError err) {
    fprintf(stderr, "[PortAudio] %s: %s\n", message, Pa_GetErrorText(err));
    return EXIT_FAILURE;
}

int opus_panic(const char *message, int err) {
    fprintf(stderr, "[opus] %s: %s\n", message, opus_strerror(err));
    return EXIT_FAILURE;
}

int audio_init() {
    PaError err = Pa_Initialize();
    if (err) return err;
    pa_initialized = true;
    return 0;
}

int audio_panic(const char *message, int err) {
    char buf[512];
    if (audio_err_message)
        sprintf("%s %s", message, audio_err_message);
    else
        strcpy(buf, message);
    return Pa_Panic(buf, err);
}

int audio_deinit() {
    PaError err = Pa_Initialize();
    if (err) return err;
    pa_initialized = true;
    return 0;
}

AudioStream *audio_stream_create(const char *device_name,
                                 AudioDeviceType type,
                                 AudioStreamCallback *callback,
                                 void *userdata,
                                 int *error) {
    PaDeviceIndex device = paNoDevice;
    const PaDeviceInfo *device_info = NULL;
    PaStreamParameters *in_params = NULL;
    PaStreamParameters *out_params = NULL;
    PaStream *stream = NULL;
    PaError pa_err;

    if (!device_name) {
        device = (type == AudioDeviceInput) ? Pa_GetDefaultInputDevice() : Pa_GetDefaultOutputDevice();
    } else {
        PaDeviceIndex idx;
        const PaDeviceInfo *di;
        for (idx = 0; idx < Pa_GetDeviceCount(); idx++) {
            di = Pa_GetDeviceInfo(idx);
            if (!strcaseprefix(di->name, device_name)) continue;
            if (type == AudioDeviceInput && di->maxInputChannels < CHANNELS)
                continue;
            else if (type == AudioDeviceOutput && di->maxOutputChannels < CHANNELS)
                continue;
            device = idx;
            break;
        }
    }
    if (device < 0) {
        *error = device;
        return NULL;
    }
    device_info = Pa_GetDeviceInfo(device);

    PaStreamParameters params = {0};
    params.device = device;
    params.channelCount = CHANNELS;
    params.sampleFormat = PA_SAMPLE_FORMAT;
    if (type == AudioDeviceInput) {
        params.suggestedLatency = device_info->defaultLowInputLatency;
        in_params = &params;
    } else {
        params.suggestedLatency = device_info->defaultLowOutputLatency;
        out_params = &params;
    }

    pa_err = Pa_OpenStream(&stream, in_params, out_params, SAMPLE_RATE, OPUS_FRAME_SIZE, paClipOff, callback, userdata);
    if (pa_err != paNoError) {
        *error = pa_err;
        return NULL;
    }
    return stream;
}

int audio_stream_start(AudioStream *as) {
    return Pa_StartStream(as);
}

int audio_stream_active(AudioStream *as) {
    return Pa_IsStreamActive(as);
}

int audio_stream_stop(AudioStream *as) {
    return Pa_StopStream(as);
}

int audio_stream_destroy(AudioStream *as) {
    return Pa_CloseStream(as);
}
