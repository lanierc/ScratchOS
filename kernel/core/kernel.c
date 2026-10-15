#include "idt.h"
#include "shell.h"
#include "timer.h"
#include "pmm.h"
#include "paging.h"
#include "kheap.h"
#include "pafs.h"
#include "task.h"
#include "tss.h"
#include "syscall.h"
#include "pci.h"
#include "vbe.h"
#include "gui.h"
#include "mouse.h"
#include "vfs.h"
#include "serial.h"
#include "net.h"

extern vfs_node_t *pafs_get_vfs_root();
extern void ata_read_sector(unsigned char drive, unsigned int lba, unsigned char *buffer);
extern void ata_write_sector(unsigned char drive, unsigned int lba, unsigned char *buffer);
extern void vbe_disable();
// Global değişkenler
unsigned int terminal_row = 0;
unsigned int terminal_col = 0;
char* const video_memory = (char*) 0xC00B8000;
struct multiboot_info* global_mbi = 0;

// Port I/O
void outb(unsigned short port, unsigned char val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

void outw(unsigned short port, unsigned short val) {
    asm volatile ( "outw %0, %1" : : "a"(val), "Nd"(port) );
}

unsigned short inw(unsigned short port) {
    unsigned short ret;
    asm volatile ( "inw %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

void vbe_init(struct multiboot_info *mb_info);
void vbe_put_pixel(int x, int y, unsigned int color);
void vbe_clear_screen(unsigned int color);
void vbe_draw_gradient();
void vbe_draw_rect(int x, int y, int w, int h, unsigned int color);

void outl(unsigned short port, unsigned int val) {
    asm volatile ( "outl %0, %1" : : "a"(val), "Nd"(port) );
}

unsigned int inl(unsigned short port) {
    unsigned int ret;
    asm volatile ( "inl %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// Ekran Fonksiyonları
void update_cursor() {
    unsigned short pos = terminal_row * 80 + terminal_col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char) (pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char) ((pos >> 8) & 0xFF));
}

void clear_scr() {
    for (int i = 0; i < 32768; i += 2) {
        video_memory[i] = ' ';
        video_memory[i+1] = 0x07;
    }
    terminal_row = 0;
    terminal_col = 0;
    update_cursor();
}

void scroll() {
    if (terminal_row >= 25) {
        for (int i = 0; i < 24 * 80 * 2; i++) {
            video_memory[i] = video_memory[i + 80 * 2];
        }
        for (int i = 24 * 80 * 2; i < 25 * 80 * 2; i += 2) {
            video_memory[i] = ' ';
            video_memory[i+1] = 0x07;
        }
        terminal_row = 24;
    }
}

extern int gui_term_is_redirected();
extern void gui_term_put_char(char c);

void put_char(char c) {
    // Tüm çıkışları seri porta da gönder (Debug için)
    serial_write(c);

    if (vbe_is_active()) {
        if (gui_term_is_redirected()) {
            gui_term_put_char(c);
            return;
        }
        char buf[2] = {c, 0};
        vbe_write(buf, 0x00FFFFFF);
        return;
    }
    if (c == '\n') {
        terminal_col = 0;
        terminal_row++;
    } else if (c == '\b') {
        if (terminal_col > 0) {
            terminal_col--;
            int pos = (terminal_row * 80 + terminal_col) * 2;
            video_memory[pos] = ' ';
            video_memory[pos+1] = 0x07;
        }
    } else {
        int pos = (terminal_row * 80 + terminal_col) * 2;
        video_memory[pos] = c;
        video_memory[pos+1] = 0x07;
        terminal_col++;
        if (terminal_col >= 80) {
            terminal_col = 0;
            terminal_row++;
        }
    }
    scroll();
    update_cursor();
}

void put_str(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        put_char(str[i]);
    }
}

void put_int(int n) {
    if (n == 0) {
        put_char('0');
        return;
    }
    if (n < 0) {
        put_char('-');
        n = -n;
    }
    char buf[12];
    int i = 0;
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    while (--i >= 0) {
        put_char(buf[i]);
    }
}

void put_hex(unsigned int n) {
    char *chars = "0123456789ABCDEF";
    put_str("0x");
    for (int i = 7; i >= 0; i--) {
        put_char(chars[(n >> (i * 4)) & 0xF]);
    }
}

// Gelişmiş Loglama ve Formatlı Yazdırma (Standart va_list kullanımı)
void kprintf(const char* format, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, format);
    
    char c;
    while ((c = *format++) != '\0') {
        if (c != '%') {
            put_char(c);
            continue;
        }
        
        c = *format++;
        if (c == 's') {
            char* s = __builtin_va_arg(args, char*);
            put_str(s);
        } else if (c == 'd') {
            int n = __builtin_va_arg(args, int);
            put_int(n);
        } else if (c == 'x') {
            unsigned int n = __builtin_va_arg(args, unsigned int);
            put_hex(n);
        } else if (c == 'c') {
            char ch = (char)__builtin_va_arg(args, int);
            put_char(ch);
        }
    }
    __builtin_va_end(args);
}

// String Fonksiyonları
int strlen(const char *s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, int n) {
    char *d = dest;
    while (n-- && (*d++ = *src++));
    while (n-- > 0) *d++ = '\0';
    return dest;
}

void *memcpy(void *dest, const void *src, int n) {
    char *d = dest;
    const char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memset(void *s, int c, int n) {
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

// Test Görevleri
extern unsigned int stack_top;

void task1_func() {
    while(1) {
        if (!vbe_is_active()) {
            ((char*)0xC00B8000)[158] = '1'; 
            ((char*)0xC00B8000)[159] = 0x0E; 
        }
        for(int i=0; i<1000000; i++) asm volatile("nop");
    }
}

void task2_func() {
    while(1) {
        if (!vbe_is_active()) {
            ((char*)0xC00B8000)[156] = '2';
            ((char*)0xC00B8000)[157] = 0x0B; 
        }
        for(int i=0; i<1000000; i++) asm volatile("nop");
    }
}

void graphics_task() {
    while(1) {
        if (!vbe_is_active()) {
            // TUI modundayken bekle ve CPU'yu rahatlat
            for(int i = 0; i < 50000; i++) asm volatile("nop");
            continue;
        }

        mouse_state_t* ms = mouse_get_state();
        gui_render();
        vbe_draw_cursor_simple(ms->x, ms->y);
        vbe_update();
        for(int i = 0; i < 10000; i++) asm volatile("nop");
    }
}
void stop_graphics() {
    if (!vbe_is_active()) return;
    
    vbe_disable();
    clear_scr();
    put_str("ScratchOS TUI (Metin) Moduna Donuldu.\n# ");
}

void user_mode_test() {
    while(1) {
        // Sistem çağrısı testi: syscall1(num, param1)
        syscall1(SYS_WRITE, (int)"[User] Ring 3 Syscall Merhaba!\n");
        
        for(int i=0; i<10000000; i++) asm volatile("nop");
    }
}

void udp_test_callback(void *data, int len, unsigned char *src_ip, unsigned short src_port) {
    char buf[128];
    if (len > 127) len = 127;
    memcpy(buf, data, len);
    buf[len] = '\0';
    
    kprintf("[UDP] %d.%d.%d.%d:%d -> '%s'\n", src_ip[0], src_ip[1], src_ip[2], src_ip[3], src_port, buf);
    
    // Echo: Geri ayni port'a gonder!
    char *reply = "ScratchOS UDP Echo Server: Mesaj alindi!\n";
    net_send_udp(src_ip, src_port, 1234, reply, strlen(reply));
}

static int graphics_task_started = 0;

void start_graphics(struct multiboot_info* mbi){
    if (vbe_is_active()) return;

    vbe_init(mbi);
    
    if (!vbe_is_active()) {
        put_str("[HATA] Grafik modu baslatilamadi!\n");
        return;
    }

    // GUI Sistemini başlat
    gui_init();
    mouse_init();
    
    if (!graphics_task_started) {
        create_task("graphics", graphics_task, 0);
        graphics_task_started = 1;
    }
}

// Kernel Giriş Noktası
static unsigned int ata_read_device1(vfs_node_t *node, unsigned int offset, unsigned int size, unsigned char *buffer) {
    // Drive 1 yoksa okuma yapma
    outb(0x1F6, 0xF0);
    for(int i=0; i<4; i++) inb(0x1F7);
    unsigned char st = inb(0x1F7);
    outb(0x1F6, 0xE0);
    if (st == 0xFF || st == 0x00) return 0;
    
    unsigned int start_sector = offset / 512;
    unsigned int end_sector = (offset + size - 1) / 512;
    unsigned char sector_buf[512];
    unsigned int read_bytes = 0;
    for (unsigned int s = start_sector; s <= end_sector; s++) {
        ata_read_sector(1, s, sector_buf);
        unsigned int off = (s == start_sector) ? (offset % 512) : 0;
        unsigned int len = (s == end_sector) ? ((offset + size - 1) % 512 - off + 1) : (512 - off);
        memcpy(buffer + read_bytes, sector_buf + off, len);
        read_bytes += len;
    }
    return read_bytes;
}

void kernel_main(unsigned int magic, struct multiboot_info* mbi) {
    global_mbi = mbi;
    
    // 0. Seri Portu Başlat (En önce!)
    init_serial();
    serial_print("\n--- ScratchOS Kernel Log Started ---\n");

    // 1. GDT ve IDT'yi kur
    init_gdt();
    init_idt();

    // 2. PIC'i yeniden haritala (tüm IRQ'lar kapalı başlar)
    irq_remap();

    // 3. Ekranı temizle ve karşılama mesajı göster
    put_str("\n\n");
    put_str("========================================\n");
    put_str("         ScratchOS v0.2 - Kernel\n");
    put_str("========================================\n\n");

    if (magic != MULTIBOOT_MAGIC) {
        put_str("[HATA] Gecersiz Multiboot Magic Number!\n");
        return;
    }

    kprintf("[OK] GDT yuklendi.\n");
    kprintf("[OK] IDT yuklendi.\n");
    kprintf("[OK] PIC yeniden haritalandi.\n");

    // 4. Fiziksel Bellek Yöneticisini (PMM) başlat
    init_pmm(mbi);
    kprintf("[OK] PMM baslatildi. Toplam Bellek: %d MB\n", pmm_total_memory_kb() / 1024);
    kprintf("[OK] Kullanilabilir Frame: %d / %d\n", pmm_free_frames(), pmm_total_frames());

    // 4.1. Sanal Bellek (Paging) başlat
    init_paging();

    // 4.2. Dinamik Bellek (Kernel Heap) başlat
    init_kheap();

    // Test: kmalloc ve kfree
    int *test_ptr1 = (int *)kmalloc(sizeof(int) * 10);
    int *test_ptr2 = (int *)kmalloc(200);
    put_str("[TEST] ptr1 tahsis edildi: 0x");
    put_hex((unsigned int)test_ptr1);
    put_str("\n[TEST] ptr2 tahsis edildi: 0x");
    put_hex((unsigned int)test_ptr2);
    put_str("\n[TEST] Bellekler serbest birakiliyor...\n");
    kfree(test_ptr1);
    put_str("[TEST] ptr1 serbest birakildi.\n");
    kfree(test_ptr2);
    put_str("[TEST] ptr2 serbest birakildi. Heap temiz.\n");

    put_str("[VFS] PAFS baslatiliyor...\n");
    pafs_init();
    put_str("[VFS] PAFS baslatildi.\n");
    
    put_str("[NET] TCP baslatiliyor...\n");
    tcp_init();
    put_str("[NET] TCP baslatildi.\n");

    // 5. Zamanlayıcıyı başlat (100 Hz = her 10ms'de bir tick)
    init_timer(100);
    put_str("[OK] Zamanlayici yuklendi (100 Hz).\n");


    // VFS Başlat
    vfs_root = pafs_get_vfs_root();
    put_str("[VFS] Kök dosya sistemi (PAFS) bağlandı.\n");

    // Faz 8: Mount test (VFS_ROOT artik hazir)
    // Önce Drive 1'in (ikinci ATA disk) var olup olmadığını kontrol et
    put_str("[VFS] /mnt klasoru aranıyor...\n");
    unsigned char drive1_status = inb(0x1F7); // Primary status
    outb(0x1F6, 0xF0); // Drive 1 (Slave) seç
    for(int i=0; i<4; i++) inb(0x1F7); // 400ns bekle
    drive1_status = inb(0x1F7);
    outb(0x1F6, 0xE0); // Drive 0'a geri dön
    for(int i=0; i<4; i++) inb(0x1F7);
    
    if (drive1_status != 0xFF && drive1_status != 0x00) {
        vfs_node_t *mnt_node = vfs_get_node_by_path(vfs_root, "/mnt");
        if (mnt_node) {
            vfs_node_t *dev_ata1 = kmalloc(sizeof(vfs_node_t));
            memset(dev_ata1, 0, sizeof(vfs_node_t));
            strcpy(dev_ata1->name, "ata1");
            dev_ata1->flags = VFS_FILE;
            dev_ata1->read = (void *)ata_read_device1;
            
            extern vfs_node_t *ext2_init(vfs_node_t *dev);
            vfs_node_t *ext2_root = ext2_init(dev_ata1);
            if (ext2_root) {
                vfs_mount("/mnt", ext2_root);
            }
        }
    } else {
        kprintf("[VFS] Drive 1 bulunamadi, EXT2 mount atlanıyor.\n");
    }

    // Donanim Kesfi
    pci_init();

    // UDP Test Sunucusu (Port 1234'u dinler)
    if (udp_bind(1234, udp_test_callback) == 0) {
        kprintf("[OK] UDP Echo Sunucusu port 1234'te baslatildi.\n");
    } else {
        kprintf("[HATA] UDP soketi olusturulamadi.\n");
    }

    // 4.4. Multitasking başlat
    init_multitasking();
    
    // TSS'yi ayarla (GDT index 5, Kernel Stack kullan)
    write_tss(5, 0x10, (unsigned int)&stack_top); 

    create_task("gorev1", task1_func, 0); // Kernel task
    create_task("gorev2", task2_func, 0); // Kernel task
    // create_task("user_task", user_mode_test, 1); // RING 3 TASK! (Su anlik kapali)


    // 5. Klavye sürücüsünü başlat (IRQ1 handler'ı kurar ve IRQ1'i açar)
    init_keyboard();
    put_str("[OK] Klavye surucusu yuklendi.\n");

    // 7. Shell'i başlat
    put_str("[OK] fash shell baslatildi.\n\n");
    init_syscalls();

    // Grafik gorevini baslat (Shell'den manuel baslatilacak)
    // start_graphics(mbi);

    put_str("\n--- SISTEM HAZIR ---\n");
    put_str("========================================\n");
    put_str("         ScratchOS v0.2 - Kernel\n");
    put_str("========================================\n\n");
    init_shell();
    
    // 7. Kesmeleri etkinleştir ve bekleme döngüsüne gir
    asm volatile("sti");

    for (;;) {
        asm volatile("hlt");
    }
}