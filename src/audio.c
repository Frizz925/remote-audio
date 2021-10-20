#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int min_int(int a, int b) {
    return (a < b) ? a : b;
}

struct AudioCtx *audio_context_create(const char *device_name, enum AudioDeviceType type) {
    int rc, err;
    struct SoundIo *soundio;
    struct SoundIoDevice *device;
    struct SoundIoChannelLayout layout;
    enum SoundIoFormat fmt = AUDIO_FORMAT;
    int ar = AUDIO_SAMPLE_RATE, ac = AUDIO_CHANNELS;

    soundio = soundio_create();
    if (!soundio) {
        fprintf(stderr, "soundio_create: Out of memory\n");
        return NULL;
    }

    if ((err = soundio_connect(soundio))) {
        fprintf(stderr, "soundio_connect: %s\n", soundio_strerror(err));
        return NULL;
    }
    soundio_flush_events(soundio);

    if (device_name) {
        device = NULL;
        int device_count = type ? soundio_output_device_count(soundio) : soundio_input_device_count(soundio);
        for (int i = 0; i < device_count; i++) {
            device = type ? soundio_get_output_device(soundio, i) : soundio_get_input_device(soundio, i);
            if (!strncasecmp(device_name, device->name, strlen(device_name))) {
                break;
            }
            soundio_device_unref(device);
            device = NULL;
        }
    } else {
        int idx = type ? soundio_default_output_device_index(soundio) : soundio_default_input_device_index(soundio);
        device = type ? soundio_get_output_device(soundio, idx) : soundio_get_input_device(soundio, idx);
    }
    if (!device) {
        fprintf(stderr, "Device not found: %s\n", device_name);
        return NULL;
    }
    if (!soundio_device_supports_format(device, fmt)) {
        fprintf(stderr, "Device format unsupported: %s\n", soundio_format_string(fmt));
        return NULL;
    }
    if (!soundio_device_supports_sample_rate(device, ar)) {
        fprintf(stderr, "Device sample rate unsupported: %d\n", ar);
        return NULL;
    }
    fprintf(stderr, "Selected device: %s\n", device->name);

    struct SoundIoChannelLayout *layout_ptr = NULL;
    for (int i = 0; i < device->layout_count; i++) {
        layout_ptr = &device->layouts[i];
        if (layout_ptr->channel_count == ac) {
            break;
        }
        layout_ptr = NULL;
    }
    if (!layout_ptr) {
        fprintf(stderr, "Device layouts unsupported\n");
        return NULL;
    }
    layout = *layout_ptr;

    struct AudioCtx *ctx = (struct AudioCtx *)malloc(sizeof(struct AudioCtx));
    ctx->soundio = soundio;
    ctx->device = device;
    ctx->layout = layout;
    ctx->format = fmt;
    ctx->sample_rate = ar;
    return ctx;
}

void audio_context_destroy(struct AudioCtx *ctx) {
    if (ctx->device) soundio_device_unref(ctx->device);
    if (ctx->soundio) soundio_destroy(ctx->soundio);
    free(ctx);
}