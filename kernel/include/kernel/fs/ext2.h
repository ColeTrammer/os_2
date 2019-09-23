#ifndef _KERNEL_FS_EXT2_H
#define _KERNEL_FS_EXT2_H 1

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/fs/file_system.h>
#include <kernel/fs/tnode.h>

#define EXT2_SUPER_BLOCK_OFFSET 1024
#define EXT2_SUPER_BLOCK_SIZE 1024

struct ext2_raw_super_block {
    uint32_t num_inodes;
    uint32_t num_blocks;
    uint32_t num_reserved_blocks;
    uint32_t num_unallocated_blocks;
    uint32_t num_unallocated_inodes;
    uint32_t super_block_block_number;
    uint32_t shifted_blck_size;
    uint32_t shifted_fragment_size;
    uint32_t num_blocks_in_block_group;
    uint32_t num_fragments_in_block_group;
    uint32_t num_inodes_in_block_group;
    uint32_t last_mount_time;
    uint32_t last_written_time;
    uint16_t num_fsck;
    uint16_t num_til_fsck;
    uint16_t ext2_sig;
#define EXT2_FS_STATE_CLEAN 1
#define EXT2_FS_STATE_ERROR 2
    uint16_t fs_state;
#define EXT2_FS_IGNORE_ERROR 1
#define EXT2_FS_REMOUNT_AS_READ_ONLY 2
#define EXT2_FS_KERNEL_PANIC 3
    uint16_t what_to_do_if_error;
    uint16_t version_minor;
    uint32_t fsck_time;
    uint32_t fsck_interval;
#define EXT2_CREATOR_LINUX 0
#define EXT2_CREATOR_GNU_HURD 1
#define EXT2_CREATOR_MASIX 2
#define EXT2_CREATOR_FREEBSD 3
#define EXT2_CREATOR_BSD_LITES 4
    uint32_t os_id;
    uint32_t version_major;
    uint16_t user_id_for_reserved_blocks;
    uint16_t group_id_for_reserved_blocks;
    uint32_t first_non_reserved_inode;
    uint16_t inode_size;
    uint16_t block_group_of_this_super_block;
#define EXT2_PREALLOCATE_BLOCKS_FOR_DIRECTORIES 1
#define EXT2_AFS_SERVER_INODES 2
#define EXT2_FS_JOURNAL 4
#define EXT2_EXTENDED_INODES 8
#define EXT2_FS_CAN_RESIZE 16
#define EXT2_DIRECTORIES_USE_HASH_INDEX 32
    uint32_t optional_features_present;
#define EXT2_COMPRESSION 1
#define EXT2_DIRECTORIES_HAVE_TYPE 2
#define EXT2_NEEDS_JOURNAL_REPLAY 4
#define EXT2_USES_JOURNAL_DEVICE 8
    uint32_t required_features_present;
#define EXT2_SPARSE_SUPERBLOCKS_AND_GROUP_DESCRIPTORS 1
#define EXT2_64_BIT_FILE_SIZE 2
#define EXT2_BINARY_TREE_DIRECTORIES 4
    uint32_t required_features_for_write;
    uint8_t fs_id[16];
    uint8_t volumne_name[16];
    uint8_t path_of_last_mount[64];
    uint32_t compression_algorithm;
    uint8_t num_blocks_to_preallocate_for_files;
    uint8_t num_blocks_to_preallocate_for_directories;
    uint8_t unused0[2];
    uint8_t journal_id[16];
    uint32_t journal_inode;
    uint32_t journal_device;
    uint32_t head_of_orphan_inode_list;
    uint8_t unused1[786];
} __attribute__((packed));

struct raw_block_group_descriptor {
    uint32_t block_usage_bitmap_block_address;
    uint32_t inode_usage_bitmap_block_address;
    uint32_t inode_table_block_address;
    uint16_t num_unallocated_blocks;
    uint16_t num_unallocated_inodes;
    uint16_t num_directories;
    uint8_t unused[24];
} __attribute__((packed));

struct tnode *ext2_lookup(struct inode *inode, const char *name);
struct file *ext2_open(struct inode *inode, int *error);
int ext2_close(struct file *file);
ssize_t ext2_read(struct file *file, void *buffer, size_t len);
ssize_t ext2_write(struct file *file, const void *buffer, size_t len);
int ext2_stat(struct inode *inode, struct stat *stat_struct);
struct tnode *ext2_mount(struct file_system *fs, char *device_path);

void init_ext2();

#endif /* _KERNEL_FS_EXT2_H */