#ifndef _RA_AUDIO_H
#define _RA_AUDIO_H

#include <opus/opus.h>
#include <portaudio.h>

#include "stream.h"

#define SAMPLE_RATE 48000
#define SAMPLE_SIZE 4
#define FRAMES_PER_BUFFER 960
#define CHANNELS 2
#define APPLICATION OPUS_APPLICATION_RESTRICTED_LOWDELAY

#define DECODE_BUFFER_SIZE CHANNELS * 5760
#define ENCODE_BUFFER_SIZE DECODE_BUFFER_SIZE
#define RING_BUFFER_SIZE 2 * SAMPLE_SIZE *DECODE_BUFFER_SIZE

#define PA_SAMPLE_TYPE paFloat32;
typedef float SAMPLE;

typedef enum {
    RA_AUDIO_DEVICE_OUTPUT,
    RA_AUDIO_DEVICE_INPUT,
} ra_audio_device_type;

int ra_audio_init();
void ra_audio_deinit();

const char *ra_audio_device_type_str(ra_audio_device_type type);
PaStream *ra_audio_create_stream(ra_audio_device_type type, PaDeviceIndex index, PaStreamCallback *callback, void *userdata);
PaDeviceIndex ra_audio_find_device(ra_audio_device_type type, const char *dev);
const char *ra_audio_device_name(PaDeviceIndex device);

#endif
