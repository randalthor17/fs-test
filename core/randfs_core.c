#include "randfs_core.h"
#include "randfs.h"
#include <asm-generic/errno-base.h>
#include <stdbool.h>
#include <stdint.h>

static bool get_index_block_occupied(unsigned char *bitmap, uint64_t idx) {
  uint32_t byte_idx = idx / 8;
  uint8_t bit_idx = idx % 8;
  if ((bitmap[byte_idx] & (1 << bit_idx)) != 0)
    return true; // Occupied
  else
    return false;
}

static void set_index_block_occupied(unsigned char *bitmap, uint64_t idx) {
  uint32_t byte_idx = idx / 8;
  uint8_t bit_idx = idx % 8;
  bitmap[byte_idx] |= (1 << bit_idx);
}

static void set_index_block_free(unsigned char *bitmap, uint64_t idx) {
  uint32_t byte_idx = idx / 8;
  uint8_t bit_idx = idx % 8;
  bitmap[byte_idx] &= ~(1 << bit_idx);
}

uint32_t fs_alloc_block(fs_superblock_t *sb, unsigned char *bitmap) {
  for (uint32_t i = sb->data_start; i < sb->num_blocks_total; i++) {
    if (!get_index_block_occupied(bitmap, i)) {
      set_index_block_occupied(bitmap, i);
      return i;
    }
  }
  return 0;
}

int32_t fs_free_block(fs_superblock_t *sb, unsigned char *bitmap,
                      uint64_t idx) {
  // idx cannot be >= total blocks, or < start of data block
  if (idx >= sb->num_blocks_total || idx < sb->data_start) {
    return -ERANGE;
  }
  if (!get_index_block_occupied(bitmap, idx)) {
    return -ENOENT;
  }
  set_index_block_free(bitmap, idx);
  return 0;
}

uint32_t fs_alloc_inode(fs_superblock_t *sb, fs_inode_t *inodes) {
  // first inode is always occupied (root inode)
  // return 0 if not found
  for (uint32_t i = 1; i < sb->num_inodes; i++) {
    if (inodes[i].type == FS_TYPE_FREE) {
      return i;
    }
  }
  return 0;
}
