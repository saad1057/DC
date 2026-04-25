#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bwt.h"

int compare_rotations(const void *a, const void *b) {
    const Rotation *ra = (const Rotation *)a;
    const Rotation *rb = (const Rotation *)b;
    return strcmp(ra->rotation, rb->rotation);
}

void bwt_encode(unsigned char *input, size_t len,
                unsigned char *output, int *primary_index) {
    // Build all rotations
    Rotation *rotations = malloc(len * sizeof(Rotation));
    char *buf = malloc(len * 2 + 1);  // doubled buffer for rotation strings

    memcpy(buf, input, len);
    memcpy(buf + len, input, len);
    buf[2 * len] = '\0';

    for (size_t i = 0; i < len; i++) {
        rotations[i].rotation = buf + i; // points into doubled buffer
        rotations[i].index = (int)i;

        // We need null-terminated strings of length len — copy them
        char *rot = malloc(len + 1);
        memcpy(rot, buf + i, len);
        rot[len] = '\0';
        rotations[i].rotation = rot;
    }

    qsort(rotations, len, sizeof(Rotation), compare_rotations);

    // Last column + find primary index
    for (size_t i = 0; i < len; i++) {
        output[i] = input[(rotations[i].index + len - 1) % len];
        if (rotations[i].index == 0)
            *primary_index = (int)i;
    }

    for (size_t i = 0; i < len; i++)
        free(rotations[i].rotation);
    free(rotations);
    free(buf);
}

void bwt_decode(unsigned char *input, size_t len,
                int primary_index, unsigned char *output) {
    // Count frequency of each character
    int freq[256] = {0};
    for (size_t i = 0; i < len; i++)
        freq[input[i]]++;

    // Cumulative count (first occurrence of each char in sorted first column)
    int cumul[256] = {0};
    for (int i = 1; i < 256; i++)
        cumul[i] = cumul[i - 1] + freq[i - 1];

    // Build the transformation vector T
    int *T = malloc(len * sizeof(int));
    int pos[256];
    memcpy(pos, cumul, sizeof(cumul));

    for (size_t i = 0; i < len; i++)
        T[pos[input[i]]++] = (int)i;

    // Follow the chain starting from primary_index
    int idx = T[primary_index];
    for (size_t i = 0; i < len; i++) {
        output[i] = input[idx];
        idx = T[idx];
    }

    free(T);
}