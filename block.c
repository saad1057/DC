#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "block.h"

BlockManager *divide_into_blocks(const char *filename, size_t block_size) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    int num_blocks = (file_size + block_size - 1) / block_size;

    BlockManager *mgr = malloc(sizeof(BlockManager));
    mgr->blocks = malloc(sizeof(Block) * num_blocks);
    mgr->num_blocks = num_blocks;
    mgr->block_size = block_size;

    for (int i = 0; i < num_blocks; i++) {
        size_t to_read = (i == num_blocks - 1)
            ? (file_size - i * block_size)
            : block_size;

        mgr->blocks[i].data = malloc(to_read);
        mgr->blocks[i].size = fread(mgr->blocks[i].data, 1, to_read, f);
        mgr->blocks[i].original_size = mgr->blocks[i].size;
    }

    fclose(f);
    return mgr;
}

int reassemble_blocks(BlockManager *manager, const char *output_filename) {
    FILE *f = fopen(output_filename, "wb");
    if (!f) return -1;

    for (int i = 0; i < manager->num_blocks; i++)
        fwrite(manager->blocks[i].data, 1, manager->blocks[i].size, f);

    fclose(f);
    return 0;
}

void free_block_manager(BlockManager *manager) {
    for (int i = 0; i < manager->num_blocks; i++)
        free(manager->blocks[i].data);
    free(manager->blocks);
    free(manager);
}