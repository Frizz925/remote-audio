#ifndef _RA_AUDIO_H
#define _RA_AUDIO_H

#include <portaudio.h>
#include <stdatomic.h>
#include <stdbool.h>

#define CHANNELS    2
#define SAMPLE_RATE 48000
#define SAMPLE_SIZE 2

#define PA_SAMPLE_FORMAT paFloat32

#define OPUS_APPLICATION    OPUS_APPLICATION_RESTRICTED_LOWDELAY
#define OPUS_FRAME_SIZE     960
#define OPUS_MAX_FRAME_SIZE 6 * OPUS_FRAME_SIZE

enum AudioDeviceType { AudioDeviceInput, AudioDeviceOutput };
typedef enum AudioDeviceType AudioDeviceType;

int Pa_Panic(const char *message, PaError err);
int opus_panic(const char *message, int err);

int audio_init();
int audio_panic(const char *message, int err);
int audio_deinit();

PaStream *audio_stream_create(const char *device_name,
                              AudioDeviceType type,
                              PaStreamCallback *callback,
                              void *userdata,
                              int *error);
#endif
