#include "lafs.h"
#include "kheap.h"
#include "ata.h"
#include "vfs.h"
#include "spinlock.h"

extern void kprintf(const char* format, ...);

// Dosyanın üst kısmına ekleyin:
static void lafs_save_superblock(void);
static void lafs_save_bitmap(void);
static void lafs_save_inode(int ino);

static spinlock_t lafs_lock = SPINLOCK_INIT;
static int current_lafs_drive = 0;

// Bellek içi hızlı erişim işaretçileri
static struct lafs_superblock *sb;
static struct lafs_inode *inodes;
static unsigned char *block_bitmap; // 0 = boş, 1 = dolu

// Disk Düzeni (LBA Adresleri)
#define LBA_SUPERBLOCK 0
#define LBA_INODES_START 1
#define LBA_INODES_COUNT 16
#define LBA_BITMAP 17
#define LBA_DATA_START 18

// static struct vfs_dirent dirent; // Kaldirildi: Thread-safe degil
static vfs_node_t *lafs_root_node = 0;

// Bit işlemleri
static void lafs_bitmap_set(int bit) {
    block_bitmap[bit / 8] |= (1 << (bit % 8));
}
static void lafs_bitmap_clear(int bit) {
    block_bitmap[bit / 8] &= ~(1 << (bit % 8));
}
static int lafs_bitmap_test(int bit) {
    return block_bitmap[bit / 8] & (1 << (bit % 8));
}

extern int strcmp(const char *s1, const char *s2);
extern void *memcpy(void *dest, const void *src, int n);
extern void *memset(void *s, int c, int n);

// Ramdisk desteği (ATA olmayan donanımlar için - USB boot vb.)
static int lafs_ramdisk_mode = 0;
static unsigned char *ramdisk = 0;
#define RAMDISK_SIZE (LAFS_TOTAL_BLOCKS * LAFS_BLOCK_SIZE)

static void lafs_disk_read(unsigned int lba, unsigned char *buffer) {
    if (lafs_ramdisk_mode) {
        memcpy(buffer, ramdisk + (lba * 512), 512);
    } else {
        ata_read_sector(current_lafs_drive, lba, buffer);
    }
}

static void lafs_disk_write(unsigned int lba, unsigned char *buffer) {
    if (lafs_ramdisk_mode) {
        memcpy(ramdisk + (lba * 512), buffer, 512);
    } else {
        ata_write_sector(current_lafs_drive, lba, buffer);
    }
}

// Yardımcı: Boş bir inode bul
static int find_free_inode() {
    for (int i = 0; i < LAFS_MAX_INODES; i++) {
        if (inodes[i].type == LAFS_TYPE_FREE) {
            return i;
        }
    }
    return -1;
}

// Yardımcı: Boş bir blok bul ve dolu işaretle
static int alloc_block() {
    for (int i = LBA_DATA_START; i < LAFS_TOTAL_BLOCKS; i++) {
        if (!lafs_bitmap_test(i)) {
            lafs_bitmap_set(i);
            sb->free_blocks--;
            lafs_save_bitmap();
            lafs_save_superblock();
            return i;
        }
    }
    return -1;
}

// Superblock'ı diske kaydet
static void lafs_save_superblock() {
    lafs_disk_write(LBA_SUPERBLOCK, (unsigned char *)sb);
}

// Inode tablosunun belirli bir kısmını (1 sektör) diske kaydet
static void lafs_save_inode(int ino) {
    int sector_idx = ino / (512 / sizeof(struct lafs_inode));
    lafs_disk_write(LBA_INODES_START + sector_idx, (unsigned char *)inodes + (sector_idx * 512));
}

// Bitmap'i diske kaydet
static void lafs_save_bitmap() {
    lafs_disk_write(LBA_BITMAP, block_bitmap);
}

// Tüm Metadata'yı diske kaydet (Açılış/Format için)
static void save_metadata() {
    lafs_save_superblock();
    for (int i = 0; i < LBA_INODES_COUNT; i++) {
        lafs_disk_write(LBA_INODES_START + i, (unsigned char *)inodes + (i * 512));
    }
    lafs_save_bitmap();
}

// Diski biçimlendir
void lafs_format(void) {
    sb->magic = LAFS_MAGIC;
    sb->total_blocks = LAFS_TOTAL_BLOCKS;
    sb->total_inodes = LAFS_MAX_INODES;
    sb->free_blocks = LAFS_TOTAL_BLOCKS - LBA_DATA_START; 
    sb->free_inodes = LAFS_MAX_INODES;
    
    for (int i = 0; i < LAFS_MAX_INODES; i++) {
        inodes[i].type = LAFS_TYPE_FREE;
        inodes[i].size = 0;
        for (int j = 0; j < 14; j++) inodes[i].blocks[j] = 0;
    }
    for (int i = 0; i < 512; i++) block_bitmap[i] = 0;
    for (int i = 0; i < LBA_DATA_START; i++) {
        lafs_bitmap_set(i);
    }

    // Root dizini oluştur
    int root_idx = find_free_inode();
    inodes[root_idx].type = LAFS_TYPE_DIR;
    inodes[root_idx].size = 0;
    for (int i = 0; i < 14; i++) inodes[root_idx].blocks[i] = 0;
    
    // Root'un ilk bloğunu hemen ayır ve sıfırla (ls çöp görmesin diye)
    int b = alloc_block();
    inodes[root_idx].blocks[0] = b;
    unsigned char clear_data[512];
    memset(clear_data, 0, 512);
    lafs_disk_write(b, clear_data);
    
    sb->root_inode = root_idx;
    sb->free_inodes--;
    
    save_metadata();
    put_str("[OK] LAFS formatlandi. Kok dizini temiz.\n");
}

// LAFS Başlatma
void lafs_init(void) {
    spin_lock(&lafs_lock);
    sb = (struct lafs_superblock *)kmalloc(512); 
    inodes = (struct lafs_inode *)kmalloc(16 * 512);
    block_bitmap = (unsigned char *)kmalloc(512);

    // 1. ATA disklerinde LAFS ara
    current_lafs_drive = -1;
    lafs_ramdisk_mode = 0;
    char *drive_names[] = {"Primary Master", "Primary Slave", "Secondary Master", "Secondary Slave"};
    
    for (int i = 0; i < 4; i++) {
        memset(sb, 0, 512); 
        unsigned short port = (i < 2) ? 0x1F0 : 0x170;
        outb(port + 6, (i % 2 == 0) ? 0xE0 : 0xF0);
        for(int j=0; j<4; j++) inb(port + 7);
        
        if (inb(port + 7) == 0xFF) continue;

        ata_read_sector(i, LBA_SUPERBLOCK, (unsigned char *)sb);
        if (sb->magic == LAFS_MAGIC) {
            current_lafs_drive = i;
            kprintf("[LAFS] Dosya sistemi bulundu: %s\n", drive_names[i]);
            break;
        }
    }

    if (current_lafs_drive == -1) {
        // ATA'da LAFS bulunamadi - Ramdisk moduna gec
        kprintf("[LAFS] ATA disk bulunamadi, Ramdisk moduna geciliyor (2MB)\n");
        lafs_ramdisk_mode = 1;
        current_lafs_drive = 0;
        ramdisk = (unsigned char *)kmalloc(RAMDISK_SIZE);
        memset(ramdisk, 0, RAMDISK_SIZE);
        lafs_format();
    } else {
        // ATA'dan oku
        for (int i = 0; i < LBA_INODES_COUNT; i++) {
            lafs_disk_read(LBA_INODES_START + i, (unsigned char *)inodes + (i * 512));
        }
        lafs_disk_read(LBA_BITMAP, block_bitmap);
    }
    spin_unlock(&lafs_lock);
}

int lafs_mkdir(int parent_ino, const char *name) {
    return lafs_create(parent_ino, name, 1);
}

static int lafs_add_entry(int dir_ino, const char *name, int file_ino) {
    struct lafs_inode *dir = &inodes[dir_ino];
    if (dir->type != LAFS_TYPE_DIR) return -1;
    int max_entries_per_block = LAFS_BLOCK_SIZE / sizeof(struct lafs_dir_entry);
    unsigned char block_data[LAFS_BLOCK_SIZE];
    
    for (int i = 0; i < 14; i++) {
        if (dir->blocks[i] == 0) {
            int b = alloc_block();
            if (b == -1) return -1;
            dir->blocks[i] = b;
            memset(block_data, 0, LAFS_BLOCK_SIZE);
            lafs_disk_write(b, block_data);
        }
        lafs_disk_read(dir->blocks[i], block_data);
        struct lafs_dir_entry *entries = (struct lafs_dir_entry *)block_data;
        for (int j = 0; j < max_entries_per_block; j++) {
            if (entries[j].inode == 0) {
                entries[j].inode = file_ino;
                int k = 0;
                while (name[k] != '\0' && k < 27) {
                    entries[j].name[k] = name[k];
                    k++;
                }
                entries[j].name[k] = '\0';
                lafs_disk_write(dir->blocks[i], block_data);
                dir->size += sizeof(struct lafs_dir_entry);
                lafs_save_inode(dir_ino);
                return 0;
            }
        }
    }
    return -1;
}

static int lafs_find_entry(int dir_ino, const char *name) {
    struct lafs_inode *dir = &inodes[dir_ino];
    if (dir->type != LAFS_TYPE_DIR || dir->size == 0) return -1;
    int max_entries_per_block = LAFS_BLOCK_SIZE / sizeof(struct lafs_dir_entry);
    unsigned char block_data[LAFS_BLOCK_SIZE];
    for (int i = 0; i < 14; i++) {
        if (dir->blocks[i] == 0) break;
        lafs_disk_read(dir->blocks[i], block_data);
        struct lafs_dir_entry *entries = (struct lafs_dir_entry *)block_data;
        for (int j = 0; j < max_entries_per_block; j++) {
            if (entries[j].inode != 0 && strcmp(entries[j].name, name) == 0) {
                return entries[j].inode;
            }
        }
    }
    return -1;
}

// Bir dizin boş mu kontrol et ( . ve .. hariç )
int lafs_is_dir_empty(int ino) {
    struct lafs_inode *dir = &inodes[ino];
    if (dir->type != LAFS_TYPE_DIR) return 0;
    return (dir->size == 0); // Şimdilik . ve .. yok, o yüzden 0 ise boştur.
}

// Dizin girişini sil
static int lafs_remove_entry(int dir_ino, const char *name) {
    struct lafs_inode *dir = &inodes[dir_ino];
    int max_entries_per_block = LAFS_BLOCK_SIZE / sizeof(struct lafs_dir_entry);
    
    int target_block = -1;
    int target_idx = -1;
    unsigned char target_data[LAFS_BLOCK_SIZE];

    // 1. Hedef girdiyi bul
    for (int i = 0; i < 14; i++) {
        if (dir->blocks[i] == 0) break;
        lafs_disk_read(dir->blocks[i], target_data);
        struct lafs_dir_entry *entries = (struct lafs_dir_entry *)target_data;
        for (int j = 0; j < max_entries_per_block; j++) {
            if (entries[j].inode != 0 && strcmp(entries[j].name, name) == 0) {
                target_block = i;
                target_idx = j;
                goto found;
            }
        }
    }
    return -1;

found:
    // 2. Son girdiyi bul (Süreklilik için swap yapacağız)
    int last_entry_total_idx = (dir->size / sizeof(struct lafs_dir_entry)) - 1;
    int last_block_idx = last_entry_total_idx / max_entries_per_block;
    int last_entry_in_block = last_entry_total_idx % max_entries_per_block;
    
    unsigned char last_block_data[LAFS_BLOCK_SIZE];
    lafs_disk_read(dir->blocks[last_block_idx], last_block_data);
    struct lafs_dir_entry *last_entries = (struct lafs_dir_entry *)last_block_data;
    
    // 3. Swap yap (Eğer hedef son girdi değilse)
    struct lafs_dir_entry *target_entries = (struct lafs_dir_entry *)target_data;
    if (target_block != last_block_idx || target_idx != last_entry_in_block) {
        target_entries[target_idx] = last_entries[last_entry_in_block];
        lafs_disk_write(dir->blocks[target_block], target_data);
    }
    
    // 4. Son girdiyi sıfırla
    memset(&last_entries[last_entry_in_block], 0, sizeof(struct lafs_dir_entry));
    lafs_disk_write(dir->blocks[last_block_idx], last_block_data);
    
    // 5. Boyutu güncelle
    dir->size -= sizeof(struct lafs_dir_entry);
    lafs_save_inode(dir_ino);
    return 0;
}

int lafs_delete(int parent_ino, const char *name) {
    spin_lock(&lafs_lock);
    int ino = lafs_find_entry(parent_ino, name);
    if (ino == -1) {
        spin_unlock(&lafs_lock);
        return -1;
    }
    
    struct lafs_inode *file = &inodes[ino];
    
    // Klasör ise boş mu kontrol et
    if (file->type == LAFS_TYPE_DIR && !lafs_is_dir_empty(ino)) {
        spin_unlock(&lafs_lock);
        return -2; // Dizin dolu hatası
    }
    
    // 1. Blokları serbest bırak
    for (int i = 0; i < 14; i++) {
        if (file->blocks[i] != 0) {
            lafs_bitmap_clear(file->blocks[i]);
            sb->free_blocks++;
            file->blocks[i] = 0;
        }
    }
    
    // 2. Inode'u serbest bırak
    file->type = LAFS_TYPE_FREE;
    file->size = 0;
    sb->free_inodes++;
    
    // 3. Ebeveyn dizinden kaldır
    int res = lafs_remove_entry(parent_ino, name);
    lafs_save_superblock();
    lafs_save_bitmap();
    lafs_save_inode(ino);
    spin_unlock(&lafs_lock);
    return res;
}

int lafs_create(int parent_ino, const char *name, int is_dir) {
    spin_lock(&lafs_lock);
    if (sb->free_inodes == 0) {
        spin_unlock(&lafs_lock);
        return -1;
    }
    int dir_ino = parent_ino;
    if (lafs_find_entry(dir_ino, name) != -1) {
        spin_unlock(&lafs_lock);
        return -1;
    }
    int new_ino = find_free_inode();
    if (new_ino == -1) {
        spin_unlock(&lafs_lock);
        return -1;
    }
    inodes[new_ino].type = is_dir ? LAFS_TYPE_DIR : LAFS_TYPE_FILE;
    inodes[new_ino].size = 0;
    for(int i=0; i<14; i++) inodes[new_ino].blocks[i] = 0;
    sb->free_inodes--;
    if (lafs_add_entry(dir_ino, name, new_ino) == 0) {
        lafs_save_inode(new_ino);
        spin_unlock(&lafs_lock);
        return new_ino;
    }
    inodes[new_ino].type = LAFS_TYPE_FREE;
    sb->free_inodes++;
    spin_unlock(&lafs_lock);
    return -1;
}

int lafs_write(const char *name, const char *data, int len) {
    spin_lock(&lafs_lock);
    int ino = lafs_find_entry(sb->root_inode, name);
    if (ino == -1) {
        ino = lafs_create(sb->root_inode, name, 0);
        if (ino == -1) {
            spin_unlock(&lafs_lock);
            return -1;
        }
    }
    struct lafs_inode *file = &inodes[ino];
    if (file->type != LAFS_TYPE_FILE) {
        spin_unlock(&lafs_lock);
        return -1;
    }
    int bytes_written = 0;
    int block_idx = 0;
    unsigned char temp_buf[LAFS_BLOCK_SIZE];
    while (bytes_written < len && block_idx < 14) {
        if (file->blocks[block_idx] == 0) {
            int b = alloc_block();
            if (b == -1) break;
            file->blocks[block_idx] = b;
        }
        memset(temp_buf, 0, LAFS_BLOCK_SIZE);
        int to_write = len - bytes_written;
        if (to_write > LAFS_BLOCK_SIZE) to_write = LAFS_BLOCK_SIZE;
        memcpy(temp_buf, data + bytes_written, to_write);
        lafs_disk_write(file->blocks[block_idx], temp_buf);
        bytes_written += to_write;
        block_idx++;
    }
    file->size = bytes_written;
    lafs_save_inode(ino);
    spin_unlock(&lafs_lock);
    return bytes_written;
}

int lafs_read(const char *name, char *buffer, int max_len) {
    spin_lock(&lafs_lock);
    int ino = lafs_find_entry(sb->root_inode, name);
    if (ino == -1) {
        spin_unlock(&lafs_lock);
        return -1;
    }
    struct lafs_inode *file = &inodes[ino];
    if (file->type != LAFS_TYPE_FILE) {
        spin_unlock(&lafs_lock);
        return -1;
    }
    int total_to_read = file->size;
    if (total_to_read > max_len) total_to_read = max_len;
    int bytes_read = 0;
    int block_idx = 0;
    unsigned char temp_buf[LAFS_BLOCK_SIZE];
    while (bytes_read < total_to_read && block_idx < 14) {
        if (file->blocks[block_idx] == 0) break;
        lafs_disk_read(file->blocks[block_idx], temp_buf);
        int to_copy = total_to_read - bytes_read;
        if (to_copy > LAFS_BLOCK_SIZE) to_copy = LAFS_BLOCK_SIZE;
        memcpy(buffer + bytes_read, temp_buf, to_copy);
        bytes_read += to_copy;
        block_idx++;
    }
    if (bytes_read < max_len) buffer[bytes_read] = '\0';
    spin_unlock(&lafs_lock);
    return bytes_read;
}

static unsigned int lafs_vfs_read(vfs_node_t *node, unsigned int offset, unsigned int size, unsigned char *buffer) {
    spin_lock(&lafs_lock);
    struct lafs_inode *file = &inodes[node->inode];
    if (file->type != LAFS_TYPE_FILE || file->size == 0) {
        spin_unlock(&lafs_lock);
        return 0;
    }
    if (offset >= file->size) {
        spin_unlock(&lafs_lock);
        return 0;
    }
    if (offset + size > file->size) size = file->size - offset;
    int bytes_read = 0;
    int block_idx = offset / LAFS_BLOCK_SIZE;
    int block_offset = offset % LAFS_BLOCK_SIZE;
    unsigned char temp_buf[LAFS_BLOCK_SIZE];
    while (bytes_read < size && block_idx < 14) {
        lafs_disk_read(file->blocks[block_idx], temp_buf);
        int to_copy = LAFS_BLOCK_SIZE - block_offset;
        if (to_copy > (size - bytes_read)) to_copy = size - bytes_read;
        memcpy(buffer + bytes_read, temp_buf + block_offset, to_copy);
        bytes_read += to_copy;
        block_idx++;
        block_offset = 0;
    }
    spin_unlock(&lafs_lock);
    return bytes_read;
}

static unsigned int lafs_vfs_write(vfs_node_t *node, unsigned int offset, unsigned int size, unsigned char *buffer) {
    spin_lock(&lafs_lock);
    struct lafs_inode *file = &inodes[node->inode];
    if (file->type != LAFS_TYPE_FILE) {
        spin_unlock(&lafs_lock);
        return 0;
    }
    
    // LAFS sadece append modunu (veya tam blok baştan yazmayı) tam destekler. 
    // Basitlik adına, doğrudan dosyanın üzerine yazacağız. 
    // Eğer offset 0 ise ve size mevcut dosya boyutundan farklıysa boyut değişebilir.
    int bytes_written = 0;
    int block_idx = offset / LAFS_BLOCK_SIZE;
    int block_offset = offset % LAFS_BLOCK_SIZE;
    unsigned char temp_buf[LAFS_BLOCK_SIZE];
    
    while (bytes_written < size && block_idx < 14) {
        if (file->blocks[block_idx] == 0) {
            int b = alloc_block();
            if (b == -1) break;
            file->blocks[block_idx] = b;
        }
        
        // Eğer bloğun bir kısmına yazıyorsak, önce eski veriyi okumalıyız
        if (block_offset > 0 || (size - bytes_written) < LAFS_BLOCK_SIZE) {
            lafs_disk_read(file->blocks[block_idx], temp_buf);
        } else {
            memset(temp_buf, 0, LAFS_BLOCK_SIZE);
        }
        
        int to_write = LAFS_BLOCK_SIZE - block_offset;
        if (to_write > (size - bytes_written)) to_write = size - bytes_written;
        
        memcpy(temp_buf + block_offset, buffer + bytes_written, to_write);
        lafs_disk_write(file->blocks[block_idx], temp_buf);
        
        bytes_written += to_write;
        block_idx++;
        block_offset = 0;
    }
    
    // Boyut büyüdüyse güncelle
    if (offset + bytes_written > file->size) {
        file->size = offset + bytes_written;
    }
    
    lafs_save_inode(node->inode);
    spin_unlock(&lafs_lock);
    return bytes_written;
}

static void lafs_vfs_close(vfs_node_t *node) {
    if (node && node != lafs_root_node) {
        if (node->impl != 0) {
            kfree((void *)node->impl);
        }
        kfree(node);
    }
}

static struct vfs_dirent *lafs_vfs_readdir(vfs_node_t *node, unsigned int index) {
    // Statik dirent yerine heap veya stack kullanmaliyiz. 
    // Ancak readdir arayüzü pointer döndüğü için düğüm içinde bir tampon tutmak en temizi.
    // Şimdilik düğümün impl alanını veya ek bir alanı kullanabiliriz.
    // vfs_dirent 132 byte. vfs_node_t içinde yer yok. 
    // Bu yüzden geçici bir dirent allocate edip dönüyoruz (veya düğümün sonuna ekliyoruz).
    // Basitlik için düğümün 'impl' kısmını bir dirent pointer'ı olarak kullanalım.

    if (node->impl == 0) {
        node->impl = (unsigned int)kmalloc(sizeof(struct vfs_dirent));
    }
    struct vfs_dirent *d = (struct vfs_dirent *)node->impl;
    memset(d, 0, sizeof(struct vfs_dirent));

    spin_lock(&lafs_lock);
    struct lafs_inode *dir = &inodes[node->inode];
    if (dir->type != LAFS_TYPE_DIR || dir->size == 0) {
        spin_unlock(&lafs_lock);
        return 0;
    }
    int max_entries_per_block = LAFS_BLOCK_SIZE / sizeof(struct lafs_dir_entry);
    int block_idx = index / max_entries_per_block;
    int entry_in_block = index % max_entries_per_block;
    
    if (index >= 14 * max_entries_per_block) {
        spin_unlock(&lafs_lock);
        return 0;
    }

    if (block_idx >= 14 || dir->blocks[block_idx] == 0) {
        spin_unlock(&lafs_lock);
        return 0;
    }
    unsigned char dir_data[LAFS_BLOCK_SIZE];
    lafs_disk_read(dir->blocks[block_idx], dir_data);
    struct lafs_dir_entry *entries = (struct lafs_dir_entry *)dir_data;
    
    if (entries[entry_in_block].inode == 0) {
        spin_unlock(&lafs_lock);
        return 0;
    }

    d->ino = entries[entry_in_block].inode;
    int i = 0;
    while (entries[entry_in_block].name[i] != '\0' && i < 27) {
        d->name[i] = entries[entry_in_block].name[i];
        i++;
    }
    d->name[i] = '\0';
    
    spin_unlock(&lafs_lock);
    return d;
}

static vfs_node_t *lafs_vfs_finddir(vfs_node_t *node, char *name) {
    spin_lock(&lafs_lock);
    int ino = lafs_find_entry(node->inode, name);
    if (ino != -1) {
        vfs_node_t *res = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
        memset(res, 0, sizeof(vfs_node_t));
        struct lafs_inode *target_inode = &inodes[ino];
        int j = 0;
        while (name[j] != '\0' && j < 127) {
            res->name[j] = name[j];
            j++;
        }
        res->name[j] = '\0';
        res->inode = ino;
        res->length = target_inode->size;
        res->flags = (target_inode->type == LAFS_TYPE_DIR) ? VFS_DIRECTORY : VFS_FILE;
        res->read = lafs_vfs_read;
        res->write = lafs_vfs_write;
        res->readdir = lafs_vfs_readdir;
        res->finddir = lafs_vfs_finddir;
        res->close = lafs_vfs_close; // Close fonksiyonunu ata
        spin_unlock(&lafs_lock);
        return res;
    }
    spin_unlock(&lafs_lock);
    return 0;
}

vfs_node_t *lafs_get_vfs_root() {
    if (lafs_root_node) return lafs_root_node;
    lafs_root_node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    memset(lafs_root_node, 0, sizeof(vfs_node_t));
    lafs_root_node->name[0] = '/';
    lafs_root_node->name[1] = '\0';
    lafs_root_node->inode = sb->root_inode;
    lafs_root_node->flags = VFS_DIRECTORY;
    lafs_root_node->length = inodes[sb->root_inode].size;
    lafs_root_node->readdir = lafs_vfs_readdir;
    lafs_root_node->finddir = lafs_vfs_finddir;
    lafs_root_node->close = lafs_vfs_close; // Klonlanan root düğümlerin temizlenmesi için close atanıyor
    return lafs_root_node;
}
