#!/bin/bash
# ScratchOS Derleme Betiği
# Çıktı klasörünü oluştur
mkdir -p output

# Eski dosyaları temizle
rm -f output/*.o output/scratchos.bin output/pekeros.bin

# Assembly
nasm -f elf32 boot/boot.s -o output/boot.o

# C Derleme - ÖNEMLI: -fno-pie -fno-pic bare-metal kernel için şart!
CFLAGS="-m32 -std=gnu99 -ffreestanding -O2 -Wall -fno-stack-protector -fno-pie -fno-pic -mno-sse -mno-sse2 -mno-mmx -Iinclude"

# --- Core ---
gcc $CFLAGS -c kernel/core/kernel.c  -o output/kernel.o
gcc $CFLAGS -c kernel/core/gdt.c     -o output/gdt.o
gcc $CFLAGS -c kernel/core/idt.c     -o output/idt.o
gcc $CFLAGS -c kernel/core/irq.c     -o output/irq.o
gcc $CFLAGS -c kernel/core/panic.c   -o output/panic.o
gcc $CFLAGS -c kernel/core/serial.c  -o output/serial.o

# --- Memory Management ---
gcc $CFLAGS -c kernel/mm/pmm.c       -o output/pmm.o
gcc $CFLAGS -c kernel/mm/paging.c    -o output/paging.o
gcc $CFLAGS -c kernel/mm/kheap.c     -o output/kheap.o

# --- Filesystem ---
gcc $CFLAGS -c kernel/fs/pafs.c      -o output/pafs.o
gcc $CFLAGS -c kernel/fs/vfs.c       -o output/vfs.o
gcc $CFLAGS -c kernel/fs/ext2.c      -o output/ext2.o

# --- Drivers ---
gcc $CFLAGS -c kernel/drivers/ata.c       -o output/ata.o
gcc $CFLAGS -c kernel/drivers/keyboard.c  -o output/keyboard.o
gcc $CFLAGS -c kernel/drivers/mouse.c     -o output/mouse.o
gcc $CFLAGS -c kernel/drivers/timer.c     -o output/timer.o
gcc $CFLAGS -c kernel/drivers/pci.c       -o output/pci.o
gcc $CFLAGS -c kernel/drivers/vbe.c       -o output/vbe.o
gcc $CFLAGS -c kernel/drivers/font.c      -o output/font.o
gcc $CFLAGS -c kernel/drivers/rtl8139.c   -o output/rtl8139.o
gcc $CFLAGS -c kernel/drivers/e1000.c     -o output/e1000.o
gcc $CFLAGS -c kernel/drivers/ac97.c      -o output/ac97.o

# --- Networking ---
gcc $CFLAGS -c kernel/net/net.c      -o output/net.o
gcc $CFLAGS -c kernel/net/socket.c   -o output/socket.o
gcc $CFLAGS -c kernel/net/tcp.c      -o output/tcp.o

# --- Process Management ---
gcc $CFLAGS -c kernel/proc/task.c    -o output/task.o
gcc $CFLAGS -c kernel/proc/tss.c     -o output/tss.o
gcc $CFLAGS -c kernel/proc/syscall.c -o output/syscall.o
gcc $CFLAGS -c kernel/proc/elf.c     -o output/elf.o

# --- UI ---
gcc $CFLAGS -c kernel/ui/shell.c     -o output/shell.o
gcc $CFLAGS -c kernel/ui/gui.c       -o output/gui.o

# Linkleme
ld -m elf_i386 -T linker.ld -o output/scratchos.bin \
    output/boot.o output/kernel.o output/gdt.o output/idt.o output/irq.o \
    output/panic.o output/serial.o \
    output/pmm.o output/paging.o output/kheap.o \
    output/pafs.o output/vfs.o output/ext2.o \
    output/ata.o output/keyboard.o output/mouse.o output/timer.o \
    output/pci.o output/vbe.o output/font.o output/rtl8139.o output/e1000.o \
    output/ac97.o \
    output/net.o output/socket.o output/tcp.o \
    output/task.o output/tss.o output/syscall.o output/elf.o \
    output/shell.o output/gui.o

# Sanal Disk Oluştur (10 MB, eğer yoksa)
if [ ! -f output/disk.img ]; then
    dd if=/dev/zero of=output/disk.img bs=1M count=10 status=none
fi

# Userland uygulamalarını derle ve disk imajına yükle
cd userland && bash ./compile_user.sh && cd ..