#include "fs_test.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
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

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("mkfs requires image path and size of the image.\n");
    return -1;
  }
  // first arg is image path, 2nd is size
  char dev_name[1024];
  strncpy(dev_name, argv[1], sizeof(dev_name) - 1);
  dev_name[sizeof(dev_name) - 1] = '\0';

  uint64_t size_bytes = (uint64_t)atoi(argv[2]) * FS_BLOCK_SIZE;
  // try to initialize superblock
  init_fs_error_t superblock_init_err = init_fs(size_bytes);
  if (superblock_init_err.error != 0) {
    fprintf(stderr,
            "Initialization failed, metadata blocks exceed total blocks.\n");
    return superblock_init_err.error;
  }
  fs_superblock_t superblock = superblock_init_err.superblock;

  // try to open file
  fd_t file = open(dev_name, O_RDWR | O_CREAT | O_TRUNC,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  if (file == -1) {
    fprintf(stderr, "Opening file failed.\n");
    return errno;
  }

  // try to truncate file to size
  error_t ftrunc_err = ftruncate(file, size_bytes);
  if (ftrunc_err == -1) {
    fprintf(stderr, "Truncating file failed.\n");
    return errno;
  }

  // try to write superblock to file
  error_t write_superblock_err = write_superblock(file, superblock);
  if (write_superblock_err == -1) {
    fprintf(stderr, "Writing superblock failed.\n");
    return errno;
  }

  // try to zero out inode table
  error_t zero_inode_table_err = zero_inode_table(file, superblock);
  if (zero_inode_table_err == -1) {
    fprintf(stderr, "Writing empty inode table failed.\n");
    return errno;
  }
  error_t zero_bitmap_block_err = zero_bitmap_block(file, superblock);
  if (zero_bitmap_block_err == -1) {
    fprintf(stderr, "Writing empty bitmap block failed.\n");
    return errno;
  }

  return EXIT_SUCCESS;
}
