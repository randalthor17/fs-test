#include "randfs.h"
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef int16_t error_t;
typedef int64_t fd_t;

error_t get_index_buf_occupied(unsigned char *buf, uint64_t size,
                               uint64_t idx) {
  if (idx >= size * sizeof(unsigned char)) {
    errno = ERANGE;
    return -1;
  } else {
    uint32_t byte_idx = idx / 8;
    uint8_t bit_idx = idx % 8;
    if ((buf[byte_idx] & (1 << bit_idx)) != 0)
      return 1; // Occupied
    else
      return 0;
  }
}

void print_superblock(fs_superblock_t superblock) {
  printf("Superblock information:\n");
  printf("\tMagic number:\t\t0x%X\n", superblock.magic_number);
  printf("\tBlock size:\t\t%d\n", superblock.block_size);
  printf("\tTotal blocks:\t\t%d\n", superblock.num_blocks_total);
  printf("\tTotal inodes:\t\t%d\n", superblock.num_inodes);
  printf("\tInode index:\t\t%d\n", superblock.inode_table_start);
  printf("\tTotal inode blocks:\t%d\n", superblock.num_inode_blocks);
  printf("\tBitmap index:\t\t%d\n", superblock.bitmap_start);
  printf("\tData index:\t\t%d\n", superblock.data_start);
}

uint32_t print_directory(fs_inode_t *itable, unsigned char *devptr,
                         uint32_t block_size, fs_inode_t *dir_inode,
                         int depth) {
  // iterate over the dir_inode
  // we assume that the given inode is a directory one
  if (dir_inode->type != FS_TYPE_DIR) {
    errno = EINVAL;
    perror("Error, given file is not a dir.\n");
    return -1;
  }

  printf("Filesystem tree:\n");

  size_t dirents_per_block = block_size / sizeof(fs_dirent_t);

  for (int i = 0; i < FS_DIRECT_BLOCK_POINTER_PER_INODE; i++) {

    // if the direct pointer points to 0 i.e. superblock i.e. nothing
    // then skip it
    if (dir_inode->direct[i] == 0)
      continue;

    fs_dirent_t *dirents = (fs_dirent_t *)((unsigned char *)devptr +
                                           block_size * dir_inode->direct[i]);
    // iterating over the block of dirents
    for (size_t j = 0; j < dirents_per_block; j++) {
      fs_dirent_t *entry = &dirents[j];
      if (entry->inode == FS_DIRENT_EMPTY)
        continue;
      // get inode of the child
      fs_inode_t *child = &itable[entry->inode];
      printf("\t%*s", depth * 4, ""); // print indent
      printf("%s", entry->name);
      if (child->type == FS_TYPE_DIR)
        printf("/"); // add trailing slash if its a dir
      printf("\n");

      if (strcmp(entry->name, ".") == 0 || strcmp(entry->name, "..") == 0) {
        continue;
      }
      if (child->type == FS_TYPE_DIR) {
        print_directory(itable, devptr, block_size, child, depth + 1);
      }
    }
  }
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "image_dump needs an image name to dump.\n");
    return -1;
  }
  char dev_name[1024];
  strncpy(dev_name, argv[1], sizeof(dev_name) - 1);
  dev_name[sizeof(dev_name) - 1] = '\0';

  fd_t file = open(dev_name, O_RDONLY);
  if (file == -1) {
    fprintf(stderr, "Error opening file.\n");
    return errno;
  }

  struct stat image_info;
  if (fstat(file, &image_info) == -1) {
    perror("Error fetching image stats.\n");
    return errno;
  }

  ssize_t image_size = image_info.st_size;

  void *mmap_ptr = mmap(NULL, image_size, PROT_READ, MAP_PRIVATE, file, 0);

  if (mmap_ptr == MAP_FAILED) {
    perror("Error mmap-ing iname.\n");
    return errno;
  }

  fs_superblock_t *superblock = (fs_superblock_t *)mmap_ptr;
  if (superblock->magic_number != FS_MAGIC_NUMBER) {
    errno = EINVAL;
    perror("Error, bad magic number.\n");
    return errno;
  }

  print_superblock(*superblock);

  fs_inode_t *itable =
      (fs_inode_t *)((unsigned char *)mmap_ptr +
                     superblock->block_size * superblock->inode_table_start);

  print_directory(itable, mmap_ptr, superblock->block_size, &itable[0], 0);

  return EXIT_SUCCESS;
}
