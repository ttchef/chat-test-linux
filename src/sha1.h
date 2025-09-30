// Simple SHA1 and Base64 implementation for WebSocket handshake
#ifndef SHA1_H
#define SHA1_H

#include <stdint.h>
#include <string.h>

// SHA1 implementation
static uint32_t rol(uint32_t value, uint32_t bits) {
    return (value << bits) | (value >> (32 - bits));
}

void sha1(const unsigned char *data, size_t len, unsigned char *hash) {
    uint32_t h[] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    unsigned char block[64];
    uint64_t total_len = len * 8;
    size_t i = 0;

    while (i < len) {
        size_t block_len = (len - i < 64) ? (len - i) : 64;
        memcpy(block, data + i, block_len);

        if (block_len < 64) {
            block[block_len++] = 0x80;
            while (block_len < 56) block[block_len++] = 0;
            for (int j = 0; j < 8; j++) {
                block[56 + j] = (total_len >> (56 - j * 8)) & 0xFF;
            }
        } else if (i + 64 >= len) {
            i += 64;
            continue;
        }

        uint32_t w[80];
        for (int j = 0; j < 16; j++) {
            w[j] = (block[j * 4] << 24) | (block[j * 4 + 1] << 16) |
                   (block[j * 4 + 2] << 8) | block[j * 4 + 3];
        }
        for (int j = 16; j < 80; j++) {
            w[j] = rol(w[j-3] ^ w[j-8] ^ w[j-14] ^ w[j-16], 1);
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int j = 0; j < 80; j++) {
            uint32_t f, k;
            if (j < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (j < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (j < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t temp = rol(a, 5) + f + e + k + w[j];
            e = d; d = c; c = rol(b, 30); b = a; a = temp;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
        i += 64;
    }

    for (int i = 0; i < 5; i++) {
        hash[i * 4] = (h[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;
        hash[i * 4 + 3] = h[i] & 0xFF;
    }
}

// Base64 encoding
void base64_encode(const unsigned char *input, int length, char *output) {
    const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i, j = 0;

    for (i = 0; i < length; i += 3) {
        uint32_t n = (input[i] << 16) | ((i + 1 < length ? input[i + 1] : 0) << 8) |
                     (i + 2 < length ? input[i + 2] : 0);

        output[j++] = b64[(n >> 18) & 63];
        output[j++] = b64[(n >> 12) & 63];
        output[j++] = (i + 1 < length) ? b64[(n >> 6) & 63] : '=';
        output[j++] = (i + 2 < length) ? b64[n & 63] : '=';
    }
    output[j] = '\0';
}

#endif