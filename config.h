// config.h
#ifndef BZIP2SIM_CONFIG_H
#define BZIP2SIM_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t block_size;
    bool rle1_enabled;
    bool mtf_enabled;
    bool rle2_enabled;
    bool huffman_enabled;
    bool benchmark_mode;
    bool output_metrics;
    char input_directory[256];
    char output_directory[256];
    char bwt_type[32];
} Config;

Config load_config(const char *filename);

#endif