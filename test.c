#include <assert.h>
#include <libgen.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "proto.h"

#define KEYSIZE 32

typedef unsigned long long cryptolen_t;

static int test_proto() {
    int type, flags;
    char buf[512];

    assert(proto_write_header(buf, PacketTypeHandshake, PacketFlagSYN) >= 0);
    assert(proto_read_header(buf, &type, &flags) >= 0);
    assert(type == PacketTypeHandshake);
    assert(flags == PacketFlagSYN);

    unsigned char key[KEYSIZE];
    unsigned char nonce[KEYSIZE];
    randombytes_buf(key, KEYSIZE);
    randombytes_buf(nonce, KEYSIZE);

    unsigned char *readkey = NULL;
    keylen_t keylen = sizeof(key);
    assert(proto_write_handshake_init(buf, key, keylen) > 0);
    assert(proto_read_handshake_init(buf, (void **)&readkey, &keylen) > 0);
    assert(keylen == KEYSIZE);
    assert(memcmp(key, readkey, keylen) == 0);

    return EXIT_SUCCESS;
}

static int test_crypto() {
    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialize sodium\n");
        return EXIT_FAILURE;
    }
    const char *expected = "Hello, world!";
    const char *actual = NULL;

    const unsigned char *msg = (unsigned char *)expected;
    cryptolen_t msglen = strlen(expected);

    unsigned char enc[512], dec[64];
    cryptolen_t enclen = sizeof(enc);
    cryptolen_t declen = sizeof(dec);

    unsigned char ssk[crypto_aead_xchacha20poly1305_IETF_KEYBYTES];
    unsigned char csk[crypto_aead_xchacha20poly1305_IETF_KEYBYTES];
    unsigned char spk[crypto_scalarmult_BYTES];
    unsigned char cpk[crypto_scalarmult_BYTES];

    unsigned char server_shared[crypto_scalarmult_BYTES];
    unsigned char client_shared[crypto_scalarmult_BYTES];

    unsigned char nonce[crypto_aead_xchacha20poly1305_IETF_NPUBBYTES];
    unsigned char shared_kdf[crypto_generichash_BYTES];

    crypto_aead_xchacha20poly1305_ietf_keygen(ssk);
    crypto_aead_xchacha20poly1305_ietf_keygen(csk);
    crypto_scalarmult_base(spk, ssk);
    crypto_scalarmult_base(cpk, csk);

    if (crypto_scalarmult(server_shared, ssk, cpk) < 0) {
        fprintf(stderr, "Server key exchange failed\n");
        return EXIT_FAILURE;
    }
    if (crypto_scalarmult(client_shared, csk, spk) < 0) {
        fprintf(stderr, "Client key exchange failed\n");
        return EXIT_FAILURE;
    }
    assert(memcmp(server_shared, client_shared, sizeof(server_shared)) == 0);

    crypto_generichash_state state;
    crypto_generichash_init(&state, server_shared, sizeof(server_shared), sizeof(shared_kdf));
    crypto_generichash_update(&state, ssk, sizeof(ssk));
    crypto_generichash_update(&state, csk, sizeof(csk));
    crypto_generichash_final(&state, shared_kdf, sizeof(shared_kdf));

    randombytes_buf(nonce, sizeof(nonce));

    int encres =
        crypto_aead_xchacha20poly1305_ietf_encrypt(enc, &enclen, msg, msglen, NULL, 0, NULL, nonce, shared_kdf);
    assert(encres >= 0);
    int decres =
        crypto_aead_xchacha20poly1305_ietf_decrypt(dec, &declen, NULL, enc, enclen, NULL, 0, nonce, shared_kdf);
    assert(decres >= 0);

    actual = (char *)dec;
    assert(strcmp(actual, expected) == 0);

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <test-case>\n", dirname(argv[0]));
        return EXIT_FAILURE;
    }
    const char *test_name = argv[1];
    if (!strcmp(test_name, "proto")) {
        return test_proto();
    } else if (!strcmp(test_name, "crypto")) {
        return test_crypto();
    }
    fprintf(stderr, "Unknown test case: %s\n", test_name);
    return EXIT_FAILURE;
}
