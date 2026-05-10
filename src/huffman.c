#include "huffman.h"
#include <stdlib.h>
#include <string.h>

static void get_lengths(HuffmanNode *node, int depth, HuffmanCode *codes) {
    if (!node) return;
    if (!node->left && !node->right) {
        codes[node->symbol].length = depth;
        return;
    }
    get_lengths(node->left, depth + 1, codes);
    get_lengths(node->right, depth + 1, codes);
}

static void free_tree(HuffmanNode *node) {
    if (!node) return;
    free_tree(node->left);
    free_tree(node->right);
    free(node);
}

void build_huffman_tree(int *frequencies, HuffmanNode **root) {
    HuffmanNode *nodes[512];
    int num_nodes = 0;

    for (int i = 0; i < 256; i++) {
        if (frequencies[i] > 0) {
            nodes[num_nodes] = malloc(sizeof(HuffmanNode));
            nodes[num_nodes]->symbol = i;
            nodes[num_nodes]->freq = frequencies[i];
            nodes[num_nodes]->left = NULL;
            nodes[num_nodes]->right = NULL;
            num_nodes++;
        }
    }

    if (num_nodes == 0) {
        *root = NULL;
        return;
    }

    if (num_nodes == 1) {
        *root = malloc(sizeof(HuffmanNode));
        (*root)->symbol = 0;
        (*root)->freq = nodes[0]->freq;
        (*root)->left = nodes[0];
        (*root)->right = NULL;
        return;
    }

    while (num_nodes > 1) {
        int m1 = 0, m2 = 1;
        if (nodes[m2]->freq < nodes[m1]->freq) {
            m1 = 1; m2 = 0;
        }
        for (int i = 2; i < num_nodes; i++) {
            if (nodes[i]->freq < nodes[m1]->freq) {
                m2 = m1;
                m1 = i;
            } else if (nodes[i]->freq < nodes[m2]->freq) {
                m2 = i;
            }
        }

        HuffmanNode *parent = malloc(sizeof(HuffmanNode));
        parent->symbol = 0;
        parent->freq = nodes[m1]->freq + nodes[m2]->freq;
        parent->left = nodes[m1];
        parent->right = nodes[m2];

        nodes[m1] = parent;
        nodes[m2] = nodes[num_nodes - 1];
        num_nodes--;
    }

    *root = nodes[0];
}

void generate_canonical_codes(HuffmanNode *root, HuffmanCode *codes) {
    for (int i = 0; i < 256; i++) {
        codes[i].code = 0;
        codes[i].length = 0;
    }
    if (!root) return;

    get_lengths(root, 0, codes);

    int length_counts[32] = {0};
    for (int i = 0; i < 256; i++) {
        if (codes[i].length > 0) {
            length_counts[codes[i].length]++;
        }
    }

    unsigned short next_code[32] = {0};
    unsigned short code = 0;
    for (int bits = 1; bits < 32; bits++) {
        code = (code + length_counts[bits - 1]) << 1;
        next_code[bits] = code;
    }

    for (int i = 0; i < 256; i++) {
        if (codes[i].length > 0) {
            codes[i].code = next_code[codes[i].length]++;
        }
    }
}

void write_header(HuffmanCode *codes, unsigned char *output, size_t *out_len) {
    size_t idx = 0;
    for (int i = 0; i < 256; i++) {
        output[idx++] = codes[i].length;
    }
    *out_len = idx;
}

void encode_data(unsigned char *input, size_t len, HuffmanCode *codes, unsigned char *output, size_t *out_len) {
    size_t byte_idx = 0;
    int bit_pos = 7;
    output[0] = 0;

    for (size_t i = 0; i < len; i++) {
        unsigned char sym = input[i];
        unsigned short code = codes[sym].code;
        int length = codes[sym].length;

        for (int b = length - 1; b >= 0; b--) {
            int bit = (code >> b) & 1;
            if (bit) {
                output[byte_idx] |= (1 << bit_pos);
            }
            bit_pos--;
            if (bit_pos < 0) {
                byte_idx++;
                output[byte_idx] = 0;
                bit_pos = 7;
            }
        }
    }
    if (bit_pos < 7) byte_idx++;
    *out_len = byte_idx;
}

void huffman_encode(unsigned char *input, size_t len, unsigned char *output, size_t *out_len) {
    int freq[256] = {0};
    for (size_t i = 0; i < len; i++) {
        freq[input[i]]++;
    }

    HuffmanNode *root = NULL;
    build_huffman_tree(freq, &root);

    HuffmanCode codes[256];
    generate_canonical_codes(root, codes);

    size_t header_len = 0;
    write_header(codes, output, &header_len);

    size_t data_len = 0;
    encode_data(input, len, codes, output + header_len, &data_len);

    *out_len = header_len + data_len;

    free_tree(root);
}

void huffman_decode(unsigned char *input, size_t len, unsigned char *output, size_t *out_len) {
    HuffmanCode codes[256];
    for (int i = 0; i < 256; i++) {
        codes[i].length = input[i];
        codes[i].code = 0;
    }

    int length_counts[32] = {0};
    for (int i = 0; i < 256; i++) {
        if (codes[i].length > 0) {
            length_counts[codes[i].length]++;
        }
    }

    unsigned short next_code[32] = {0};
    unsigned short code = 0;
    for (int bits = 1; bits < 32; bits++) {
        code = (code + length_counts[bits - 1]) << 1;
        next_code[bits] = code;
    }

    for (int i = 0; i < 256; i++) {
        if (codes[i].length > 0) {
            codes[i].code = next_code[codes[i].length]++;
        }
    }

    size_t in_idx = 256;
    size_t out_idx = 0;
    int bit_pos = 7;

    while (out_idx < *out_len && in_idx < len) {
        unsigned short current_code = 0;
        int current_len = 0;
        int match_found = 0;

        while (!match_found && current_len < 31) {
            int bit = (input[in_idx] >> bit_pos) & 1;
            current_code = (current_code << 1) | bit;
            current_len++;

            bit_pos--;
            if (bit_pos < 0) {
                in_idx++;
                bit_pos = 7;
            }

            for (int i = 0; i < 256; i++) {
                if (codes[i].length == current_len && codes[i].code == current_code) {
                    output[out_idx++] = (unsigned char)i;
                    match_found = 1;
                    break;
                }
            }
        }
    }
}