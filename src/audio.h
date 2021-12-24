#ifndef _RA_AUDIO_H
#define _RA_AUDIO_H

#include <portaudio.h>
#include <opus/opus.h>

#include "stream.h"

#define SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER 960
#define CHANNELS 2
#define APPLICATION OPUS_APPLICATION_RESTRICTED_LOWDELAY

#define PA_SAMPLE_TYPE paFloat32;
typedef float SAMPLE;

int ra_audio_init();
void ra_audio_deinit();

#endif
