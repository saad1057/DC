#include "rle1.h"

/*
 * Packet format:
 * - Literal packet: [len:1..127][len literal bytes]
 * - Run packet:     [0x80 | len:1..127][byte]
 *
 * Runs are only emitted for repeats of length >= 2 so single bytes remain
 * part of literal packets (not [byte,count] pairs).
 */
void rle1_encode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len) {
    size_t i = 0;
    size_t j = 0;

    while (i < len) {
        size_t run = 1;
        while (i + run < len && input[i + run] == input[i] && run < 127) {
            run++;
        }

        if (run >= 2) {
            output[j++] = (unsigned char)(0x80u | run);
            output[j++] = input[i];
            i += run;
            continue;
        }

        size_t lit_start = i;
        size_t lit_len = 1;
        i++;

        while (i < len && lit_len < 127) {
            run = 1;
            while (i + run < len && input[i + run] == input[i] && run < 127) {
                run++;
            }
            if (run >= 2) {
                break;
            }
            lit_len++;
            i++;
        }

        output[j++] = (unsigned char)lit_len;
        for (size_t k = 0; k < lit_len; k++) {
            output[j++] = input[lit_start + k];
        }
    }

    *out_len = j;
}

void rle1_decode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len) {
    size_t i = 0;
    size_t j = 0;

    while (i < len) {
        unsigned char tag = input[i++];
        size_t count = (size_t)(tag & 0x7Fu);
        if (count == 0) {
            break;
        }

        if (tag & 0x80u) {
            if (i >= len) {
                break;
            }
            unsigned char ch = input[i++];
            for (size_t k = 0; k < count; k++) {
                output[j++] = ch;
            }
        } else {
            if (i + count > len) {
                break;
            }
            for (size_t k = 0; k < count; k++) {
                output[j++] = input[i++];
            }
        }
    }

    *out_len = j;
}