// config.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

static bool parse_bool(const char *val) {
    return strncmp(val, "true", 4) == 0;
}

Config load_config(const char *filename) {
    /* Defaults are used when config.ini is missing or partial. */
    Config cfg = {
        .block_size       = 500000,
        .rle1_enabled     = true,
        .mtf_enabled      = true,
        .rle2_enabled     = true,
        .huffman_enabled  = true,
        .benchmark_mode   = false,
        .output_metrics   = true,
    };
    strcpy(cfg.bwt_type, "matrix");
    strcpy(cfg.input_directory, "./benchmarks/");
    strcpy(cfg.output_directory, "./results/");

    FILE *f = fopen(filename, "r");
    if (!f) return cfg;  // return defaults if file missing

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Strip comments
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';

        char key[128], val[128];
        /* Expected line format: key = value */
        if (sscanf(line, " %127[^= ] = %127s", key, val) != 2)
            continue;

        if      (strcmp(key, "block_size")      == 0) cfg.block_size      = atoi(val);
        else if (strcmp(key, "rle1_enabled")    == 0) cfg.rle1_enabled    = parse_bool(val);
        else if (strcmp(key, "bwt_type")        == 0) strncpy(cfg.bwt_type, val, 31);
        else if (strcmp(key, "mtf_enabled")     == 0) cfg.mtf_enabled     = parse_bool(val);
        else if (strcmp(key, "rle2_enabled")    == 0) cfg.rle2_enabled    = parse_bool(val);
        else if (strcmp(key, "huffman_enabled") == 0) cfg.huffman_enabled = parse_bool(val);
        else if (strcmp(key, "benchmark_mode")  == 0) cfg.benchmark_mode  = parse_bool(val);
        else if (strcmp(key, "output_metrics")  == 0) cfg.output_metrics  = parse_bool(val);
        else if (strcmp(key, "input_directory") == 0) strncpy(cfg.input_directory, val, 255);
        else if (strcmp(key, "output_directory")== 0) strncpy(cfg.output_directory, val, 255);
    }

    fclose(f);
    return cfg;
}