#include <soundio/soundio.h>

#define AUDIO_FORMAT      SoundIoFormatFloat32LE
#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_CHANNELS    2

enum AudioDeviceType { AUDIO_DEVICE_INPUT, AUDIO_DEVICE_OUTPUT };

struct AudioCtx {
    struct SoundIo *soundio;
    struct SoundIoDevice *device;
    struct SoundIoChannelLayout layout;
    enum SoundIoFormat format;
    int sample_rate;
};

int min_int(int a, int b);

struct AudioCtx *audio_context_create(const char *device_name, enum AudioDeviceType type);
void audio_context_destroy(struct AudioCtx *ctx);