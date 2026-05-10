#ifndef ANS_H
#define ANS_H

#include <stddef.h>
#include <stdint.h>

void ans_encode(unsigned char *input, size_t len, unsigned char *output, size_t *out_len);
void ans_decode(unsigned char *input, size_t len, unsigned char *output, size_t *out_len);

#endif