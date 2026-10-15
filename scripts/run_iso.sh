#!/bin/bash
qemu-system-i386 -m 256 -cdrom output/scratchos.iso -drive format=raw,file=output/disk.img -vga std -serial stdio
