#include "randfs.h"
#include <stdint.h>

// finds an unallocated block, allocates it in bitmap
// returns block index, or 0 if there's no block free
uint32_t fs_alloc_block(fs_superblock_t *sb, unsigned char *bitmap);

// sets an already allocated block as unallocated
// returns 0, or -ENOENT if the block is already unallocated
//    or -ERANGE if requested block is out of range
int32_t fs_free_block(fs_superblock_t *sb, unsigned char *bitmap, uint64_t idx);

// finds the first unallocated inode
// returns index of inode, or 0 if not found
uint32_t fs_alloc_inode(fs_superblock_t *sb, fs_inode_t *inodes);
