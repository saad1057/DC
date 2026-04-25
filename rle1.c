#include "rle1.h"

void rle1_encode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len) {
    size_t i = 0, j = 0;

    while (i < len) {
        unsigned char ch = input[i];
        int count = 1;

        while (i + count < len && input[i + count] == ch && count < 255)
            count++;

        output[j++] = ch;
        output[j++] = (unsigned char)count;
        i += count;
    }

    *out_len = j;
}

void rle1_decode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len) {
    size_t i = 0, j = 0;

    while (i + 1 < len) {
        unsigned char ch    = input[i];
        unsigned char count = input[i + 1];

        for (int k = 0; k < count; k++)
            output[j++] = ch;

        i += 2;
    }

    *out_len = j;
}