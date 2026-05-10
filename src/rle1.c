#include "rle1.h"

void rle1_encode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len) {
    size_t i = 0, j = 0;

    while (i < len) {
        unsigned char ch = input[i];
        int count = 1;

        // Max run we can encode in one chunk is 4 + 255 = 259
        while (i + count < len && input[i + count] == ch && count < 259) {
            count++;
        }

        if (count < 4) {
            // Under threshold: just output the characters literally
            for (int k = 0; k < count; k++) {
                output[j++] = ch;
            }
        } else {
            // Threshold met: output 4 characters, then the remaining count
            for (int k = 0; k < 4; k++) {
                output[j++] = ch;
            }
            output[j++] = (unsigned char)(count - 4);
        }
        
        i += count;
    }

    *out_len = j;
}

void rle1_decode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len) {
    size_t i = 0, j = 0;
    int run_len = 0;
    unsigned int last_ch = 256; // 256 is an impossible byte value

    while (i < len) {
        unsigned char ch = input[i++];
        output[j++] = ch;
        
        if (ch == last_ch) {
            run_len++;
            if (run_len == 4) {
                // We saw 4 in a row! The next byte is the extra count.
                if (i < len) {
                    unsigned char extra = input[i++];
                    for (int k = 0; k < extra; k++) {
                        output[j++] = ch;
                    }
                }
                // Reset tracker so we don't accidentally keep counting
                run_len = 0;
                last_ch = 256;
            }
        } else {
            run_len = 1;
            last_ch = ch;
        }
    }

    *out_len = j;
}
