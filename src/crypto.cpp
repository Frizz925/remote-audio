#include "crypto.h"

#include <sodium.h>
#include <string.h>

static unsigned char privkey[PRIVKEY_SIZE];
static unsigned char pubkey[PUBKEY_SIZE];

static const unsigned char *pskey = PRESHARED_KEY;

int ra_crypto_init() {
    int err = sodium_init();
    if (err != 0) return err;
    crypto_aead_xchacha20poly1305_ietf_keygen(privkey);
    crypto_scalarmult_ed25519_base(pubkey, privkey);
    return 0;
}

int ra_crypto_compute_shared_key(const unsigned char *peerkey, unsigned char *outkey, size_t keylen, int mode) {
    crypto_generichash_state state;
    unsigned char tempkey[TEMPKEY_SIZE];
    if (crypto_scalarmult_ed25519(tempkey, privkey, peerkey)) return 1;
    crypto_generichash_blake2b_init(&state, NULL, 0, keylen);
    crypto_generichash_blake2b_update(&state, tempkey, TEMPKEY_SIZE);
    if (mode == SHARED_KEY_MODE_SERVER) {
        crypto_generichash_blake2b_update(&state, pubkey, PUBKEY_SIZE);
        crypto_generichash_blake2b_update(&state, peerkey, PUBKEY_SIZE);
    } else {
        crypto_generichash_blake2b_update(&state, peerkey, PUBKEY_SIZE);
        crypto_generichash_blake2b_update(&state, pubkey, PUBKEY_SIZE);
    }
    crypto_generichash_blake2b_update(&state, pskey, strlen(pskey));
    crypto_generichash_blake2b_final(&state, outkey, keylen);
    return 0;
}

int ra_crypto_write_public_key(unsigned char *outkey) {
    memcpy(outkey, pubkey, PUBKEY_SIZE);
    return PUBKEY_SIZE;
}

const char *ra_crypto_print_key(const unsigned char *key, size_t keylen) {
    static char hex[256];
    return sodium_bin2hex(hex, sizeof(hex), key, keylen);
}
