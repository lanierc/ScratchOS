#!/bin/bash
set -e

# Renkler
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}==============================================${NC}"
echo -e "${GREEN}   ScratchOS ISO Derleme ve Başlatma Scripti  ${NC}"
echo -e "${BLUE}==============================================${NC}"

# Proje kök dizinine geç
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

# 1. Çekirdeği derle
echo -e "\n${YELLOW}[1/3] ScratchOS çekirdeği derleniyor...${NC}"
if make; then
    echo -e "${GREEN}✓ Çekirdek başarıyla derlendi.${NC}"
else
    echo -e "${RED}✗ Derleme hatası! Lütfen hataları kontrol edin.${NC}"
    exit 1
fi

# 2. ISO İmajını oluştur
echo -e "\n${YELLOW}[2/3] GRUB tabanlı ISO imajı oluşturuluyor...${NC}"
if bash scripts/make_iso.sh; then
    echo -e "${GREEN}✓ ISO imajı başarıyla oluşturuldu.${NC}"
else
    echo -e "${RED}✗ ISO oluşturulamadı!${NC}"
    exit 1
fi

# 3. QEMU ile ISO'yu başlat
echo -e "\n${YELLOW}[3/3] QEMU Sanal Makinesi ISO ile başlatılıyor...${NC}"
echo -e "${BLUE}QEMU Parametreleri: -cdrom output/scratchos.iso -drive output/disk.img -vga std${NC}\n"

qemu-system-i386 \
    -m 256 \
    -cdrom output/scratchos.iso \
    -drive format=raw,file=output/disk.img \
    -vga std \
    -serial stdio \
    -net nic,model=rtl8139 \
    -net user
