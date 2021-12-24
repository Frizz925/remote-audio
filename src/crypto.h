#include <sodium.h>

#define PRIVATE_KEY_SIZE crypto_aead_xchacha20poly1305_IETF_KEYBYTES
#define PUBLIC_KEY_SIZE crypto_scalarmult_curve25519_BYTES
#define SHARED_SECRET_SIZE crypto_generichash_blake2b_BYTES
#define NONCE_SIZE crypto_aead_xchacha20poly1305_IETF_NPUBBYTES

typedef enum {
    RA_SHARED_SECRET_SERVER,
    RA_SHARED_SECRET_CLIENT,
} ra_shared_secret_type;

typedef struct {
    unsigned char private[PRIVATE_KEY_SIZE];
    unsigned char public[PUBLIC_KEY_SIZE];
    size_t private_size;
    size_t public_size;
} ra_keypair_t;


int ra_crypto_init();
void ra_generate_keypair(ra_keypair_t *keypair);
int ra_compute_shared_secret(unsigned char *outkey, size_t outlen, const unsigned char *peerkey, size_t inlen,
                             const ra_keypair_t *keypair, ra_shared_secret_type type);
