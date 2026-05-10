#ifndef BWT_H
#define BWT_H

#include <stddef.h>

// Matrix Struct
typedef struct {
    char *rotation;
    int index;
} Rotation;

// Core Functions
void bwt_encode(unsigned char *input, size_t len, unsigned char *output, int *primary_index, const char *bwt_type);
void bwt_decode(unsigned char *input, size_t len, int primary_index, unsigned char *output);

#endif
