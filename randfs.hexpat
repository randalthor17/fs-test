#include <std/sys.pat>
#include <std/time.pat>

// Constants
#define FS_BLOCK_SIZE 4096
#define FS_MAGIC_NUMBER 0x52616E64
#define DIRECT_BLOCK_POINTER_PER_INODE 8
#define FS_MAX_FILENAME_SIZE 59
#define FS_DIRENT_EMPTY 0xFFFFFFFF

// Inode Type Enum
enum FsInodeType : u32 {
    FS_TYPE_FREE    = 0,
    FS_TYPE_FILE    = 1,
    FS_TYPE_DIR     = 2,
    FS_TYPE_SYMLINK = 3
};

// Superblock Struct
struct FsSuperblock {
    u32 magic_number [[format("hex_magic")]];
    u32 block_size;
    u32 num_blocks_total;
    u32 num_inodes;
    u32 inode_table_start;
    u32 num_inode_blocks;
    u32 bitmap_start;
    u32 data_start;
};

// Formatting functions
fn hex_magic(u32 val) {
    if (val == FS_MAGIC_NUMBER) {
        return std::format("0x{:08X} (Valid Rand-FS)", val);
    } else {
        return std::format("0x{:08X} (INVALID)", val);
    }
};

fn octal_mode(u32 mode) {
    return std::format("0o{:04o}", mode);
};

fn format_timestamp(u32 epoch) {
    if (epoch == 0) return "Not Set";
    return std::time::format(std::time::to_utc(epoch), "%Y-%m-%d %H:%M:%S UTC");
};

fn format_dirent_inode(u32 inode_idx) {
    if (inode_idx == FS_DIRENT_EMPTY) {
        return "EMPTY (-1)";
    }
    return std::format("Inode #{}", inode_idx);
};

// Inode Struct (64 Bytes)
struct FsInode {
    FsInodeType type;
    u32 size;
    u32 mode [[format("octal_mode")]];
    u32 created_at [[format("format_timestamp")]];
    u32 modified_at [[format("format_timestamp")]];
    u32 direct[DIRECT_BLOCK_POINTER_PER_INODE];
    u32 single_indirect;
    u32 double_indirect;
    u32 reserved;
};

// Directory Entry Struct (64 Bytes)
struct FsDirent {
    char name[FS_MAX_FILENAME_SIZE + 1];
    u32 inode [[format("format_dirent_inode")]];
};

// Helper struct to represent a 4KB Directory Data Block (64 Dirents)
struct DirectoryBlock {
    FsDirent entries[FS_BLOCK_SIZE / sizeof(FsDirent)];
};

// Main Layout Placement
FsSuperblock superblock @ 0x00;

// Helper struct to group the Inode Table neatly by Inodes
// (Referencing global `superblock` directly instead of `parent.superblock`)
struct InodeTable {
    FsInode inodes[superblock.num_inodes];
};

// Helper wrapper for Data Region (split into Directory Blocks)
struct DataRegion {
    DirectoryBlock directory_blocks[superblock.num_blocks_total - superblock.data_start];
};

// 1. Inode Table: Expands directly into individual `inodes[i]`
InodeTable inode_table @ (superblock.inode_table_start * superblock.block_size);

// 2. Block Bitmap
u8 block_bitmap[(superblock.data_start - superblock.bitmap_start) * superblock.block_size] @ (superblock.bitmap_start * superblock.block_size);

// 3. Data Blocks Region: Divided into Directory Blocks containing 64 `dirents` each
DataRegion data_blocks @ (superblock.data_start * superblock.block_size);