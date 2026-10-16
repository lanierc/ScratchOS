import sys
import struct

# LAFS Ayarları
BLOCK_SIZE = 512
LBA_SUPERBLOCK = 0
LBA_INODES_START = 1
LBA_BITMAP = 17
LBA_DATA_START = 18
TOTAL_BLOCKS = 20480

def format_lafs(disk_path):
    with open(disk_path, "r+b") as disk:
        # 1. Her şeyi sıfırla (ilk 10MB)
        disk.seek(0)
        # Sadece başlıkları ve yapıları sıfırlasak yeter, DD ile zaten sıfırlanmış geliyor.

        # 2. Superblock Oluştur
        magic = 0xAF5
        total_blocks = TOTAL_BLOCKS
        total_inodes = 128
        free_blocks = TOTAL_BLOCKS - LBA_DATA_START
        free_inodes = 128 - 1 # Root inode kullanıldı
        root_inode = 0
        
        sb_data = struct.pack("<IIIIII", magic, total_blocks, total_inodes, free_blocks, free_inodes, root_inode)
        disk.seek(LBA_SUPERBLOCK * BLOCK_SIZE)
        disk.write(sb_data.ljust(BLOCK_SIZE, b'\x00'))

        # 3. Inodes Tablosunu Oluştur (Sıfırla)
        inodes_data = bytearray(128 * 64)
        
        # Root Inode (Type=2 (DIRECTORY), Size=1, blocks[0]=LBA_DATA_START)
        root_blocks = [LBA_DATA_START] + [0]*13
        inodes_data[0:64] = struct.pack("<II14I", 2, 0, *root_blocks)
        
        # Mnt Inode (Inode 1, Type=2, Size=0, blocks[0]=LBA_DATA_START + 1)
        mnt_blocks = [LBA_DATA_START + 1] + [0]*13
        inodes_data[64:128] = struct.pack("<II14I", 2, 0, *mnt_blocks)
        
        disk.seek(LBA_INODES_START * BLOCK_SIZE)
        disk.write(inodes_data)

        # 4. Bitmap Oluştur
        bitmap = bytearray(512)
        # LBA_DATA_START + 2 blok kullanıldı (0 to LBA_DATA_START + 1)
        for i in range(LBA_DATA_START + 2):
            byte_idx = i // 8
            bit_idx = i % 8
            bitmap[byte_idx] |= (1 << bit_idx)
            
        disk.seek(LBA_BITMAP * BLOCK_SIZE)
        disk.write(bitmap)

        # 5. Root Dizin Bloğunu Hazırla
        disk.seek(LBA_DATA_START * BLOCK_SIZE)
        root_dir = bytearray(BLOCK_SIZE)
        # Entry 0: "mnt" -> Inode 1
        struct.pack_into("<I28s", root_dir, 0, 1, b"mnt")
        disk.write(root_dir)
        
        # 6. Mnt Dizin Bloğunu Sıfırla
        disk.seek((LBA_DATA_START + 1) * BLOCK_SIZE)
        disk.write(b'\x00' * BLOCK_SIZE)

        print("LAFS formati basariyla tamamlandi (Varsayilan dizinler: /mnt).")

        print("LAFS formati basariyla tamamlandi.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Kullanim: python3 lafs_format.py <disk_img>")
    else:
        format_lafs(sys.argv[1])
