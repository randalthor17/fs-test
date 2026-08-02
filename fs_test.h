// we declare the structure of the fs here

#ifndef FS_TEST_H
#define FS_TEST_H

// first, let's declare some properties:
//
#include <stdint.h>
#ifndef FS_BLOCK_SIZE
#define FS_BLOCK_SIZE 4096 // default block size
#endif                     // !FS_BLOCK_SIZE

#define FS_MAGIC_NUMBER 0x52616E64 // Rand-FS

#define DIRECT_BLOCK_POINTER_PER_INODE 8 // 8 is a good balance for a small FS

#define FS_MAX_FILENAME_SIZE 59 // 59 maximum characters in filename

#define FS_DIRENT_EMPTY ((uint32_t)-1) // 0xFFFFFFFF

// then is the superblock:
typedef struct {
  uint32_t magic_number;      // magic number for fs
  uint32_t block_size;        // block size
  uint32_t num_blocks_total;  // number of blocks in total
  uint32_t num_inodes;        // number of inodes
  uint32_t inode_table_start; // starting block index of inode table
  uint32_t num_inode_blocks;  // number of blocks in inode table
  uint32_t bitmap_start;      // starting block index of bitmap
  // NOTE: bitmap will have 0 for free and 1 for occupied
  uint32_t data_start; // starting block index of data
} fs_superblock_t;

// then is the inode type
// NOTE: Adding all these gets inode size to 64 bytes
typedef struct {
  uint32_t type;
  uint32_t size; // size of the file
  uint32_t mode; // this will have the permission octet, i presume
  uint32_t created_at;
  uint32_t modified_at;
  uint32_t direct[DIRECT_BLOCK_POINTER_PER_INODE]; // this stores block indices
  uint32_t single_indirect;
  uint32_t double_indirect;
  uint32_t reserved; // for padding
} fs_inode_t;

// NOTE:  1 for file, 2 for dir, 3 for symlink and so on, and 0 for empty
typedef enum {
  FS_TYPE_FREE = 0,
  FS_TYPE_FILE = 1,
  FS_TYPE_DIR = 2,
  FS_TYPE_SYMLINK = 3,
} fs_inode_type_t;

// then is the directory entry, i.e. a file entry in a directory
// NOTE: Setting MAX_FILENAME_SIZE gets dirent size to 64 bytes, perfect
typedef struct {
  char name[FS_MAX_FILENAME_SIZE + 1]; // filename
  uint32_t inode;                      // inode index corresponding to the file
  // NOTE: that we use -1 as indication that the directory is empty
} fs_dirent_t;

#endif // !FS_TEST_H
