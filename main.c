#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "bwt.h"
#include "config.h"
#include "rle1.h"

#define MAGIC "BZS1"

typedef struct {
    uint32_t orig_size;
    uint32_t stage_size;
    uint32_t primary_index;
    uint32_t final_size;
} EncodedBlockHeader;

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

static void print_block_text_preview(const char *label, uint32_t block_index,
                                     const unsigned char *data, size_t len) {
    char tagged[64];
    snprintf(tagged, sizeof(tagged), "%s block %u", label, block_index);
    print_text_preview(tagged, data, len);
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

        unsigned char *rle_buf = NULL;
        size_t rle_size = stage_size;
        if (cfg->rle1_enabled) {
            rle_buf = malloc(stage_size * 2 + 2);
            if (!rle_buf) {
                fprintf(stderr, "Out of memory during RLE encode\n");
                fclose(out);
                free_block_manager(mgr);
                return 1;
            }
            rle1_encode(blk->data, blk->size, rle_buf, &rle_size);
            stage_data = rle_buf;
            stage_size = rle_size;
        }

        unsigned char *bwt_buf = malloc(stage_size);
        if (!bwt_buf) {
            fprintf(stderr, "Out of memory during BWT encode\n");
            free(rle_buf);
            fclose(out);
            free_block_manager(mgr);
            return 1;
        }

        int primary_index = 0;
        bwt_encode(stage_data, stage_size, bwt_buf, &primary_index);

        EncodedBlockHeader hdr;
        hdr.orig_size = (uint32_t)blk->size;
        hdr.stage_size = (uint32_t)stage_size;
        hdr.primary_index = (uint32_t)primary_index;
        hdr.final_size = (uint32_t)stage_size;

        fwrite(&hdr, sizeof(hdr), 1, out);
        fwrite(bwt_buf, 1, stage_size, out);

        printf("Block %d encoded: orig=%u stage=%u primary=%u\n",
               i, hdr.orig_size, hdr.stage_size, hdr.primary_index);
        print_block_text_preview("Input", (uint32_t)i, blk->data, blk->size);
        print_block_text_preview("Encoded payload", (uint32_t)i, bwt_buf, stage_size);

        free(rle_buf);
        free(bwt_buf);
    }

    fclose(out);
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
        if (fread(&hdr, sizeof(hdr), 1, in) != 1) {
            fprintf(stderr, "Invalid encoded format at block %u\n", i);
            fclose(out);
            fclose(in);
            return 1;
        }

        unsigned char *encoded = malloc(hdr.final_size);
        unsigned char *bwt_out = malloc(hdr.stage_size);
        if (!encoded || !bwt_out) {
            fprintf(stderr, "Out of memory during decode\n");
            free(encoded);
            free(bwt_out);
            fclose(out);
            fclose(in);
            return 1;
        }

        if (fread(encoded, 1, hdr.final_size, in) != hdr.final_size) {
            fprintf(stderr, "Unexpected EOF while reading block %u\n", i);
            free(encoded);
            free(bwt_out);
            fclose(out);
            fclose(in);
            return 1;
        }

        bwt_decode(encoded, hdr.stage_size, (int)hdr.primary_index, bwt_out);

        unsigned char *final_buf = bwt_out;
        size_t final_len = hdr.stage_size;
        unsigned char *rle_out = NULL;
        size_t rle_out_len = 0;

        if (cfg->rle1_enabled) {
            rle_out = malloc(hdr.orig_size);
            if (!rle_out) {
                fprintf(stderr, "Out of memory during RLE decode\n");
                free(encoded);
                free(bwt_out);
                fclose(out);
                fclose(in);
                return 1;
            }
            rle1_decode(bwt_out, hdr.stage_size, rle_out, &rle_out_len);
            final_buf = rle_out;
            final_len = rle_out_len;
        }

        fwrite(final_buf, 1, final_len, out);

        printf("Block %u decoded: stage=%u final=%zu\n", i, hdr.stage_size, final_len);
        print_block_text_preview("Decoded", i, final_buf, final_len);

        free(encoded);
        free(bwt_out);
        free(rle_out);
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

    fprintf(stderr, "Invalid mode '%s'. Use encode or decode.\n", argv[1]);
    return 1;
}