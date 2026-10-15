#ifndef PAFS_H
#define PAFS_H

#include "common.h"

// PAFS (ScratchOS File System) Ayarları
#define PAFS_MAGIC       0xAF5
#define PAFS_BLOCK_SIZE  512
#define PAFS_MAX_INODES  128
#define PAFS_TOTAL_BLOCKS 4096 // 2 MB Sanal Disk

// Inode Tipleri
#define PAFS_TYPE_FREE 0
#define PAFS_TYPE_FILE 1
#define PAFS_TYPE_DIR  2

// Dosya Sistemi Üstverisi (Superblock)
struct pafs_superblock {
    unsigned int magic;
    unsigned int total_blocks;
    unsigned int total_inodes;
    unsigned int free_blocks;
    unsigned int free_inodes;
    unsigned int root_inode;
};

// Inode (Dosya/Klasör Meta Verisi)
struct pafs_inode {
    unsigned int type;           // 1: Dosya, 2: Klasör
    unsigned int size;           // Dosya boyutu
    unsigned int blocks[14];     // Veri blok numaraları (Maks 14 * 512 = 7KB dosya boyutu)
};

// Klasör Girdisi (Directory Entry)
struct pafs_dir_entry {
    unsigned int inode;          // Hangi inode'a ait?
    char name[28];               // Dosya/Klasör adı
};

// PAFS Fonksiyonları
void pafs_init(void);
void pafs_format(void);
int pafs_create(int parent_ino, const char *name, int is_dir);
int pafs_mkdir(int parent_ino, const char *name);
int pafs_delete(int parent_ino, const char *name);
int pafs_write(const char *name, const char *data, int len);
int pafs_read(const char *name, char *buffer, int max_len);
void pafs_list_dir(void);

#endif
