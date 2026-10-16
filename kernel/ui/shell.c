#include "idt.h"
#include "shell.h"
#include "timer.h"
#include "pmm.h"
#include "paging.h"
#include "kheap.h"
#include "lafs.h"
#include "task.h"
#include "tss.h"
#include "syscall.h"
#include "pci.h"
#include "vbe.h"
#include "mouse.h"
#include "multiboot.h"
#include "vfs.h"

extern void stop_graphics();
extern struct multiboot_info* global_mbi;
extern void start_graphics(struct multiboot_info* mbi);
extern int elf_load(vfs_node_t *base, const char *filename);

// Metin fonksiyonları (kernel.c içinde tanımladık)
extern int strcmp(const char *s1, const char *s2);
extern int strlen(const char *s);
extern void clear_scr();

#define BUFFER_SIZE 256
static char input_buffer[BUFFER_SIZE];
static int buffer_idx = 0;

#define HISTORY_MAX 10
static char history_buffer[HISTORY_MAX][BUFFER_SIZE];
static int history_count = 0;
static int history_nav_idx = -1;
static char shell_cwd_path[128] = "/";
static vfs_node_t *shell_cwd_node = 0;
int shell_active = 1;

// Basit bir strncmp
static int strncmp(const char *s1, const char *s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

// Basit atoi: string -> int
static int atoi(const char *s) {
    int result = 0;
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return result;
}

void print_prompt() {
    put_str("lash ");
    put_str(shell_cwd_path);
    put_str("> ");
}

void init_shell() {
    buffer_idx = 0;
    history_count = 0;
    history_nav_idx = 0;
    shell_cwd_node = vfs_root;
    for(int i=0; i<BUFFER_SIZE; i++) input_buffer[i] = 0;
    print_prompt();
}

void execute_command(char* cmd) {
    if (strlen(cmd) == 0) return;

    if (strcmp(cmd, "help") == 0) {
        put_str("Komutlar:\n");
        put_str("  help     - Bu yardim mesajini goster\n");
        put_str("  clear    - Ekrani temizle\n");
        put_str("  version  - Sistem surumunu goster\n");
        put_str("  uptime   - Sistemin calisme suresini goster\n");
        put_str("  pci      - PCI aygitlarini tara ve listele\n");
        put_str("  ticks    - Zamanlayici tick sayisini goster\n");
        put_str("  mem      - Bellek istatistiklerini goster\n");
        put_str("  sleep N  - N milisaniye bekle (ornek: sleep 1000)\n");
        put_str("  echo ... - Mesaji ekrana yaz\n");
        put_str("  ls [path]- Dizini listele\n");
        put_str("  cd path  - Dizini degistir (.. desteklenir)\n");
        put_str("  mkdir p  - 'p' adinda dizin olustur\n");
        put_str("  rm f     - 'f' dosyasini sil\n");
        put_str("  rmdir d  - 'd' dizinini sil (bos olmali)\n");
        put_str("  touch f  - 'f' adinda dosya olustur\n");
        put_str("  write f t- 'f' dosyasina 't' metnini yaz\n");
        put_str("  cat f    - 'f' dosyasini oku\n");
        put_str("  exec f   - 'f' ELF dosyasini calistir\n");
        put_str("  vinit    - Gorsel modunu baslat\n");
        put_str("  format   - Diski bicimlendir (SIFIRLA)\n");
        put_str("  reboot   - Sistemi yeniden baslat\n");
        put_str("  shutdown   - Sistemi kapat\n");
    } 
    else if (strcmp(cmd, "clear") == 0) {
        clear_scr();
        extern unsigned int terminal_row, terminal_col;
        terminal_row = 0;
        terminal_col = 0;
    } 
    else if (strcmp(cmd, "version") == 0) {
        put_str("ScratchOS v0.2 - Shell: lash v0.1\n");
    }
    else if (strcmp(cmd, "uptime") == 0) {
        unsigned int total_secs = timer_get_seconds();
        unsigned int mins = total_secs / 60;
        unsigned int secs = total_secs % 60;
        put_str("Sistem calisme suresi: ");
        put_int(mins);
        put_str(" dakika, ");
        put_int(secs);
        put_str(" saniye (");
        put_int(timer_get_ticks());
        put_str(" tick)\n");
    }
    else if (strcmp(cmd, "pci") == 0) {
        pci_init();
    }
    else if (strcmp(cmd, "ticks") == 0) {
        put_str("Toplam tick: ");
        put_int(timer_get_ticks());
        put_str("\n");
    }
    else if (strcmp(cmd, "mem") == 0) {
        put_str("--- Bellek Istatistikleri ---\n");
        put_str("Toplam Bellek: ");
        put_int(pmm_total_memory_kb() / 1024);
        put_str(" MB (");
        put_int(pmm_total_memory_kb());
        put_str(" KB)\n");
        
        put_str("Toplam Frame (4KB): ");
        put_int(pmm_total_frames());
        put_str("\n");
        
        put_str("Kullanilan Frame: ");
        put_int(pmm_used_frames());
        put_str(" (");
        put_int((pmm_used_frames() * 4) / 1024);
        put_str(" MB)\n");
        
        put_str("Bos Frame: ");
        put_int(pmm_free_frames());
        put_str(" (");
        put_int((pmm_free_frames() * 4) / 1024);
        put_str(" MB)\n");
    }
    else if (strncmp(cmd, "sleep ", 6) == 0) {
        int ms = atoi(cmd + 6);
        if (ms <= 0) {
            put_str("Kullanim: sleep <milisaniye> (ornek: sleep 1000)\n");
        } else {
            put_str("Bekleniyor: ");
            put_int(ms);
            put_str(" ms...\n");
            sleep(ms);
            put_str("Tamamlandi.\n");
        }
    }
    else if (strncmp(cmd, "echo ", 5) == 0) {
        put_str(cmd + 5);
        put_str("\n");
    }
    else if (strcmp(cmd, "echo") == 0) {
        put_str("\n");
    }
    else if (strcmp(cmd, "vinit") == 0){
        if(vbe_is_active()){
            put_str("Gorsel mod zaten Aktif!\n");
        }
        else{
            put_str("Gorsel mod baslatiliyor...\n");
            start_graphics(global_mbi);
        }
    }
    else if (strcmp(cmd, "tui") == 0 || strcmp(cmd, "exit_gui") == 0) {
        if (!vbe_is_active()) {
            put_str("Zaten TUI (metin) modundasiniz.\n");
        } else {
            stop_graphics();
        }
    }
    else if (strncmp(cmd, "ls", 2) == 0) {
        char *path = ".";
        if (strlen(cmd) > 3 && cmd[2] == ' ') path = cmd + 3;
        
        vfs_node_t *dir = vfs_get_node_by_path(shell_cwd_node, path);

        if (dir && (dir->flags & VFS_DIRECTORY)) {
            int i = 0;
            struct vfs_dirent *dirent = 0;
            while ((dirent = vfs_readdir(dir, i))) {
                put_str(dirent->name);
                
                // Klasor olup olmadigini anlamak icin node'u bulalim
                vfs_node_t *file_node = vfs_finddir(dir, dirent->name);
                if (file_node) {
                    if (file_node->flags & VFS_DIRECTORY) {
                        put_str("/");
                    }
                    vfs_close(file_node);
                }
                
                put_str("  ");
                i++;
            }
            put_str("\n");
            if (dir != vfs_root && dir != shell_cwd_node) vfs_close(dir);
        } else {
            put_str("Hata: Dizin bulunamadi.\n");
        }
    }
    else if (strncmp(cmd, "mkdir ", 6) == 0) {
        char *dirname = cmd + 6;
        if (lafs_mkdir(shell_cwd_node->inode, dirname) != -1) {
            put_str("Dizin olusturuldu.\n");
        } else {
            put_str("Hata: Dizin olusturulamadi.\n");
        }
    }
    else if (strncmp(cmd, "touch ", 6) == 0) {
        char *filename = cmd + 6;
        if (strlen(filename) == 0) {
            put_str("Kullanim: touch <dosya_adi>\n");
        } else {
            int ino = lafs_create(shell_cwd_node->inode, filename, 0);
            if (ino != -1) {
                put_str("Dosya olusturuldu: "); put_str(filename); put_str("\n");
            } else {
                put_str("Hata: Dosya olusturulamadi.\n");
            }
        }
    }
    else if (strncmp(cmd, "cat ", 4) == 0) {
        char *path = cmd + 4;
        vfs_node_t *node = vfs_get_node_by_path(shell_cwd_node, path);

        if (node) {
            char buf[1024];
            int read_len = vfs_read(node, 0, 1023, (unsigned char *)buf);
            buf[read_len] = '\0';
            put_str(buf);
            put_str("\n");
            if (node != vfs_root && node != shell_cwd_node) vfs_close(node);
        } else {
            put_str("Hata: Dosya bulunamadi.\n");
        }
    }
    else if (strncmp(cmd, "exec ", 5) == 0) {
        char *filename = cmd + 5;
        if (strlen(filename) == 0) {
            put_str("Kullanim: exec <dosya_adi>\n");
        } else {
            int entry = elf_load(shell_cwd_node, filename);
            if (entry != -1) {
                put_str("Program baslatiliyor...\n");
                shell_active = 0;
                create_task(filename, (void (*)())entry, 1);
            } else {
                put_str("Hata: ELF yuklenemedi.\n");
            }
        }
    }
    else if (strncmp(cmd, "cd ", 3) == 0) {
        char *path = cmd + 3;
        if (strcmp(path, "..") == 0) {
            // Bir üst dizine çık (Basit path manipülasyonu)
            if (strcmp(shell_cwd_path, "/") != 0) {
                int len = strlen(shell_cwd_path);
                int i = len - 1;
                if (shell_cwd_path[i] == '/') i--;
                while (i >= 0 && shell_cwd_path[i] != '/') i--;
                if (i < 0) i = 0;
                shell_cwd_path[i + 1] = '\0';
                if (i == 0) shell_cwd_path[1] = '\0'; // "/" durumunu koru
                
                vfs_node_t *new_node = vfs_get_node_by_path(vfs_root, shell_cwd_path);
                if (new_node) {
                    if (shell_cwd_node != vfs_root) vfs_close(shell_cwd_node);
                    shell_cwd_node = new_node;
                }
            }
        } else {
            vfs_node_t *node = vfs_get_node_by_path(shell_cwd_node, path);
            if (node && (node->flags & VFS_DIRECTORY)) {
                if (shell_cwd_node != vfs_root) vfs_close(shell_cwd_node);
                shell_cwd_node = node;
                
                // Path guncelle
                if (path[0] == '/') {
                    int i = 0;
                    while(path[i]) { shell_cwd_path[i] = path[i]; i++; }
                    shell_cwd_path[i] = '\0';
                } else {
                    int len = strlen(shell_cwd_path);
                    if (shell_cwd_path[len-1] != '/') {
                        shell_cwd_path[len] = '/';
                        len++;
                    }
                    int i = 0;
                    while(path[i]) { shell_cwd_path[len+i] = path[i]; i++; }
                    shell_cwd_path[len+i] = '\0';
                }
            } else {
                put_str("Hata: Dizin bulunamadi.\n");
                if (node) vfs_close(node);
            }
        }
    }
    else if (strncmp(cmd, "rm ", 3) == 0) {
        char *filename = cmd + 3;
        if (lafs_delete(shell_cwd_node->inode, filename) == 0) {
            put_str("Dosya silindi.\n");
        } else {
            put_str("Hata: Dosya silinemedi.\n");
        }
    }
    else if (strncmp(cmd, "rmdir ", 6) == 0) {
        char *dirname = cmd + 6;
        int res = lafs_delete(shell_cwd_node->inode, dirname);
        if (res == 0) {
            put_str("Dizin silindi.\n");
        } else if (res == -2) {
            put_str("Hata: Dizin bos degil.\n");
        } else {
            put_str("Hata: Dizin silinemedi.\n");
        }
    }
    else if (strcmp(cmd, "reboot") == 0) {
        put_str("Sistem yeniden baslatiliyor...\n");
        // Klavye kontrolcüsü üzerinden reset (8042 port 0x64)
        outb(0x64, 0xFE);
    }
    else if (strcmp(cmd, "shutdown") == 0){
        put_str("Sistem Kapatılıyor...\n");
        outw(0x604, 0x2000);
    }
    else if (strcmp(cmd, "beep") == 0) {
        put_str("AC97 Ses Testi (Bip) baslatiliyor...\n");
        unsigned int buf_phys = alloc_frame();
        short *pcm = (short*)(buf_phys * 4096);
        for(int i = 0; i < 4096 / 2; i+=2) {
            short val = ((i / 50) % 2 == 0) ? 8000 : -8000;
            pcm[i] = val;   // Sol kanal
            pcm[i+1] = val; // Sag kanal
        }
        extern void ac97_play(unsigned char* pcm_data, unsigned int length);
        ac97_play((unsigned char*)pcm, 4096);
        put_str("Ses DMA'ya gonderildi.\n");
    }
    else if (strcmp(cmd, "format") == 0) {
        put_str("LAFS bicimlendiriliyor...\n");
        lafs_format();
        put_str("Disk temizlendi. Lutfen 'reboot' yaparak sistemi yenileyin.\n");
    }
    else {
        put_str("Hata: Bilinmeyen komut '");
        put_str(cmd);
        put_str("'. 'help' yazarak komutlari gorebilirsiniz.\n");
    }
}

void shell_input(char c) {
    if (vbe_is_active()) return;
    
    if (c == '\n') {
        put_char('\n');
        input_buffer[buffer_idx] = '\0';
        
        // Komut geçmişine kaydet
        if (buffer_idx > 0) {
            if (history_count == 0 || strcmp(history_buffer[history_count - 1], input_buffer) != 0) {
                if (history_count < HISTORY_MAX) {
                    for(int i = 0; i <= buffer_idx; i++) history_buffer[history_count][i] = input_buffer[i];
                    history_count++;
                } else {
                    for (int i = 0; i < HISTORY_MAX - 1; i++) {
                        for(int j = 0; j < BUFFER_SIZE; j++) history_buffer[i][j] = history_buffer[i+1][j];
                    }
                    for(int i = 0; i <= buffer_idx; i++) history_buffer[HISTORY_MAX - 1][i] = input_buffer[i];
                }
            }
        }
        
        execute_command(input_buffer);
        
        // Tamponu sıfırla
        buffer_idx = 0;
        for(int i=0; i<BUFFER_SIZE; i++) input_buffer[i] = 0;
        history_nav_idx = history_count;
        
        if (shell_active) {
            print_prompt();
        }
    } 
    else if (c == '\x11') { // Yukarı Ok (Önceki komut)
        if (history_count > 0 && history_nav_idx > 0) {
            history_nav_idx--;
            // Ekrandaki mevcut yazıyı sil
            while (buffer_idx > 0) {
                put_char('\b');
                buffer_idx--;
            }
            // Geçmişteki komutu yaz
            int i = 0;
            while (history_buffer[history_nav_idx][i]) {
                input_buffer[i] = history_buffer[history_nav_idx][i];
                put_char(input_buffer[i]);
                i++;
            }
            buffer_idx = i;
            input_buffer[buffer_idx] = '\0';
        }
    }
    else if (c == '\x12') { // Aşağı Ok (Sonraki komut)
        if (history_nav_idx < history_count) {
            history_nav_idx++;
            // Ekrandaki mevcut yazıyı sil
            while (buffer_idx > 0) {
                put_char('\b');
                buffer_idx--;
            }
            if (history_nav_idx == history_count) {
                buffer_idx = 0;
                input_buffer[0] = '\0';
            } else {
                int i = 0;
                while (history_buffer[history_nav_idx][i]) {
                    input_buffer[i] = history_buffer[history_nav_idx][i];
                    put_char(input_buffer[i]);
                    i++;
                }
                buffer_idx = i;
                input_buffer[buffer_idx] = '\0';
            }
        }
    }
    else if (c == '\b') {
        if (buffer_idx > 0) {
            buffer_idx--;
            input_buffer[buffer_idx] = 0;
            put_char('\b');
        }
    } 
    else {
        if (buffer_idx < BUFFER_SIZE - 1) {
            input_buffer[buffer_idx++] = c;
            put_char(c);
        }
    }
}
