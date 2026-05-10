#include "mtf.h"
#include <string.h>

void mtf_encode(unsigned char *input, size_t len, unsigned char *output) {
    unsigned char list[256];
    for (int i = 0; i < 256; i++) {
        list[i] = (unsigned char)i;
    }

    for (size_t i = 0; i < len; i++) {
        unsigned char c = input[i];
        int idx = 0;

        for (int j = 0; j < 256; j++) {
            if (list[j] == c) {
                idx = j;
                break;
            }
        }

        output[i] = (unsigned char)idx;

        for (int j = idx; j > 0; j--) {
            list[j] = list[j - 1];
        }
        list[0] = c;
    }
}

void mtf_decode(unsigned char *input, size_t len, unsigned char *output) {
    unsigned char list[256];
    for (int i = 0; i < 256; i++) {
        list[i] = (unsigned char)i;
    }

    for (size_t i = 0; i < len; i++) {
        int idx = input[i];
        unsigned char c = list[idx];

        output[i] = c;

        for (int j = idx; j > 0; j--) {
            list[j] = list[j - 1];
        }
        list[0] = c;
    }
}
