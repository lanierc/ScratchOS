#!/bin/bash

# Dosya yolları
KERNEL="output/scratchos.bin"
ISO="output/scratchos.iso"
ISODIR="isodir"

# Renkler
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}--- ScratchOS ISO Oluşturma Aracı ---${NC}"

# 1. Kontroller
if [ ! -f "$KERNEL" ]; then
    echo -e "${RED}Hata: $KERNEL bulunamadı!${NC}"
    echo "Lütfen önce çekirdeği derleyin: ./scripts/compile.sh"
    exit 1
fi

# grub-mkrescue kontrolü
if ! command -v grub-mkrescue &> /dev/null; then
    echo -e "${RED}Hata: 'grub-mkrescue' bulunamadı!${NC}"
    echo "Lütfen gerekli paketleri kurun: sudo apt install grub-common xorriso"
    exit 1
fi

# 2. ISO dizin yapısını temizle ve yeniden oluştur
echo "[1/4] ISO dizin yapısı hazırlanıyor..."
rm -rf $ISODIR
mkdir -p $ISODIR/boot/grub

# 3. Çekirdeği kopyala
echo "[2/4] Çekirdek kopyalanıyor..."
cp $KERNEL $ISODIR/boot/

# 4. grub.cfg oluştur
echo "[3/4] grub.cfg yapılandırılıyor..."
cat << EOF > $ISODIR/boot/grub/grub.cfg
set timeout=5
set default=0

insmod all_video
set gfxmode=800x600x32
set gfxpayload=800x600x32

menuentry "ScratchOS" {
    multiboot /boot/scratchos.bin
    boot
}
EOF

# 5. ISO dosyasını oluştur
echo "[4/4] ISO oluşturuluyor (grub-mkrescue)..."
if grub-mkrescue -o $ISO $ISODIR; then
    echo -e "${GREEN}------------------------------------------------${NC}"
    echo -e "${GREEN}Başarılı! ISO dosyası hazır: $ISO${NC}"
    echo "Bu dosyayı Rufus veya BalenaEtcher ile USB'ye yazdırabilirsiniz."
    echo -e "${GREEN}------------------------------------------------${NC}"
else
    echo -e "${RED}Hata: ISO oluşturulamadı!${NC}"
    exit 1
fi

# 6. Temizlik
rm -rf $ISODIR
