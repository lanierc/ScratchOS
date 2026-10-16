#ifndef LAFS_H
#define LAFS_H

#include "common.h"

// LAFS (Lanier's Advanced File System) Ayarları
#define LAFS_MAGIC       0xAF5
#define LAFS_BLOCK_SIZE  512
#define LAFS_MAX_INODES  128
#define LAFS_TOTAL_BLOCKS 4096 // 2 MB Sanal Disk

// Inode Tipleri
#define LAFS_TYPE_FREE 0
#define LAFS_TYPE_FILE 1
#define LAFS_TYPE_DIR  2

// Dosya Sistemi Üstverisi (Superblock)
struct lafs_superblock {
    unsigned int magic;
    unsigned int total_blocks;
    unsigned int total_inodes;
    unsigned int free_blocks;
    unsigned int free_inodes;
    unsigned int root_inode;
};

// Inode (Dosya/Klasör Meta Verisi)
struct lafs_inode {
    unsigned int type;           // 1: Dosya, 2: Klasör
    unsigned int size;           // Dosya boyutu
    unsigned int blocks[14];     // Veri blok numaraları (Maks 14 * 512 = 7KB dosya boyutu)
};

// Klasör Girdisi (Directory Entry)
struct lafs_dir_entry {
    unsigned int inode;          // Hangi inode'a ait?
    char name[28];               // Dosya/Klasör adı
};

// LAFS Fonksiyonları
void lafs_init(void);
void lafs_format(void);
int lafs_create(int parent_ino, const char *name, int is_dir);
int lafs_mkdir(int parent_ino, const char *name);
int lafs_delete(int parent_ino, const char *name);
int lafs_write(const char *name, const char *data, int len);
int lafs_read(const char *name, char *buffer, int max_len);
void lafs_list_dir(void);
struct vfs_node *lafs_get_vfs_root(void);

// Geriye dönük uyumluluk takma adları (Aliases)
#define pafs_get_vfs_root lafs_get_vfs_root
#define pafs_init lafs_init
#define pafs_format lafs_format
#define pafs_create lafs_create
#define pafs_mkdir lafs_mkdir
#define pafs_delete lafs_delete
#define pafs_write lafs_write
#define pafs_read lafs_read
#define pafs_list_dir lafs_list_dir
#define pafs_superblock lafs_superblock
#define pafs_inode lafs_inode
#define pafs_dir_entry lafs_dir_entry
#define PAFS_MAGIC LAFS_MAGIC
#define PAFS_BLOCK_SIZE LAFS_BLOCK_SIZE
#define PAFS_MAX_INODES LAFS_MAX_INODES
#define PAFS_TOTAL_BLOCKS LAFS_TOTAL_BLOCKS
#define PAFS_TYPE_FREE LAFS_TYPE_FREE
#define PAFS_TYPE_FILE LAFS_TYPE_FILE
#define PAFS_TYPE_DIR LAFS_TYPE_DIR

#endif


