#include "fs_test.h"
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define NUM_INODES 1024
#define MAX_PATH_LEN 1024

typedef int16_t error_t;
typedef int64_t fd_t;

typedef struct {
  fs_superblock_t superblock;
  error_t error; // 0 is ok, else error
} init_fs_error_t;

init_fs_error_t init_fs(uint64_t size_bytes) {
  fs_superblock_t superblock;
  superblock.inode_table_start = 1;
  superblock.num_inodes = NUM_INODES;
  superblock.magic_number = FS_MAGIC_NUMBER;
  superblock.block_size = FS_BLOCK_SIZE;
  superblock.num_inode_blocks =
      ceil((double)superblock.num_inodes * sizeof(fs_inode_t) /
           superblock.block_size);
  superblock.bitmap_start =
      superblock.inode_table_start + superblock.num_inode_blocks;
  superblock.data_start = superblock.bitmap_start + 1;
  superblock.num_blocks_total =
      ceil((double)size_bytes / superblock.block_size);

  if (superblock.data_start >= superblock.num_blocks_total) {
    errno = ENOSPC;
    return (init_fs_error_t){.superblock = (fs_superblock_t){0}, .error = -1};
  } else {
    return (init_fs_error_t){.superblock = superblock, .error = 0};
  }
}

error_t write_superblock(fd_t file, fs_superblock_t superblock) {
  unsigned char superblock_temp_buf[superblock.block_size];
  memset(superblock_temp_buf, 0, sizeof(superblock_temp_buf));
  memcpy(superblock_temp_buf, &superblock, sizeof(fs_superblock_t));
  ssize_t written = pwrite(file, superblock_temp_buf, superblock.block_size, 0);
  if (written != (ssize_t)sizeof(superblock_temp_buf))
    return -1;
  return 0;
}

error_t zero_inode_table(fd_t file, fs_superblock_t superblock) {
  for (uint32_t i = 0; i < superblock.num_inode_blocks; i++) {
    unsigned char temp_zero_buf[superblock.block_size];
    memset(temp_zero_buf, 0, sizeof(temp_zero_buf));
    int written =
        pwrite(file, &temp_zero_buf, superblock.block_size,
               (superblock.inode_table_start + i) * superblock.block_size);
    if (written != (ssize_t)sizeof(temp_zero_buf))
      return -1;
  }
  return 0;
}

error_t set_index_buf_occupied(unsigned char *buf, uint64_t size,
                               uint64_t idx) {
  if (idx >= size * sizeof(unsigned char)) {
    errno = ERANGE;
    return -1;
  } else {
    uint32_t byte_idx = idx / 8;
    uint8_t bit_idx = idx % 8;
    buf[byte_idx] |= (1 << bit_idx);
    return 0;
  }
}

error_t zero_bitmap_block(fd_t file, fs_superblock_t superblock) {
  unsigned char temp_zero_buf[superblock.block_size];
  memset(temp_zero_buf, 0, sizeof(temp_zero_buf));
  set_index_buf_occupied(temp_zero_buf, sizeof(temp_zero_buf), 0);

  ssize_t written = pwrite(file, &temp_zero_buf, superblock.block_size,
                           superblock.bitmap_start * superblock.block_size);
  if (written != (ssize_t)sizeof(temp_zero_buf))
    return -1;
  return 0;
}

error_t write_root_inode(fd_t file, fs_superblock_t superblock) {
  // set created_at and modified_at
  time_t timestamp = time(NULL);
  fs_inode_t root_inode = {
      .type = FS_TYPE_DIR,
      .mode = 0755,
      .size = 2 * sizeof(fs_dirent_t),
      .direct = {superblock.data_start},
      .created_at = timestamp,
      .modified_at = timestamp,
  };
  ssize_t written =
      pwrite(file, &root_inode, sizeof(root_inode),
             superblock.inode_table_start * superblock.block_size);
  if (written != (ssize_t)sizeof(root_inode))
    return -1;
  return 0;
}

error_t write_root_data_block(fd_t file, fs_superblock_t superblock) {
  unsigned char temp_buf[superblock.block_size];
  memset(temp_buf, 0, sizeof(temp_buf));
  fs_dirent_t *entries = (fs_dirent_t *)temp_buf;
  size_t entries_per_block = superblock.block_size / sizeof(fs_dirent_t);
  for (size_t i = 0; i < entries_per_block; i++) {
    entries[i].inode = FS_DIRENT_EMPTY; // initialize all as empty
  }
  // current dir
  strncpy(entries[0].name, ".", FS_MAX_FILENAME_SIZE);
  entries[0].inode = 0; // root
  // root is its own parent
  strncpy(entries[1].name, "..", FS_MAX_FILENAME_SIZE);
  entries[1].inode = 0;
  // write
  ssize_t written = pwrite(file, temp_buf, sizeof(temp_buf),
                           superblock.data_start * superblock.block_size);
  if (written != (ssize_t)sizeof(temp_buf))
    return -1;
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    errno = EINVAL;
    perror("mkfs requires image path and size of the image (in blocks).\n");
    return errno;
  }
  // first arg is image path, 2nd is size
  char dev_name[1024];
  strncpy(dev_name, argv[1], sizeof(dev_name) - 1);
  dev_name[sizeof(dev_name) - 1] = '\0';

  uint64_t size_bytes = (uint64_t)atoi(argv[2]) * FS_BLOCK_SIZE;
  // try to initialize superblock
  init_fs_error_t superblock_init_err = init_fs(size_bytes);
  if (superblock_init_err.error == -1) {
    perror("Initialization failed, metadata blocks exceed total blocks.\n");
    return errno;
  }
  fs_superblock_t superblock = superblock_init_err.superblock;

  // try to open file
  fd_t file = open(dev_name, O_RDWR | O_CREAT | O_TRUNC,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  if (file == -1) {
    perror("Error opening file.\n");
    return errno;
  }

  // try to truncate file to size
  if (ftruncate(file, size_bytes) == -1) {
    perror("Error truncating file.\n");
    return errno;
  }

  // try to write superblock to file
  if (write_superblock(file, superblock) == -1) {
    perror("Error writing superblock.\n");
    return errno;
  }

  // try to zero out inode table
  if (zero_inode_table(file, superblock) == -1) {
    perror("Error writing empty inode table.\n");
    return errno;
  }
  if (zero_bitmap_block(file, superblock) == -1) {
    perror("Error writing empty bitmap block.\n");
    return errno;
  }

  if (write_root_inode(file, superblock) == -1) {
    perror("Error writing root inode.\n");
    return errno;
  }

  if (write_root_data_block(file, superblock) == -1) {
    perror("Error writing root data block.\n");
    return errno;
  }

  if (close(file) == -1) {
    fprintf(stderr, "Error closing file.\n");
    return errno;
  }

  return EXIT_SUCCESS;
}
