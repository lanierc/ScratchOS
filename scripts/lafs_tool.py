import sys
import struct

# LAFS Ayarları
BLOCK_SIZE = 512
LBA_SUPERBLOCK = 0
LBA_INODES_START = 1
LBA_BITMAP = 17
LBA_DATA_START = 18

def add_file_to_lafs(disk_path, file_path, name_in_lafs):
    with open(disk_path, "r+b") as disk:
        # 1. Superblock oku
        disk.seek(LBA_SUPERBLOCK * BLOCK_SIZE)
        sb_data = disk.read(24)
        magic, total_blocks, total_inodes, free_blocks, free_inodes, root_inode = struct.unpack("<IIIIII", sb_data)
        
        if magic != 0xAF5:
            print("Hata: Gecersiz LAFS disk!")
            return

        # 2. Inode'ları oku
        disk.seek(LBA_INODES_START * BLOCK_SIZE)
        inodes_data = bytearray(disk.read(128 * 64)) # 128 inode * 64 byte

        # 3. Boş bir inode bul
        target_ino = -1
        for i in range(128):
            itype = struct.unpack_from("<I", inodes_data, i * 64)[0]
            if itype == 0: # LAFS_TYPE_FREE
                target_ino = i
                break
        
        if target_ino == -1:
            print("Hata: Bos inode kalmadi!")
            return

        # 4. Dosya verisini oku
        with open(file_path, "rb") as f:
            data = f.read()
        
        if len(data) > 14 * BLOCK_SIZE:
            print("Hata: Dosya 7KB'dan buyuk!")
            return

        # 5. Blokları ayır ve veriyi yaz
        num_blocks = (len(data) + BLOCK_SIZE - 1) // BLOCK_SIZE
        # Bitmap'i oku
        disk.seek(LBA_BITMAP * BLOCK_SIZE)
        bitmap = bytearray(disk.read(512))
        
        assigned_blocks = []
        for b in range(LBA_DATA_START, 4096):
            byte_idx = b // 8
            bit_idx = b % 8
            if not (bitmap[byte_idx] & (1 << bit_idx)):
                bitmap[byte_idx] |= (1 << bit_idx)
                assigned_blocks.append(b)
                if len(assigned_blocks) == num_blocks:
                    break
        
        if len(assigned_blocks) < num_blocks:
            print("Hata: Yeterli bos blok yok!")
            return

        # Veriyi diske yaz
        for i, b in enumerate(assigned_blocks):
            disk.seek(b * BLOCK_SIZE)
            chunk = data[i * BLOCK_SIZE : (i + 1) * BLOCK_SIZE]
            disk.write(chunk.ljust(BLOCK_SIZE, b'\x00'))

        # 6. Inode'u güncelle
        # struct lafs_inode: type, size, blocks[14]
        inode_struct = struct.pack("<II", 1, len(data)) # type=1 (FILE)
        block_list = struct.pack("<14I", *(assigned_blocks + [0]*(14-len(assigned_blocks))))
        inodes_data[target_ino * 64 : (target_ino + 1) * 64] = inode_struct + block_list

        # 7. Root dizinine (Inode 0) ekle
        root_itype, root_size = struct.unpack_from("<II", inodes_data, 0)
        root_blocks = struct.unpack_from("<14I", inodes_data, 0 + 8)
        
        # Root'un ilk bloğunu oku (Dir entries)
        disk.seek(root_blocks[0] * BLOCK_SIZE)
        dir_data = bytearray(disk.read(BLOCK_SIZE))
        
        # İlk olarak dosya zaten var mı diye kontrol edelim
        for i in range(BLOCK_SIZE // 32):
            ino_val = struct.unpack_from("<I", dir_data, i * 32)[0]
            if ino_val != 0:
                existing_name = struct.unpack_from("<28s", dir_data, i * 32 + 4)[0].split(b'\x00')[0].decode('ascii')
                if existing_name == name_in_lafs:
                    print(f"Hata: '{name_in_lafs}' adinda bir dosya zaten var!")
                    return
        
        # Boş bir giriş bul
        entry_found = False
        for i in range(BLOCK_SIZE // 32):
            ino_val = struct.unpack_from("<I", dir_data, i * 32)[0]
            if ino_val == 0: # Boş giriş
                # Inode ve Isim yaz
                name_bytes = name_in_lafs.encode('ascii')[:27] + b'\x00'
                dir_data[i * 32 : (i + 1) * 32] = struct.pack("<I28s", target_ino, name_bytes.ljust(28, b'\x00'))
                entry_found = True
                break
        
        if not entry_found:
            print("Hata: Root dizini dolu!")
            return

        # 8. Tüm değişiklikleri kaydet
        disk.seek(root_blocks[0] * BLOCK_SIZE)
        disk.write(dir_data)
        
        disk.seek(LBA_INODES_START * BLOCK_SIZE)
        disk.write(inodes_data)
        
        disk.seek(LBA_BITMAP * BLOCK_SIZE)
        disk.write(bitmap)
        
        # Superblock güncelle (free counts)
        sb_new = struct.pack("<IIIIII", magic, total_blocks, total_inodes, free_blocks - num_blocks, free_inodes - 1, root_inode)
        disk.seek(LBA_SUPERBLOCK * BLOCK_SIZE)
        disk.write(sb_new)
        
        # Root dizin boyutunu güncelle
        new_root_size = root_size + 32
        disk.seek(LBA_INODES_START * BLOCK_SIZE + 4) # Root inode size offset
        disk.write(struct.pack("<I", new_root_size))

        print(f"Basarili: '{file_path}' dosyasi '{name_in_lafs}' olarak eklendi.")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Kullanim: python3 lafs_tool.py <disk_img> <source_file> <dest_name>")
    else:
        add_file_to_lafs(sys.argv[1], sys.argv[2], sys.argv[3])
