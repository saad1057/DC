#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "bwt.h"
#include "config.h"
#include "rle1.h"
#include "mtf.h"
#include "rle2.h"
#include "huffman.h"
#include "ans.h" // Added for Stage 8.3 Extra Credit

#define MAGIC "BZS1"

typedef struct {
    uint32_t orig_size;
    uint32_t stage_size;
    uint32_t primary_index;
    uint32_t final_size;   // Size after RLE-2
    uint32_t entropy_size; // Final size on disk after Huffman or ANS
} EncodedBlockHeader;

static void print_stage_sizes(uint32_t block_id, const char *stage, size_t in_size, size_t out_size) {
    printf("  [Block %u] %-14s in=%zu bytes, out=%zu bytes\n", block_id, stage, in_size, out_size);
}

static void print_text_preview(const char *label, const unsigned char *data, size_t len) {
    size_t n = len < 200 ? len : 200;
    printf("%s (%zu bytes): ", label, len);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = data[i];
        if (c >= 32 && c <= 126) {
            putchar((int)c);
        } else if (c == '\n') {
            printf("\\n");
        } else if (c == '\t') {
            printf("\\t");
        } else {
            putchar('.');
        }
    }
    if (n < len) {
        printf("...");
    }
    putchar('\n');
}

static int encode_file(const char *input_filename, const Config *cfg) {
    BlockManager *mgr = divide_into_blocks(input_filename, cfg->block_size);
    if (!mgr) {
        fprintf(stderr, "Failed to read input file: %s\n", input_filename);
        return 1;
    }

    FILE *out = fopen("output.bin", "wb");
    if (!out) {
        fprintf(stderr, "Failed to create output.bin\n");
        free_block_manager(mgr);
        return 1;
    }

    fwrite(MAGIC, 1, 4, out);
    {
        uint32_t block_count = (uint32_t)mgr->num_blocks;
        fwrite(&block_count, sizeof(block_count), 1, out);
    }

    for (int i = 0; i < mgr->num_blocks; i++) {
        Block *blk = &mgr->blocks[i];
        unsigned char *stage_data = blk->data;
        size_t stage_size = blk->size;
        print_stage_sizes((uint32_t)i, "Input", blk->size, blk->size);

        // Stage 1: RLE-1
        unsigned char *rle1_buf = NULL;
        size_t rle1_size = stage_size;
        if (cfg->rle1_enabled) {
            size_t before = stage_size;
            rle1_buf = malloc(stage_size * 2 + 2);
            rle1_encode(blk->data, blk->size, rle1_buf, &rle1_size);
            stage_data = rle1_buf;
            stage_size = rle1_size;
            print_stage_sizes((uint32_t)i, "RLE-1", before, stage_size);
        } else {
            print_stage_sizes((uint32_t)i, "RLE-1(skip)", stage_size, stage_size);
        }

        // Stage 2: BWT
        size_t bwt_in = stage_size;
        unsigned char *bwt_buf = malloc(stage_size);
        int primary_index = 0;
        bwt_encode(stage_data, stage_size, bwt_buf, &primary_index, cfg->bwt_type);
        stage_data = bwt_buf;
        print_stage_sizes((uint32_t)i, "BWT", bwt_in, stage_size);

        // Stage 3: MTF
        unsigned char *mtf_buf = NULL;
        if (cfg->mtf_enabled) {
            size_t before = stage_size;
            mtf_buf = malloc(stage_size);
            mtf_encode(stage_data, stage_size, mtf_buf);
            stage_data = mtf_buf;
            print_stage_sizes((uint32_t)i, "MTF", before, stage_size);
        } else {
            print_stage_sizes((uint32_t)i, "MTF(skip)", stage_size, stage_size);
        }

        // Stage 4: RLE-2
        unsigned char *rle2_buf = NULL;
        size_t rle2_size = stage_size;
        if (cfg->rle2_enabled) {
            size_t before = stage_size;
            rle2_buf = malloc(stage_size * 2);
            rle2_encode(stage_data, stage_size, rle2_buf, &rle2_size);
            stage_data = rle2_buf;
            stage_size = rle2_size;
            print_stage_sizes((uint32_t)i, "RLE-2", before, stage_size);
        } else {
            print_stage_sizes((uint32_t)i, "RLE-2(skip)", stage_size, stage_size);
        }

        // Stage 5: Entropy Coding (Huffman or ANS)
        size_t entropy_in = stage_size;
        unsigned char *entropy_buf = malloc(stage_size * 2 + 1024); // Extra padding for headers
        size_t final_entropy_size = stage_size;

        if (strcmp(cfg->entropy_type, "ans") == 0) {
            ans_encode(stage_data, stage_size, entropy_buf, &final_entropy_size);
        } else {
            huffman_encode(stage_data, stage_size, entropy_buf, &final_entropy_size);
        }
        stage_data = entropy_buf;
        print_stage_sizes((uint32_t)i, "Entropy", entropy_in, final_entropy_size);

        EncodedBlockHeader hdr;
        hdr.orig_size = (uint32_t)blk->size;
        hdr.stage_size = (uint32_t)(cfg->rle1_enabled ? rle1_size : blk->size);
        hdr.primary_index = (uint32_t)primary_index;
        hdr.final_size = (uint32_t)stage_size; // Size before entropy coding
        hdr.entropy_size = (uint32_t)final_entropy_size;

        fwrite(&hdr, sizeof(hdr), 1, out);
        fwrite(stage_data, 1, hdr.entropy_size, out);

        printf("Block %d encoded: orig=%u bwt=%u final=%u primary=%u (using %s)\n",
               i, hdr.orig_size, hdr.stage_size, hdr.entropy_size, hdr.primary_index, cfg->entropy_type);

        free(rle1_buf);
        free(bwt_buf);
        free(mtf_buf);
        free(rle2_buf);
        free(entropy_buf);
    }

    fclose(out);
    print_text_preview("Input preview", mgr->blocks[0].data, mgr->blocks[0].size);
    free_block_manager(mgr);
    printf("Done. Output written to output.bin\n");
    return 0;
}

static int decode_file(const char *encoded_filename, const Config *cfg) {
    FILE *in = fopen(encoded_filename, "rb");
    if (!in) {
        fprintf(stderr, "Failed to open encoded file: %s\n", encoded_filename);
        return 1;
    }

    char magic[4];
    if (fread(magic, 1, 4, in) != 4 || memcmp(magic, MAGIC, 4) != 0) {
        fprintf(stderr, "Invalid encoded format (missing magic header)\n");
        fclose(in);
        return 1;
    }

    uint32_t block_count = 0;
    if (fread(&block_count, sizeof(block_count), 1, in) != 1) {
        fprintf(stderr, "Invalid encoded format (missing block count)\n");
        fclose(in);
        return 1;
    }

    FILE *out = fopen("decoded.bin", "wb");
    if (!out) {
        fprintf(stderr, "Failed to create decoded.bin\n");
        fclose(in);
        return 1;
    }

    for (uint32_t i = 0; i < block_count; i++) {
        EncodedBlockHeader hdr;
        if (fread(&hdr, sizeof(hdr), 1, in) != 1) break;

        unsigned char *encoded = malloc(hdr.entropy_size);
        fread(encoded, 1, hdr.entropy_size, in);

        unsigned char *stage_data = encoded;
        size_t stage_size = hdr.entropy_size;
        print_stage_sizes(i, "Encoded input", hdr.entropy_size, hdr.entropy_size);

        // Stage 5 Inverse: Entropy
        unsigned char *entropy_out = malloc(hdr.final_size);
        size_t entropy_out_len = hdr.final_size;
        size_t entropy_before = stage_size;
        if (strcmp(cfg->entropy_type, "ans") == 0) {
            ans_decode(stage_data, stage_size, entropy_out, &entropy_out_len);
        } else {
            huffman_decode(stage_data, stage_size, entropy_out, &entropy_out_len);
        }
        stage_data = entropy_out;
        stage_size = entropy_out_len;
        print_stage_sizes(i, "Entropy^-1", entropy_before, stage_size);

        // Stage 4 Inverse: RLE-2
        unsigned char *rle2_out = malloc(hdr.stage_size);
        size_t rle2_out_len = 0;
        size_t rle2_before = stage_size;
        rle2_decode(stage_data, stage_size, rle2_out, &rle2_out_len);
        stage_data = rle2_out;
        stage_size = rle2_out_len;
        print_stage_sizes(i, "RLE-2^-1", rle2_before, stage_size);

        // Stage 3 Inverse: MTF
        unsigned char *mtf_out = malloc(stage_size);
        size_t mtf_before = stage_size;
        mtf_decode(stage_data, stage_size, mtf_out);
        stage_data = mtf_out;
        print_stage_sizes(i, "MTF^-1", mtf_before, stage_size);

        // Stage 2 Inverse: BWT
        unsigned char *bwt_out = malloc(stage_size);
        size_t bwt_before = stage_size;
        bwt_decode(stage_data, stage_size, (int)hdr.primary_index, bwt_out);
        stage_data = bwt_out;
        print_stage_sizes(i, "BWT^-1", bwt_before, stage_size);

        // Stage 1 Inverse: RLE-1
        unsigned char *rle1_out = malloc(hdr.orig_size);
        size_t final_len = 0;
        size_t rle1_before = stage_size;
        rle1_decode(stage_data, stage_size, rle1_out, &final_len);
        print_stage_sizes(i, "RLE-1^-1", rle1_before, final_len);

        fwrite(rle1_out, 1, final_len, out);

        printf("Block %u decoded: entropy=%u final=%zu\n", i, hdr.entropy_size, final_len);

        free(encoded);
        free(entropy_out);
        free(rle2_out);
        free(mtf_out);
        free(bwt_out);
        free(rle1_out);
    }

    fclose(out);
    fclose(in);
    printf("Done. Output written to decoded.bin\n");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <encode|decode> <file>\n", argv[0]);
        return 1;
    }

    Config cfg = load_config("config.ini");

    if (strcmp(argv[1], "encode") == 0) {
        return encode_file(argv[2], &cfg);
    }
    if (strcmp(argv[1], "decode") == 0) {
        return decode_file(argv[2], &cfg);
    }

    return 1;
}
