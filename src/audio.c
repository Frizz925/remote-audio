#include "audio.h"

#include <stdio.h>

#include "string.h"

const PaSampleFormat ra_prioritized_sample_formats[] = {
    paFloat32,
    paInt16,
    0,
};

const int ra_prioritized_sample_rates[] = {
    48000, 24000, 16000, 12000, 8000, 0,
};

static void print_pa_error(const char *cause, int err) {
    fprintf(stderr, "%s: (%d) %s\n", cause, err, Pa_GetErrorText(err));
}

static void init_stream_params(ra_audio_config_t *cfg, const PaDeviceInfo *info, PaStreamParameters *params) {
    memset(params, 0, sizeof(PaStreamParameters));
    params->device = cfg->device;
    params->channelCount = cfg->channel_count;
    params->suggestedLatency =
        cfg->type == RA_AUDIO_DEVICE_INPUT ? info->defaultLowInputLatency : info->defaultLowOutputLatency;
}

static void copy_stream_params(PaStreamParameters *dst, const PaStreamParameters *src) {
    memcpy(dst, src, sizeof(PaStreamParameters));
}

static void assign_stream_params(ra_audio_device_type type, PaStreamParameters *params, PaStreamParameters **inparams,
                                 PaStreamParameters **outparams) {
    if (type == RA_AUDIO_DEVICE_INPUT) {
        *inparams = params;
        *outparams = NULL;
    } else {
        *outparams = params;
        *inparams = NULL;
    }
}

static int find_sample_rate(ra_audio_device_type type, const PaStreamParameters *params, int *err) {
    PaStreamParameters temp;
    copy_stream_params(&temp, params);
    PaStreamParameters *inparams, *outparams;
    assign_stream_params(type, &temp, &inparams, &outparams);
    for (const int *rate_ptr = ra_prioritized_sample_rates; *rate_ptr != 0; rate_ptr++) {
        *err = Pa_IsFormatSupported(inparams, outparams, *rate_ptr);
        if (*err == paFormatIsSupported) return *rate_ptr;
    }
    return 0;
}

static PaSampleFormat find_sample_format(ra_audio_device_type type, const PaDeviceInfo *info,
                                         const PaStreamParameters *params, int *err) {
    PaStreamParameters temp;
    copy_stream_params(&temp, params);
    PaStreamParameters *inparams, *outparams;
    assign_stream_params(type, &temp, &inparams, &outparams);
    for (const PaSampleFormat *fmt_ptr = ra_prioritized_sample_formats; *fmt_ptr != 0; fmt_ptr++) {
        temp.sampleFormat = *fmt_ptr;
        *err = Pa_IsFormatSupported(inparams, outparams, info->defaultSampleRate);
        if (*err == paFormatIsSupported) return *fmt_ptr;
    }
    return 0;
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

const char *ra_audio_sample_format_str(PaSampleFormat fmt) {
    switch (fmt) {
    case paFloat32:
        return "32-bit float";
    case paInt32:
        return "32-bit signed integer";
    case paInt24:
        return "24-bit signed integer";
    case paInt16:
        return "16-bit signed integer";
    case paInt8:
        return "8-bit signed integer";
    case paUInt8:
        return "8-bit unsigned integer";
    default:
        return "Unknown";
    }
}

const char *ra_audio_device_type_str(ra_audio_device_type type) {
    return type == RA_AUDIO_DEVICE_INPUT ? "input" : "output";
}

PaStream *ra_audio_create_stream(ra_audio_config_t *cfg, PaStreamCallback *callback, void *userdata) {
    int err = paNoError;
    const char *devtype = ra_audio_device_type_str(cfg->type);
    const PaDeviceInfo *info = Pa_GetDeviceInfo(cfg->device);
    if (!info) {
        fprintf(stderr, "Couldn't get %s device info\n", devtype);
        return NULL;
    }

    PaStream *stream;
    PaStreamParameters params;
    init_stream_params(cfg, info, &params);

    if (cfg->sample_format == 0) {
        cfg->sample_format = find_sample_format(cfg->type, info, &params, &err);
        if (err != paFormatIsSupported) {
            print_pa_error("No supported sample format found for the device", err);
            return NULL;
        }
    }
    if (cfg->sample_rate <= 0) {
        cfg->sample_rate = find_sample_rate(cfg->type, &params, &err);
        if (err != paFormatIsSupported) {
            print_pa_error("No supported sample rate found for the device", err);
            return NULL;
        }
    }
    printf("Channel count: %d\n", cfg->channel_count);
    printf("Sample format: %s\n", ra_audio_sample_format_str(cfg->sample_format));
    printf("Sample rate: %d\n", cfg->sample_rate);

    PaStreamParameters *inparams, *outparams;
    assign_stream_params(cfg->type, &params, &inparams, &outparams);
    err = Pa_OpenStream(&stream, inparams, outparams, cfg->sample_rate, cfg->frame_size, 0, callback, userdata);
    if (err != paNoError) {
        print_pa_error("Failed to open stream", err);
        return NULL;
    }
    return stream;
}

PaDeviceIndex ra_audio_find_device(ra_audio_config_t *cfg, const char *dev) {
    const char *devtype = ra_audio_device_type_str(cfg->type);
    PaDeviceIndex index = cfg->device = paNoDevice;
    const PaDeviceInfo *info;

    if (dev) {
        int count = Pa_GetDeviceCount();
        for (index = 0; index < count; index++) {
            info = Pa_GetDeviceInfo(index);
            if (strncasecmp(dev, info->name, strlen(dev)))
                continue;
            else if (cfg->type == RA_AUDIO_DEVICE_INPUT && info->maxInputChannels <= 0)
                continue;
            else if (cfg->type == RA_AUDIO_DEVICE_OUTPUT && info->maxOutputChannels <= 0)
                continue;
            break;
        }
        if (index >= count) {
            fprintf(stderr, "No %s device found: %s\n", devtype, dev);
            return paNoDevice;
        }
    } else {
        index = cfg->type == RA_AUDIO_DEVICE_INPUT ? Pa_GetDefaultInputDevice() : Pa_GetDefaultOutputDevice();
        if (index == paNoDevice) {
            fprintf(stderr, "No default %s device found\n", devtype);
            return paNoDevice;
        }
    }

    cfg->device = index;
    if (cfg->type == RA_AUDIO_DEVICE_INPUT && info->maxInputChannels < cfg->channel_count)
        cfg->channel_count = info->maxInputChannels;
    else if (cfg->type == RA_AUDIO_DEVICE_OUTPUT && info->maxOutputChannels < cfg->channel_count)
        cfg->channel_count = info->maxOutputChannels;
    return index;
}

const char *ra_audio_device_name(PaDeviceIndex device) {
    const PaDeviceInfo *info = Pa_GetDeviceInfo(device);
    return info ? info->name : "Unknown";
}
