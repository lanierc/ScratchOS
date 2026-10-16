# ScratchOS Makefile
# ==========================================

# Araçlar
CC      = gcc
LD      = ld
ASM     = nasm

# Bayraklar
CFLAGS  = -m32 -std=gnu99 -ffreestanding -O2 -Wall -fno-stack-protector \
          -fno-pie -fno-pic -mno-sse -mno-sse2 -mno-mmx -Iinclude
LDFLAGS = -m elf_i386 -T linker.ld
ASFLAGS = -f elf32

# Dizinler
OUTDIR  = output
BOOTDIR = boot

# Kaynak Dosyaları (kategorize edilmiş)
CORE_SRC    = kernel/core/kernel.c kernel/core/gdt.c kernel/core/idt.c \
              kernel/core/irq.c kernel/core/panic.c kernel/core/serial.c

MM_SRC      = kernel/mm/pmm.c kernel/mm/paging.c kernel/mm/kheap.c

FS_SRC      = kernel/fs/lafs.c kernel/fs/vfs.c kernel/fs/ext2.c

DRIVER_SRC  = kernel/drivers/ata.c kernel/drivers/keyboard.c kernel/drivers/mouse.c \
              kernel/drivers/timer.c kernel/drivers/pci.c kernel/drivers/vbe.c \
              kernel/drivers/font.c kernel/drivers/rtl8139.c kernel/drivers/e1000.c \
              kernel/drivers/ac97.c

NET_SRC     = kernel/net/net.c kernel/net/socket.c kernel/net/tcp.c

PROC_SRC    = kernel/proc/task.c kernel/proc/tss.c kernel/proc/syscall.c \
              kernel/proc/elf.c

UI_SRC      = kernel/ui/shell.c kernel/ui/gui.c

# Tüm C kaynakları
C_SOURCES   = $(CORE_SRC) $(MM_SRC) $(FS_SRC) $(DRIVER_SRC) $(NET_SRC) $(PROC_SRC) $(UI_SRC)

# Object dosyaları (output/ klasörüne düzleştirilmiş)
C_OBJECTS   = $(patsubst kernel/%.c,$(OUTDIR)/%.o,$(C_SOURCES))
ASM_OBJECTS = $(OUTDIR)/boot.o

# Tüm object dosyaları
OBJECTS     = $(ASM_OBJECTS) $(C_OBJECTS)

# Hedef
KERNEL      = $(OUTDIR)/scratchos.bin
DISK        = $(OUTDIR)/disk.img

# ==========================================
# Ana Hedefler
# ==========================================

.PHONY: all clean run iso

all: $(OUTDIR) $(KERNEL) $(DISK) userland_apps

userland_apps: $(DISK)
	@cd userland && bash ./compile_user.sh

$(OUTDIR):
	@mkdir -p $(OUTDIR)

# Assembly derleme
$(OUTDIR)/boot.o: $(BOOTDIR)/boot.s
	$(ASM) $(ASFLAGS) $< -o $@

# C derleme kuralı — alt dizinlerdeki .c'leri düzleştirilmiş .o'lara çevir
$(OUTDIR)/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Linkleme
$(KERNEL): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OUTDIR)/boot.o \
		$(OUTDIR)/core/kernel.o $(OUTDIR)/core/gdt.o $(OUTDIR)/core/idt.o \
		$(OUTDIR)/core/irq.o $(OUTDIR)/core/panic.o $(OUTDIR)/core/serial.o \
		$(OUTDIR)/mm/pmm.o $(OUTDIR)/mm/paging.o $(OUTDIR)/mm/kheap.o \
		$(OUTDIR)/fs/lafs.o $(OUTDIR)/fs/vfs.o $(OUTDIR)/fs/ext2.o \
		$(OUTDIR)/drivers/ata.o $(OUTDIR)/drivers/keyboard.o $(OUTDIR)/drivers/mouse.o \
		$(OUTDIR)/drivers/timer.o $(OUTDIR)/drivers/pci.o $(OUTDIR)/drivers/vbe.o \
		$(OUTDIR)/drivers/font.o $(OUTDIR)/drivers/rtl8139.o $(OUTDIR)/drivers/e1000.o \
		$(OUTDIR)/drivers/ac97.o \
		output/net/net.o output/net/socket.o output/net/tcp.o \
		$(OUTDIR)/proc/task.o $(OUTDIR)/proc/tss.o $(OUTDIR)/proc/syscall.o \
		$(OUTDIR)/proc/elf.o \
		$(OUTDIR)/ui/shell.o $(OUTDIR)/ui/gui.o

# Sanal Disk (yoksa oluştur)
$(DISK):
	dd if=/dev/zero of=$@ bs=1M count=10 status=none

# QEMU ile çalıştır
run: all
	qemu-system-i386 -m 256 -drive format=raw,file=$(DISK) \
		-drive format=raw,file=output/ext2.img \
		-kernel $(KERNEL) -vga std \
		-net nic,model=rtl8139 -net user

# ISO oluştur
iso: all
	@bash scripts/make_iso.sh

push: all
	@bash scripts/push.sh "This push was done with the Makefile!"
	
# Temizlik
clean:
	rm -rf $(OUTDIR)/*.o $(OUTDIR)/core $(OUTDIR)/mm $(OUTDIR)/fs \
	       $(OUTDIR)/drivers $(OUTDIR)/net $(OUTDIR)/proc $(OUTDIR)/ui \
	       $(KERNEL) output/disk.img
