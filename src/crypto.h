#ifndef _H_RA_CRYPTO
#define _H_RA_CRYPTO

#include <stdlib.h>

namespace remoteaudio {
namespace crypto {
typedef enum {
    SHARED_KEY_MODE_SERVER,
    SHARED_KEY_MODE_CLIENT,
} SharedKeyMode;

}
}  // namespace remoteaudio

#define PRIVKEY_SIZE crypto_aead_xchacha20poly1305_IETF_KEYBYTES
#define PUBKEY_SIZE crypto_scalarmult_ed25519_BYTES
#define TEMPKEY_SIZE crypto_scalarmult_ed25519_BYTES

#define PRESHARED_KEY "remoteaudiopresharedkey"

int ra_crypto_init();
int ra_crypto_compute_shared_key(const unsigned char *peerkey, unsigned char *outkey, size_t keylen, int mode);
int ra_crypto_write_public_key(unsigned char *outkey);
const char *ra_crypto_print_key(const unsigned char *key, size_t keylen);
#endif
