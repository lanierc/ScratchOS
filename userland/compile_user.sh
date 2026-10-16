#!/bin/bash
# Userland derleme betiği

# Klasörleri oluştur
mkdir -p bin

echo "[1/3] C kodlari derleniyor..."
gcc -m32 -c syscalls.c -o syscalls.o -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mno-sse -mno-sse2 -mno-mmx -Os
gcc -m32 -c hello.c -o hello.o -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mno-sse -mno-sse2 -mno-mmx -Os
gcc -m32 -c guess.c -o guess.o -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mno-sse -mno-sse2 -mno-mmx -Os
gcc -m32 -c vi.c -o vi.o -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mno-sse -mno-sse2 -mno-mmx -Os
gcc -m32 -c udp_test.c -o udp_test.o -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mno-sse -mno-sse2 -mno-mmx -Os
gcc -m32 -c tcp_test.c -o tcp_test.o -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mno-sse -mno-sse2 -mno-mmx -Os
gcc -m32 -c multi_tcp_test.c -o multi_tcp_test.o -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mno-sse -mno-sse2 -mno-mmx -Os

echo "[2/3] Linkleniyor (ELF32)..."
ld -m elf_i386 -T user.ld syscalls.o hello.o -o bin/hello.elf
ld -m elf_i386 -T user.ld syscalls.o guess.o -o bin/guess.elf
ld -m elf_i386 -T user.ld syscalls.o vi.o -o bin/vi.elf
ld -m elf_i386 -T user.ld syscalls.o udp_test.o -o bin/udp_test.elf
ld -m elf_i386 -T user.ld syscalls.o tcp_test.o -o bin/tcp_test.elf
ld -m elf_i386 -T user.ld syscalls.o multi_tcp_test.o -o bin/multi_tcp_test.elf

strip bin/*.elf

echo "[3/3] Disk imajina enjekte ediliyor..."
if [ -f ../output/disk.img ]; then
    python3 ../scripts/lafs_format.py ../output/disk.img
    python3 ../scripts/lafs_tool.py ../output/disk.img bin/hello.elf hello
    python3 ../scripts/lafs_tool.py ../output/disk.img bin/guess.elf guess
    python3 ../scripts/lafs_tool.py ../output/disk.img bin/vi.elf vi
    python3 ../scripts/lafs_tool.py ../output/disk.img bin/udp_test.elf udp_test
    python3 ../scripts/lafs_tool.py ../output/disk.img bin/tcp_test.elf tcp_test
    python3 ../scripts/lafs_tool.py ../output/disk.img bin/multi_tcp_test.elf multi_tcp_test
else
    echo "Hata: ../output/disk.img bulunamadı! Önce kernel'i derleyin."
fi

rm hello.o guess.o syscalls.o vi.o udp_test.o tcp_test.o multi_tcp_test.o
echo "Bitti! Simdi ScratchOS'u baslatip 'exec udp_test' yazabilirsiniz."
