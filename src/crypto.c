#include "crypto.h"

#include "stdio.h"

int ra_crypto_init() {
    int err = sodium_init();
    if (err) {
        fprintf(stderr, "Failed to initialize cryptography library\n.");
    }
    return err;
}

void ra_generate_keypair(ra_keypair_t *keypair) {
    crypto_aead_xchacha20poly1305_ietf_keygen(keypair->private);
    crypto_scalarmult_curve25519_base(keypair->public, keypair->private);
}

int ra_compute_shared_secret(unsigned char *outkey, size_t outlen, const unsigned char *peerkey, size_t inlen,
                             const ra_keypair_t *keypair, ra_shared_secret_type type) {
    unsigned char tempkey[crypto_scalarmult_curve25519_BYTES];
    int err = crypto_scalarmult_curve25519(tempkey, keypair->private, peerkey);
    if (err) return err;

    crypto_generichash_blake2b_state state;
    crypto_generichash_blake2b_init(&state, NULL, 0, outlen);
    crypto_generichash_blake2b_update(&state, tempkey, sizeof(tempkey));
    if (type == RA_SHARED_SECRET_SERVER) {
        crypto_generichash_blake2b_update(&state, keypair->public, sizeof(keypair->public));
        crypto_generichash_blake2b_update(&state, peerkey, inlen);
    } else {
        crypto_generichash_blake2b_update(&state, peerkey, inlen);
        crypto_generichash_blake2b_update(&state, keypair->public, sizeof(keypair->public));
    }
    return crypto_generichash_blake2b_final(&state, outkey, outlen);
}
