#ifndef _RA_AUDIO_H
#define _RA_AUDIO_H

#include <opus/opus.h>
#include <portaudio.h>

#include "types.h"
#include "logger.h"

#define MAX_CHANNELS 2
#define MAX_SAMPLE_SIZE 4
#define FRAMES_PER_BUFFER 960
#define OPUS_APPLICATION OPUS_APPLICATION_AUDIO

#define DECODE_BUFFER_SIZE 5760 * MAX_CHANNELS * MAX_SAMPLE_SIZE
#define ENCODE_BUFFER_SIZE DECODE_BUFFER_SIZE
#define RING_BUFFER_SIZE 8 * DECODE_BUFFER_SIZE

#define ENCODE_HIGH_BANDWIDTH

typedef enum {
    RA_AUDIO_DEVICE_OUTPUT,
    RA_AUDIO_DEVICE_INPUT,
} ra_audio_device_type;

typedef struct {
    ra_audio_device_type type;
    PaDeviceIndex device;
    PaSampleFormat sample_format;
    int channel_count;
    int sample_rate;
    int frame_size;
    size_t sample_size;
} ra_audio_config_t;

int ra_audio_init(ra_logger_t *logger);
void ra_audio_deinit();

size_t ra_audio_sample_format_size(PaSampleFormat fmt);
const char *ra_audio_sample_format_str(PaSampleFormat fmt);
const char *ra_audio_device_type_str(ra_audio_device_type type);
PaStream *ra_audio_create_stream(ra_audio_config_t *cfg, PaStreamCallback *callback, void *userdata);
PaDeviceIndex ra_audio_find_device(ra_audio_config_t *cfg, const char *dev);
const char *ra_audio_device_name(PaDeviceIndex device);

#endif
