#!/bin/bash

# Renk tanımlamaları
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}ScratchOS Git Push Scripti${NC}"
echo "--------------------------"

# Commit mesajı kontrolü
if [ -z "$1" ]; then
    echo -e "${RED}Hata: Bir commit mesajı girmelisiniz!${NC}"
    echo "Kullanım: ./push.sh \"commit mesajı\""
    exit 1
fi

COMMIT_MSG=$1

# Değişiklikleri ekle
echo -e "${BLUE}[1/3] Dosyalar ekleniyor...${NC}"
git add .

# Commit oluştur
echo -e "${BLUE}[2/3] Commit oluşturuluyor: ${NC}\"$COMMIT_MSG\""
git commit -m "$COMMIT_MSG"

# Push yap
echo -e "${BLUE}[3/3] GitHub'a gönderiliyor...${NC}"
git push

if [ $? -eq 0 ]; then
    echo -e "${GREEN}Başarılı! Değişiklikler gönderildi.${NC}"
else
    echo -e "${RED}Hata: Push işlemi başarısız oldu.${NC}"
fi
