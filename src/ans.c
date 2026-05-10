#include "ans.h"
#include <stdlib.h>
#include <string.h>

#define L_BITS 16
#define L_COUNT (1 << L_BITS)
#define M_BITS 12
#define M_MASK ((1 << M_BITS) - 1)

typedef struct {
    uint16_t freq;
    uint16_t cum;
} Symbol;

void ans_encode(unsigned char *input, size_t len, unsigned char *output, size_t *out_len) {
    if (len == 0) { *out_len = 0; return; }

    // 1. Count Frequencies
    uint32_t counts[256] = {0};
    for (size_t i = 0; i < len; i++) counts[input[i]]++;

    // 2. Normalize to M_BITS (4096) — single pass, no slow loop
    Symbol syms[256];
    memset(syms, 0, sizeof(syms));
    uint32_t total = 0;
    int last = -1;
    for (int i = 0; i < 256; i++) {
        if (counts[i] > 0) {
            uint32_t f = (uint32_t)((double)counts[i] / len * (1 << M_BITS));
            if (f == 0) f = 1;
            syms[i].freq = (uint16_t)f;
            total += f;
            last = i;
        }
    }
    // Apply signed correction to largest-freq symbol to hit exactly 4096
    if (last >= 0) {
        int32_t delta = (int32_t)(1 << M_BITS) - (int32_t)total;
        int biggest = 0;
        for (int i = 1; i < 256; i++)
            if (syms[i].freq > syms[biggest].freq) biggest = i;
        syms[biggest].freq = (uint16_t)((int32_t)syms[biggest].freq + delta);
    }
    // Recompute cumulative sums
    uint32_t cum = 0;
    for (int i = 0; i < 256; i++) {
        syms[i].cum = (uint16_t)cum;
        cum += syms[i].freq;
    }

    // 3. Write Header (Frequency Table)
    unsigned char *ptr = output;
    for (int i = 0; i < 256; i++) {
        *ptr++ = (unsigned char)(syms[i].freq & 0xFF);
        *ptr++ = (unsigned char)(syms[i].freq >> 8);
    }

    // 4. Encode (BACKWARDS)
    uint32_t state = L_COUNT;
    uint16_t *bitstream = malloc((len * 2 + 64) * sizeof(uint16_t));
    int b_idx = 0;

    for (int i = (int)len - 1; i >= 0; i--) {
        unsigned char c = input[i];
        uint32_t freq = syms[c].freq;
        uint32_t cum = syms[c].cum;

        while (state >= (freq << (32 - M_BITS))) {
            bitstream[b_idx++] = (uint16_t)(state & 0xFFFF);
            state >>= 16;
        }
        state = ((state / freq) << M_BITS) + (state % freq) + cum;
    }

    // 5. Write state and bitstream
    memcpy(ptr, &state, 4); ptr += 4;
    memcpy(ptr, &b_idx, 4); ptr += 4;
    for (int i = b_idx - 1; i >= 0; i--) {
        memcpy(ptr, &bitstream[i], 2); ptr += 2;
    }

    *out_len = ptr - output;
    free(bitstream);
}

void ans_decode(unsigned char *input, size_t len, unsigned char *output, size_t *out_len) {
    (void)len;
    unsigned char *ptr = input;
    Symbol syms[256];
    uint32_t cum_map[1 << M_BITS];
    uint32_t total = 0;

    for (int i = 0; i < 256; i++) {
        syms[i].freq = ptr[0] | (ptr[1] << 8);
        ptr += 2;
        if (syms[i].freq > 0) {
            for (int k = 0; k < syms[i].freq; k++) cum_map[total + k] = i;
            syms[i].cum = total;
            total += syms[i].freq;
        }
    }

    uint32_t state; memcpy(&state, ptr, 4); ptr += 4;
    int b_idx; memcpy(&b_idx, ptr, 4); ptr += 4;
    uint16_t *bitstream = (uint16_t*)ptr;
    int current_b = 0;

    size_t target_len = *out_len;
    for (size_t i = 0; i < target_len; i++) {
        uint32_t m = state & M_MASK;
        unsigned char c = (unsigned char)cum_map[m];
        output[i] = c;

        state = syms[c].freq * (state >> M_BITS) + (m - syms[c].cum);

        while (state < L_COUNT && current_b < b_idx) {
            state = (state << 16) | bitstream[current_b++];
        }
    }
}