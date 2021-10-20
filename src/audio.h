#include <soundio/soundio.h>

#define AUDIO_FORMAT      SoundIoFormatFloat32LE
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS    2

struct AudioCtx {
    struct SoundIo *soundio;
    struct SoundIoDevice *device;
    struct SoundIoChannelLayout layout;
    enum SoundIoFormat format;
    int sample_rate;
};

int min_int(int a, int b);