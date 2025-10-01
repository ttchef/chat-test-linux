/**
 * SHA1 and Base64 Implementation for WebSocket Handshake
 *
 * This header provides simplified implementations of:
 * 1. SHA-1 hashing algorithm (used for WebSocket handshake key generation)
 * 2. Base64 encoding (used to encode the SHA-1 hash for transmission)
 *
 * These implementations are specifically designed for WebSocket handshake
 * according to RFC 6455 specification.
 */

#ifndef SHA1_H
#define SHA1_H

#include <stdint.h>  // Fixed-width integer types: uint32_t, uint64_t
#include <string.h>  // String operations: memcpy

/**
 * Rotate left (circular left shift) operation
 * Used in SHA-1 algorithm to mix bits
 *
 * @param value - 32-bit value to rotate
 * @param bits - Number of bit positions to rotate left
 * @return Rotated 32-bit value
 */
static uint32_t rol(uint32_t value, uint32_t bits) {
    // Left shift value by 'bits' positions and OR with right-shifted remainder
    // This creates a circular rotation where bits shifted out on the left wrap to the right
    // Example: rol(10110011, 2) = 11001110
    return (value << bits) | (value >> (32 - bits));
}

/**
 * Compute SHA-1 hash of input data
 * SHA-1 produces a 160-bit (20-byte) hash value
 *
 * Algorithm overview:
 * 1. Initialize five 32-bit hash values (h0-h4)
 * 2. Process data in 512-bit (64-byte) blocks
 * 3. For each block, perform 80 rounds of mixing operations
 * 4. Output is the final concatenated hash values
 *
 * @param data - Pointer to input data to hash
 * @param len - Length of input data in bytes
 * @param hash - Output buffer for 20-byte hash (must be at least 20 bytes)
 */
void sha1(const unsigned char *data, size_t len, unsigned char *hash) {
    // Initialize SHA-1 hash values (defined by the SHA-1 specification)
    // These are fractional parts of square roots of first 5 primes
    uint32_t h[] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

    // Buffer for processing one 64-byte block at a time
    unsigned char block[64];

    // Total length in bits (used in padding)
    // SHA-1 requires appending the original message length as a 64-bit value
    uint64_t total_len = len * 8;

    // Current position in the input data
    size_t i = 0;

    // Process data in 64-byte blocks
    while (i < len) {
        // Calculate how many bytes to read for this block
        // Last block may be less than 64 bytes
        size_t block_len = (len - i < 64) ? (len - i) : 64;

        // Copy data into block buffer
        memcpy(block, data + i, block_len);

        // If this is the last block and it's not full, add padding
        if (block_len < 64) {
            // Append a single '1' bit (0x80 = 10000000 in binary)
            block[block_len++] = 0x80;

            // Fill with zeros until we have room for the 8-byte length
            // We need 8 bytes at the end for the original length
            while (block_len < 56) block[block_len++] = 0;

            // Append the original message length in bits (big-endian 64-bit)
            for (int j = 0; j < 8; j++) {
                // Extract each byte from the 64-bit length value
                // Shift right by (56 - j*8) bits and mask to get one byte
                block[56 + j] = (total_len >> (56 - j * 8)) & 0xFF;
            }
        }
        // If current block is full but we're at the end, we need another padding block
        else if (i + 64 >= len) {
            // Skip processing this block and continue to create padding block
            i += 64;
            continue;
        }

        // Expand the 16-word (64-byte) block into 80 words for processing
        uint32_t w[80];

        // First 16 words are directly copied from the block (in big-endian format)
        for (int j = 0; j < 16; j++) {
            // Combine 4 bytes into a 32-bit word (big-endian byte order)
            w[j] = (block[j * 4] << 24) |      // Most significant byte
                   (block[j * 4 + 1] << 16) |  // Second byte
                   (block[j * 4 + 2] << 8) |   // Third byte
                   block[j * 4 + 3];           // Least significant byte
        }

        // Remaining 64 words are derived using XOR and rotation
        // This expands the 16 input words into 80 words for the compression function
        for (int j = 16; j < 80; j++) {
            // XOR four previous words at specific offsets
            // Then rotate left by 1 bit for diffusion
            w[j] = rol(w[j-3] ^ w[j-8] ^ w[j-14] ^ w[j-16], 1);
        }

        // Initialize working variables with current hash values
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];

        // Main compression loop: 80 rounds
        for (int j = 0; j < 80; j++) {
            uint32_t f, k;

            // SHA-1 uses different functions and constants for different rounds
            // Rounds 0-19: f = (b AND c) OR ((NOT b) AND d)
            if (j < 20) {
                f = (b & c) | ((~b) & d);  // Conditional function
                k = 0x5A827999;             // Round constant
            }
            // Rounds 20-39: f = b XOR c XOR d
            else if (j < 40) {
                f = b ^ c ^ d;              // Parity function
                k = 0x6ED9EBA1;             // Round constant
            }
            // Rounds 40-59: f = (b AND c) OR (b AND d) OR (c AND d)
            else if (j < 60) {
                f = (b & c) | (b & d) | (c & d);  // Majority function
                k = 0x8F1BBCDC;                    // Round constant
            }
            // Rounds 60-79: f = b XOR c XOR d
            else {
                f = b ^ c ^ d;              // Parity function (same as rounds 20-39)
                k = 0xCA62C1D6;             // Round constant
            }

            // Compute new value for 'a' by combining:
            // - Rotated 'a' value (by 5 bits)
            // - Function output 'f'
            // - Variable 'e'
            // - Round constant 'k'
            // - Current expanded word w[j]
            uint32_t temp = rol(a, 5) + f + e + k + w[j];

            // Shift variables: e=d, d=c, c=rotated(b), b=a, a=temp
            // This creates a cascade effect mixing the bits
            e = d;              // Old d becomes new e
            d = c;              // Old c becomes new d
            c = rol(b, 30);     // Old b rotated becomes new c
            b = a;              // Old a becomes new b
            a = temp;           // Computed temp becomes new a
        }

        // Add the compressed chunk to the current hash values
        // This is the core update step of SHA-1
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;

        // Move to the next 64-byte block
        i += 64;
    }

    // Convert the five 32-bit hash values into a 20-byte output array
    // Store in big-endian format (most significant byte first)
    for (int i = 0; i < 5; i++) {
        hash[i * 4] = (h[i] >> 24) & 0xFF;      // Most significant byte
        hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;  // Second byte
        hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;   // Third byte
        hash[i * 4 + 3] = h[i] & 0xFF;          // Least significant byte
    }
}

/**
 * Encode binary data to Base64 string
 * Base64 encoding converts binary data to ASCII text using 64 printable characters
 *
 * Algorithm:
 * 1. Process input in 3-byte groups (24 bits)
 * 2. Split each group into four 6-bit values
 * 3. Map each 6-bit value to a Base64 character (A-Z, a-z, 0-9, +, /)
 * 4. Pad with '=' if input length is not a multiple of 3
 *
 * @param input - Pointer to binary data to encode
 * @param length - Length of input data in bytes
 * @param output - Output buffer for null-terminated Base64 string
 *                 (must be at least ((length + 2) / 3) * 4 + 1 bytes)
 */
void base64_encode(const unsigned char *input, int length, char *output) {
    // Base64 alphabet: 64 characters used for encoding
    // Index 0-25: A-Z, 26-51: a-z, 52-61: 0-9, 62: +, 63: /
    const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    // Input index
    int i;
    // Output index
    int j = 0;

    // Process input 3 bytes at a time
    for (i = 0; i < length; i += 3) {
        // Combine 3 input bytes into a 24-bit value
        // If we don't have 3 bytes left, pad with zeros
        uint32_t n = (input[i] << 16) |                              // First byte (bits 23-16)
                     ((i + 1 < length ? input[i + 1] : 0) << 8) |    // Second byte (bits 15-8) or 0
                     (i + 2 < length ? input[i + 2] : 0);            // Third byte (bits 7-0) or 0

        // Extract four 6-bit values and map to Base64 characters
        // First 6 bits (bits 23-18)
        output[j++] = b64[(n >> 18) & 63];
        // Second 6 bits (bits 17-12)
        output[j++] = b64[(n >> 12) & 63];
        // Third 6 bits (bits 11-6), or '=' padding if we only had 1 input byte
        output[j++] = (i + 1 < length) ? b64[(n >> 6) & 63] : '=';
        // Fourth 6 bits (bits 5-0), or '=' padding if we had less than 3 input bytes
        output[j++] = (i + 2 < length) ? b64[n & 63] : '=';
    }

    // Null-terminate the output string
    output[j] = '\0';
}

#endif
