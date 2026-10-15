#!/bin/bash
# QEMU'yu başlat (Standart VGA/VBE)
qemu-system-i386 -m 256 -drive format=raw,file=output/disk.img -kernel output/scratchos.bin -vga std -net nic,model=rtl8139 -net user