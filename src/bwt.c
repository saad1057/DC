#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bwt.h"

// ==========================================
// 1. MATRIX IMPLEMENTATION (Standard)
// ==========================================
int compare_rotations(const void *a, const void *b) {
    const Rotation *ra = (const Rotation *)a;
    const Rotation *rb = (const Rotation *)b;
    return strcmp(ra->rotation, rb->rotation);
}

static void bwt_encode_matrix(unsigned char *input, size_t len, unsigned char *output, int *primary_index) {
    Rotation *rotations = malloc(len * sizeof(Rotation));
    char *buf = malloc(len * 2 + 1);

    memcpy(buf, input, len);
    memcpy(buf + len, input, len);
    buf[2 * len] = '\0';

    for (size_t i = 0; i < len; i++) {
        rotations[i].rotation = buf + i;
        rotations[i].index = (int)i;

        char *rot = malloc(len + 1);
        memcpy(rot, buf + i, len);
        rot[len] = '\0';
        rotations[i].rotation = rot;
    }

    qsort(rotations, len, sizeof(Rotation), compare_rotations);

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

// ==========================================
// 2. SUFFIX ARRAY — PREFIX DOUBLING O(n log n)
// ==========================================
static void build_suffix_array(unsigned char *input, int n, int *sa) {
    int *rank = malloc(n * sizeof(int));
    int *tmp  = malloc(n * sizeof(int));
    int *sa2  = malloc(n * sizeof(int));

    // Initial sort by single character using counting sort
    int cnt[256] = {0};
    for (int i = 0; i < n; i++) cnt[input[i]]++;
    for (int i = 1; i < 256; i++) cnt[i] += cnt[i-1];
    for (int i = n-1; i >= 0; i--) sa[--cnt[input[i]]] = i;

    // Initial ranks from character values
    rank[sa[0]] = 0;
    for (int i = 1; i < n; i++)
        rank[sa[i]] = rank[sa[i-1]] + (input[sa[i]] != input[sa[i-1]] ? 1 : 0);

    // Prefix doubling: each round doubles the comparison length
    for (int gap = 1; gap < n && rank[sa[n-1]] < n-1; gap *= 2) {
        // Build sa2: suffixes sorted by their second half (rank at i+gap)
        // Suffixes whose second half starts beyond n get rank -1 (smallest)
        int p = 0;
        for (int i = n - gap; i < n; i++) sa2[p++] = i;
        for (int i = 0; i < n; i++) if (sa[i] >= gap) sa2[p++] = sa[i] - gap;

        // Counting sort sa2 by first-half rank to get new sa
        int max_rank = rank[sa[n-1]] + 2;
        int *c = calloc(max_rank, sizeof(int));
        for (int i = 0; i < n; i++) c[rank[sa2[i]]]++;
        for (int i = 1; i < max_rank; i++) c[i] += c[i-1];
        for (int i = n-1; i >= 0; i--) sa[--c[rank[sa2[i]]]] = sa2[i];
        free(c);

        // Recompute ranks using both halves
        tmp[sa[0]] = 0;
        for (int i = 1; i < n; i++) {
            int ra = rank[sa[i]],   rb = (sa[i]+gap  < n) ? rank[sa[i]+gap]   : -1;
            int pa = rank[sa[i-1]], pb = (sa[i-1]+gap < n) ? rank[sa[i-1]+gap] : -1;
            tmp[sa[i]] = tmp[sa[i-1]] + (ra != pa || rb != pb ? 1 : 0);
        }
        memcpy(rank, tmp, n * sizeof(int));
    }

    free(rank); free(tmp); free(sa2);
}

static void bwt_encode_suffix(unsigned char *input, size_t len, unsigned char *output, int *primary_index) {
    int n = (int)len;
    int *sa = malloc(n * sizeof(int));
    build_suffix_array(input, n, sa);
    for (int i = 0; i < n; i++) {
        output[i] = input[(sa[i] + n - 1) % n];
        if (sa[i] == 0) *primary_index = i;
    }
    free(sa);
}

// ==========================================
// 3. MAIN ROUTER & DECODER
// ==========================================
void bwt_encode(unsigned char *input, size_t len, unsigned char *output, int *primary_index, const char *bwt_type) {
    if (strcmp(bwt_type, "suffix_array") == 0) {
        printf("   -> Using Suffix Array BWT\n");
        bwt_encode_suffix(input, len, output, primary_index);
    } else {
        printf("   -> Using Matrix BWT\n");
        bwt_encode_matrix(input, len, output, primary_index);
    }
}

void bwt_decode(unsigned char *input, size_t len, int primary_index, unsigned char *output) {
    int freq[256] = {0};
    for (size_t i = 0; i < len; i++)
        freq[input[i]]++;

    int cumul[256] = {0};
    for (int i = 1; i < 256; i++)
        cumul[i] = cumul[i - 1] + freq[i - 1];

    int *T = malloc(len * sizeof(int));
    int pos[256];
    memcpy(pos, cumul, sizeof(cumul));

    for (size_t i = 0; i < len; i++)
        T[pos[input[i]]++] = (int)i;

    int idx = T[primary_index];
    for (size_t i = 0; i < len; i++) {
        output[i] = input[idx];
        idx = T[idx];
    }

    free(T);
}
