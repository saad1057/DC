#include "rle2.h"

void rle2_encode(unsigned char *input, size_t len, unsigned char *output, size_t *out_len) {
    size_t i = 0;
    size_t j = 0;

    while (i < len) {
        if (input[i] == 0) {
            int count = 0;
            while (i < len && input[i] == 0 && count < 255) {
                count++;
                i++;
            }
            output[j++] = 0;
            output[j++] = (unsigned char)count;
        } else {
            output[j++] = input[i++];
        }
    }

    *out_len = j;
}

void rle2_decode(unsigned char *input, size_t len, unsigned char *output, size_t *out_len) {
    size_t i = 0;
    size_t j = 0;

    while (i < len) {
        if (input[i] == 0) {
            i++;
            unsigned char count = input[i++];
            for (int k = 0; k < count; k++) {
                output[j++] = 0;
            }
        } else {
            output[j++] = input[i++];
        }
    }

    *out_len = j;
}