#include "audio.h"

#include <stdio.h>

#include "string.h"

const PaSampleFormat ra_prioritized_sample_formats[] = {
    paFloat32,
    paInt16,
    0,
};

const int ra_prioritized_sample_rates[] = {
    48000,
    24000,
    16000,
    12000,
    8000,
    0,
};

static ra_logger_t *g_logger;

static void print_pa_error(const char *cause, int err) {
    ra_logger_error(g_logger, "%s: (%d) %s", cause, err, Pa_GetErrorText(err));
}

static void init_stream_params(ra_audio_config_t *cfg, const PaDeviceInfo *info, PaStreamParameters *params) {
    memset(params, 0, sizeof(PaStreamParameters));
    params->device = cfg->device;
    params->channelCount = cfg->channel_count;
    params->sampleFormat = cfg->sample_format;
    params->suggestedLatency =
        cfg->type == RA_AUDIO_DEVICE_INPUT ? info->defaultLowInputLatency : info->defaultLowOutputLatency;
}

static void copy_stream_params(PaStreamParameters *dst, const PaStreamParameters *src) {
    memcpy(dst, src, sizeof(PaStreamParameters));
}

static void assign_stream_params(ra_audio_device_type type,
                                 PaStreamParameters *params,
                                 PaStreamParameters **inparams,
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

static PaSampleFormat find_sample_format(ra_audio_device_type type,
                                         const PaDeviceInfo *info,
                                         const PaStreamParameters *params,
                                         int *err) {
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

int ra_audio_init(ra_logger_t *logger) {
    g_logger = logger;
    int err = Pa_Initialize();
    if (err) print_pa_error("Failed to initialize audio library", err);
    return err;
}

void ra_audio_deinit() {
    Pa_Terminate();
}

size_t ra_audio_sample_format_size(PaSampleFormat fmt) {
    switch (fmt) {
    case paFloat32:
    case paInt32:
        return 4;
    case paInt24:
        return 3;
    case paInt16:
        return 2;
    case paInt8:
    case paUInt8:
        return 1;
    default:
        return 0;
    }
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
        ra_logger_error(g_logger, "Couldn't get %s device info", devtype);
        return NULL;
    }

    PaStream *stream;
    PaStreamParameters params;
    init_stream_params(cfg, info, &params);

    if (!cfg->sample_format) {
        cfg->sample_format = find_sample_format(cfg->type, info, &params, &err);
        if (err != paFormatIsSupported) {
            print_pa_error("No supported sample format found for the device", err);
            return NULL;
        }
        params.sampleFormat = cfg->sample_format;
    }
    if (!cfg->sample_rate) {
        cfg->sample_rate = find_sample_rate(cfg->type, &params, &err);
        if (err != paFormatIsSupported) {
            print_pa_error("No supported sample rate found for the device", err);
            return NULL;
        }
    }
    cfg->sample_size = ra_audio_sample_format_size(cfg->sample_format);

    ra_logger_info(g_logger, "Channel count: %d", cfg->channel_count);
    ra_logger_info(g_logger, "Sample format: %s", ra_audio_sample_format_str(cfg->sample_format));
    ra_logger_info(g_logger, "Sample rate: %d", cfg->sample_rate);

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
            ra_logger_error(g_logger, "No %s device found: %s", devtype, dev);
            return paNoDevice;
        }
    } else {
        index = cfg->type == RA_AUDIO_DEVICE_INPUT ? Pa_GetDefaultInputDevice() : Pa_GetDefaultOutputDevice();
        if (index == paNoDevice) {
            ra_logger_error(g_logger, "No default %s device found.", devtype);
            return paNoDevice;
        }
        info = Pa_GetDeviceInfo(index);
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
