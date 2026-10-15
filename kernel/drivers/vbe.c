#include "vbe.h"
#include "common.h"
#include "paging.h"
#include "pci.h"
#include "kheap.h"

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF
#define VBE_DISPI_INDEX_XRES   0x1
#define VBE_DISPI_INDEX_YRES   0x2
#define VBE_DISPI_INDEX_BPP    0x3
#define VBE_DISPI_INDEX_ENABLE   0x4
#define VBE_DISPI_INDEX_BANK     0x5
#define VBE_DISPI_INDEX_VIRT_W   0x6
#define VBE_DISPI_INDEX_VIRT_H   0x7
#define VBE_DISPI_INDEX_X_OFFSET 0x8
#define VBE_DISPI_INDEX_Y_OFFSET 0x9
#define VBE_DISPI_DISABLED       0x00
#define VBE_DISPI_ENABLED        0x01
#define VBE_DISPI_LFB_ENABLED    0x40

extern unsigned char font8x16[256][16];
#define VBE_VIRT_ADDR 0xE0000000

static vbe_info_t vbe_info;
static unsigned int* back_buffer = 0;
static unsigned int mouse_bg_save[64]; // Fare altindaki orijinal pikselleri saklamak icin

static void bga_write(unsigned short index, unsigned short data) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, data);
}

void vbe_init(struct multiboot_info *mb_info) {
    unsigned int lfb_phys = 0;
    int found = 0;
    
    // 1. Multiboot'tan Grafik Bilgilerini Al (En güvenilir yöntem)
    if (mb_info && (mb_info->flags & (1 << 12))) {
        // framebuffer_type: 0 = indexed, 1 = RGB, 2 = text
        if (mb_info->framebuffer_type == 1) {
            lfb_phys = (unsigned int)mb_info->framebuffer_addr;
            vbe_info.width = mb_info->framebuffer_width;
            vbe_info.height = mb_info->framebuffer_height;
            vbe_info.pitch = mb_info->framebuffer_pitch;
            vbe_info.bpp = mb_info->framebuffer_bpp;
            found = 1;
        }
    }
    
    // 2. PCI Üzerinden Grafik Kartını Tara (Fallback)
    if (!found) {
        put_str("[VBE] PCI uzerinden grafik karti araniyor...\n");
        for (int bus = 0; bus < 8; bus++) {
            for (int dev = 0; dev < 32; dev++) {
                unsigned short vendor = pci_config_read_word(bus, dev, 0, 0);
                if (vendor == 0xFFFF) continue;
                
                unsigned short class_sub = pci_config_read_word(bus, dev, 0, 10);
                if ((class_sub >> 8) == 0x03) { // Display Controller
                    unsigned short device = pci_config_read_word(bus, dev, 0, 2);
                    put_str("[VBE] Grafik karti bulundu: ");
                    put_hex(vendor); put_str(":"); put_hex(device); put_str("\n");

                    // BAR0 oku (LFB)
                    lfb_phys = (pci_config_read_word(bus, dev, 0, 0x12) << 16) | pci_config_read_word(bus, dev, 0, 0x10);
                    lfb_phys &= 0xFFFFFFF0;
                    
                    // BGA Mode Switch - SADECE QEMU/Bochs ise (Vendor 0x1234)
                    if (vendor == 0x1234 || vendor == 0x80EE) { // 0x80EE is VirtualBox
                        put_str("[VBE] BGA moduna geciliyor...\n");
                        bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
                        bga_write(VBE_DISPI_INDEX_XRES, 800);
                        bga_write(VBE_DISPI_INDEX_YRES, 600);
                        bga_write(VBE_DISPI_INDEX_BPP, 32);
                        bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
                        
                        vbe_info.width = 800;
                        vbe_info.height = 600;
                        vbe_info.pitch = 800 * 4;
                        vbe_info.bpp = 32;
                        found = 1;
                    } else {
                        put_str("[VBE] Gercek donanim algilandi. Multiboot mode gerekli.\n");
                        // Gerçek donanımda VBE modunu BIOS olmadan (Real Mode) değiştiremeyiz.
                        // Eğer Multiboot ile gelmediyse, mevcut modu kullanmaya çalışalım.
                        // Ama genişlik/yükseklik bilmediğimiz için güvenli değil.
                        found = 0;
                    }
                    if (found) break;
                }
            }
            if (found) break;
        }
    }
    
    if (!found || lfb_phys < 0x100000) { // 1MB altı veya bulunamadıysa iptal et
        vbe_set_active(0);
        return;
    }
    
    vbe_info.address = (unsigned int*)VBE_VIRT_ADDR;
    
    // 3. LFB'yi haritala
    paging_map_memory(lfb_phys, VBE_VIRT_ADDR, vbe_info.width * vbe_info.height * 4);
    
    // 4. Back Buffer için bellek ayır (Kernel Heap)
    if (!back_buffer) {
        back_buffer = (unsigned int*)kmalloc(vbe_info.width * vbe_info.height * 4);
    }
    
    if (!back_buffer) {
        // Heap yetmediyse acil durum mapping (0xD0000000)
        back_buffer = (unsigned int*)0xD0000000;
        paging_map_memory(0x02000000, 0xD0000000, vbe_info.width * vbe_info.height * 4);
    }

    vbe_set_active(1); // Artık güvenle aktif edebiliriz
}

void vbe_put_pixel(int x, int y, unsigned int color) {
    if (x < 0 || x >= vbe_info.width || y < 0 || y >= vbe_info.height || !back_buffer) return;
    back_buffer[y * vbe_info.width + x] = color;
}
void vbe_disable() {
    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_write(VBE_DISPI_INDEX_X_OFFSET, 0);
    bga_write(VBE_DISPI_INDEX_Y_OFFSET, 0);
    bga_write(VBE_DISPI_INDEX_XRES, 640);
    bga_write(VBE_DISPI_INDEX_YRES, 400);
    bga_write(VBE_DISPI_INDEX_BPP, 4);

    // CRTC Reset: Standart 80x25 Text Modu (80 sütun = 40 word offset)
    outb(0x3D4, 0x0C); outb(0x3D5, 0x00); // Start Address High = 0
    outb(0x3D4, 0x0D); outb(0x3D5, 0x00); // Start Address Low = 0
    outb(0x3D4, 0x13); outb(0x3D5, 0x28); // Offset (Stride): 40 words = 80 chars per line!
    outb(0x3D4, 0x14); outb(0x3D5, 0x00); // Underline / Byte mode
    outb(0x3D4, 0x17); outb(0x3D5, 0xA3); // Mode Control: standard text timing

    // Graphics Controller Miscellaneous Register: Belleği 0xB8000'a (32KB) haritala
    outb(0x3CE, 0x06);
    outb(0x3CF, 0x0E);

    vbe_set_active(0);
}
unsigned int vbe_get_pixel(int x, int y) {
    if (x < 0 || x >= vbe_info.width || y < 0 || y >= vbe_info.height) return 0;
    return back_buffer[y * vbe_info.width + x];
}

void vbe_update() {
    unsigned int *src = back_buffer;
    unsigned int *dest = vbe_info.address;
    int size = (vbe_info.width * vbe_info.height);
    
    // İşlemci seviyesinde blok kopyalama (rep movsd) kullanarak 
    // tek tek piksel kopyalamaktan 10-20 kat daha hızlı sonuç alırız.
    asm volatile (
        "cld\n"
        "rep movsl"
        :
        : "S"(src), "D"(dest), "c"(size)
        : "memory"
    );
}

// Belirli bir alani (rect) guncelle (Daha hizli!)
void vbe_update_rect(int x, int y, int w, int h) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            int px = x + j;
            int py = y + i;
            if (px < 0 || px >= vbe_info.width || py < 0 || py >= vbe_info.height) continue;
            vbe_info.address[py * vbe_info.width + px] = back_buffer[py * vbe_info.width + px];
        }
    }
}

void vbe_clear_screen(unsigned int color) {
    int size = (vbe_info.width * vbe_info.height);
    for(int i = 0; i < size; i++) {
        back_buffer[i] = color;
    }
}

void vbe_draw_gradient() {
    for (int y = 0; y < vbe_info.height; y++) {
        unsigned char g = (y * 40) / vbe_info.height;
        unsigned char b = (y * 80) / vbe_info.height;
        unsigned int color = (g << 8) | b;
        for (int x = 0; x < vbe_info.width; x++) {
            vbe_put_pixel(x, y, color);
        }
    }
}

void vbe_draw_rect(int x, int y, int w, int h, unsigned int color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            vbe_put_pixel(x + j, y + i, color);
        }
    }
}

void vbe_draw_char(int x, int y, char c, unsigned int color) {
    for (int i = 0; i < 16; i++) {
        unsigned char row = font8x16[(unsigned char)c][i];
        for (int j = 0; j < 8; j++) {
            if (row & (0x80 >> j)) {
                vbe_put_pixel(x + j, y + i, color);
            }
        }
    }
}

static int cursor_x = 0;
static int cursor_y = 0;

void vbe_write(const char *str, unsigned int color) {
    while (*str) {
        if (*str == '\n') {
            cursor_x = 0;
            cursor_y += 16;
        } else {
            vbe_draw_char(cursor_x, cursor_y, *str, color);
            cursor_x += 8;
            if (cursor_x >= vbe_info.width) {
                cursor_x = 0;
                cursor_y += 16;
            }
        }
        str++;
    }
}

void vbe_write_at(int x, int y, const char *str, unsigned int color) {
    int cur_x = x;
    while (*str) {
        vbe_draw_char(cur_x, y, *str, color);
        cur_x += 8;
        str++;
    }
}

static int last_cursor_x = -1;
static int last_cursor_y = -1;

void vbe_draw_cursor(int x, int y) {
    unsigned char cursor[8][8] = {
        {2,2,0,0,0,0,0,0},
        {2,1,2,0,0,0,0,0},
        {2,1,1,2,0,0,0,0},
        {2,1,1,1,2,0,0,0},
        {2,1,1,1,1,2,0,0},
        {2,1,1,2,2,2,0,0},
        {2,1,2,0,0,0,0,0},
        {2,2,0,0,0,0,0,0}
    };

    // 1. Eski konumu onar (Arka plani geri yukle)
    if (last_cursor_x != -1) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                unsigned int original_pixel = mouse_bg_save[i * 8 + j];
                vbe_put_pixel(last_cursor_x + j, last_cursor_y + i, original_pixel);
            }
        }
        vbe_update_rect(last_cursor_x, last_cursor_y, 8, 8);
    }

    // 2. Yeni konumdaki arka plani kaydet
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            mouse_bg_save[i * 8 + j] = vbe_get_pixel(x + j, y + i);
        }
    }

    // 3. Fare imlecini ciz
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (cursor[i][j] == 1) vbe_put_pixel(x + j, y + i, 0xFFFFFF);
            else if (cursor[i][j] == 2) vbe_put_pixel(x + j, y + i, 0x000000);
        }
    }

    vbe_update_rect(x, y, 8, 8);
    last_cursor_x = x;
    last_cursor_y = y;
}

// GUI için basit imleç (Save/Restore yapmaz, sadece back_buffer'a çizer)
void vbe_draw_cursor_simple(int x, int y) {
    unsigned char cursor[8][8] = {
        {2,2,0,0,0,0,0,0},
        {2,1,2,0,0,0,0,0},
        {2,1,1,2,0,0,0,0},
        {2,1,1,1,2,0,0,0},
        {2,1,1,1,1,2,0,0},
        {2,1,1,2,2,2,0,0},
        {2,1,2,0,0,0,0,0},
        {2,2,0,0,0,0,0,0}
    };

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (cursor[i][j] == 1) vbe_put_pixel(x + j, y + i, 0xFFFFFF);
            else if (cursor[i][j] == 2) vbe_put_pixel(x + j, y + i, 0x000000);
        }
    }
}

static int vbe_active = 0;

int vbe_is_active() {
    return vbe_active;
}

void vbe_set_active(int active) {
    vbe_active = active;
}
