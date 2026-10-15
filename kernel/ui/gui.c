#include "gui.h"
#include "common.h"
#include "kheap.h"
#include "vfs.h"
#include "pafs.h"

static void test_btn_click(gui_widget_t* btn);

static window_t *window_list_head = 0;
static window_t *window_list_tail = 0;
//Window Create Variables
static int window_offset = 0;
static int cascade_dir = 1;

/* ==================== GRAFIKSEL TERMINAL TAMPONU ==================== */
#define TERM_MAX_LINES 128
#define TERM_MAX_COLS   60
#define TERM_LINE_LEN   64

static char term_lines[TERM_MAX_LINES][TERM_LINE_LEN];
static int term_total_lines = 0;
static int term_scroll_offset = 0;
static char term_input[128];
static int term_input_len = 0;
static int term_cursor_blink = 0;
static window_t *term_window = 0;
static int term_redirect_active = 0;

int gui_term_is_redirected() {
    return term_redirect_active;
}

static void term_new_line() {
    if (term_total_lines < TERM_MAX_LINES) {
        term_total_lines++;
    } else {
        for (int i = 0; i < TERM_MAX_LINES - 1; i++) {
            memcpy(term_lines[i], term_lines[i + 1], TERM_LINE_LEN);
        }
    }
    memset(term_lines[term_total_lines - 1], 0, TERM_LINE_LEN);
    term_scroll_offset = 0;
}

void gui_term_put_char(char c) {
    if (term_total_lines == 0) {
        term_new_line();
    }
    
    if (c == '\n') {
        term_new_line();
        return;
    }
    
    if (c == '\r') {
        return; // CR atla
    }
    
    if (c == '\t') {
        // Tab karakterini 4 boşluğa çevir
        for (int i = 0; i < 4; i++) {
            gui_term_put_char(' ');
        }
        return;
    }
    
    if (c == '\b') {
        int cur_len = strlen(term_lines[term_total_lines - 1]);
        if (cur_len > 0) {
            term_lines[term_total_lines - 1][cur_len - 1] = '\0';
        }
        return;
    }
    
    int cur_len = strlen(term_lines[term_total_lines - 1]);
    if (cur_len < TERM_MAX_COLS) {
        term_lines[term_total_lines - 1][cur_len] = c;
        term_lines[term_total_lines - 1][cur_len + 1] = '\0';
    } else {
        // Satır pencere sınırına ulaştığında otomatik alt satıra geç (Word/Line wrap)
        term_new_line();
        term_lines[term_total_lines - 1][0] = c;
        term_lines[term_total_lines - 1][1] = '\0';
    }
}

void gui_term_put_str(const char *str) {
    while (*str) {
        gui_term_put_char(*str++);
    }
}

void gui_term_init(window_t *win) {
    term_window = win;
    term_total_lines = 0;
    term_scroll_offset = 0;
    term_input_len = 0;
    term_input[0] = '\0';
    
    gui_term_put_str("ScratchOS v0.2 Grafiksel Terminal\n");
    gui_term_put_str("Yardim icin 'help' yazabilirsiniz.\n");
    gui_term_put_str("----------------------------------------\n");
    gui_term_put_str("fash /> ");
}

void gui_term_scroll(int delta) {
    if (!term_window || !term_window->active) return;
    
    int visible_rows = (term_window->h - 36) / 16;
    int max_scroll = term_total_lines - visible_rows;
    if (max_scroll < 0) max_scroll = 0;
    
    // delta > 0: tekerlek yukari -> gecmise kaydir (scroll_offset artar)
    // delta < 0: tekerlek asagi -> bugune kaydir (scroll_offset azalir)
    term_scroll_offset += (delta * 2);
    
    if (term_scroll_offset > max_scroll) term_scroll_offset = max_scroll;
    if (term_scroll_offset < 0) term_scroll_offset = 0;
}

extern void execute_command(char* cmd);
void gui_bring_to_front(window_t *win);
static window_t *notepad_window = 0;
void gui_notepad_on_key(char c);

static void fm_strncpy(char *dst, const char *src, int n) {
    int i = 0;
    while (i < n - 1 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void gui_on_key_event(char c) {
    if (!window_list_tail) return;
    
    // En ondeki pencereye gore yonlendir
    if (window_list_tail == term_window && term_window != 0) {
        // Herhangi bir tusa basildiginda en alta odaklan
        term_scroll_offset = 0;
        
        if (c == '\n') {
            gui_term_put_char('\n');
            term_input[term_input_len] = '\0';
            
            if (term_input_len > 0) {
                if (strcmp(term_input, "clear") == 0) {
                    term_total_lines = 0;
                    term_scroll_offset = 0;
                } else {
                    term_redirect_active = 1;
                    execute_command(term_input);
                    term_redirect_active = 0;
                }
            }
            
            term_input_len = 0;
            term_input[0] = '\0';
            gui_term_put_str("fash /> ");
        } else if (c == '\b') {
            if (term_input_len > 0) {
                term_input_len--;
                term_input[term_input_len] = '\0';
                gui_term_put_char('\b');
            }
        } else if (c >= 32 && c <= 126) {
            if (term_input_len < 120) {
                term_input[term_input_len++] = c;
                term_input[term_input_len] = '\0';
                gui_term_put_char(c);
            }
        }
    } else if (window_list_tail == notepad_window && notepad_window != 0) {
        gui_notepad_on_key(c);
    }
}
/* ==================================================================== */

/* ==================== GRAFIKSEL NOT DEFTERI (NOTEPAD) ==================== */
#define NOTEPAD_MAX_SIZE 2048

static char notepad_buffer[NOTEPAD_MAX_SIZE];
static int notepad_len = 0;
static char notepad_filepath[128] = "/not.txt";
static char notepad_status[64] = "Yeni Belge";
static int notepad_cursor_blink = 0;

void gui_notepad_init(window_t *win, const char *filepath) {
    notepad_window = win;
    notepad_len = 0;
    notepad_buffer[0] = '\0';
    notepad_cursor_blink = 0;

    if (filepath && strlen(filepath) > 0) {
        fm_strncpy(notepad_filepath, filepath, 127);
        // VFS'den dosyayı oku
        vfs_node_t *node = vfs_get_node_by_path(vfs_root, notepad_filepath);
        if (node) {
            int to_read = node->length;
            if (to_read > NOTEPAD_MAX_SIZE - 1) to_read = NOTEPAD_MAX_SIZE - 1;
            int read_len = vfs_read(node, 0, to_read, (unsigned char*)notepad_buffer);
            if (read_len < 0) read_len = 0;
            notepad_buffer[read_len] = '\0';
            notepad_len = read_len;
            strcpy(notepad_status, "Dosya acildi");
            if (node != vfs_root) vfs_close(node);
        } else {
            strcpy(notepad_status, "Yeni Belge");
        }
    } else {
        strcpy(notepad_filepath, "/not.txt");
        strcpy(notepad_status, "Yeni Belge");
    }
}

void gui_notepad_save(void) {
    if (strlen(notepad_filepath) == 0) {
        strcpy(notepad_filepath, "/not.txt");
    }

    // Dosya mevcut mu kontrol et
    vfs_node_t *node = vfs_get_node_by_path(vfs_root, notepad_filepath);
    if (!node) {
        // Dosya adı "/" ile başlıyorsa adı ayıkla
        char *fname = notepad_filepath;
        if (fname[0] == '/') fname++;
        // PAFS create çağır
        pafs_create(vfs_root->inode, fname, 0);
        node = vfs_get_node_by_path(vfs_root, notepad_filepath);
    }

    if (node) {
        vfs_write(node, 0, notepad_len, (unsigned char*)notepad_buffer);
        strcpy(notepad_status, "Kaydedildi!");
        if (node != vfs_root) vfs_close(node);
    } else {
        strcpy(notepad_status, "Kayit hatasi!");
    }
}

void gui_notepad_clear(void) {
    notepad_len = 0;
    notepad_buffer[0] = '\0';
    strcpy(notepad_status, "Temizlendi");
}

void gui_notepad_on_key(char c) {
    if (c == '\b') {
        if (notepad_len > 0) {
            notepad_len--;
            notepad_buffer[notepad_len] = '\0';
            strcpy(notepad_status, "Duzenleniyor");
        }
    } else if (c == '\n') {
        if (notepad_len < NOTEPAD_MAX_SIZE - 1) {
            notepad_buffer[notepad_len++] = '\n';
            notepad_buffer[notepad_len] = '\0';
            strcpy(notepad_status, "Duzenleniyor");
        }
    } else if (c >= 32 && c <= 126) {
        if (notepad_len < NOTEPAD_MAX_SIZE - 1) {
            notepad_buffer[notepad_len++] = c;
            notepad_buffer[notepad_len] = '\0';
            strcpy(notepad_status, "Duzenleniyor");
        }
    }
}

void gui_notepad_on_click(int rel_x, int rel_y) {
    // Üst Araç Çubuğu (rel_y: 30 - 58)
    if (rel_y >= 30 && rel_y <= 58) {
        // [Kaydet] Butonu (x: 10 - 74)
        if (rel_x >= 10 && rel_x <= 74) {
            gui_notepad_save();
            return;
        }
        // [Temizle] Butonu (x: 80 - 144)
        if (rel_x >= 80 && rel_x <= 144) {
            gui_notepad_clear();
            return;
        }
    }
}

/* ==================== GRAFIKSEL DOSYA YONETICISI ==================== */
#define FM_MAX_ENTRIES 64

typedef struct {
    char name[64];
    int is_dir;
    unsigned int size;
} fm_entry_t;

static fm_entry_t fm_entries[FM_MAX_ENTRIES];
static int fm_entry_count = 0;
static char fm_current_path[128] = "/";
static int fm_scroll_offset = 0;
static window_t *fm_window = 0;
static int fm_selected_index = -1;

static void fm_strcat(char *dst, const char *src) {
    while (*dst) dst++;
    while (*src) {
        *dst++ = *src++;
    }
    *dst = '\0';
}

static void fm_format_size(unsigned int size, char *buf) {
    if (size == 0) {
        strcpy(buf, "0 B");
        return;
    }
    char temp[16];
    int idx = 0;
    unsigned int n = size;
    while (n > 0) {
        temp[idx++] = (n % 10) + '0';
        n /= 10;
    }
    int out = 0;
    for (int i = idx - 1; i >= 0; i--) {
        buf[out++] = temp[i];
    }
    buf[out++] = ' ';
    buf[out++] = 'B';
    buf[out] = '\0';
}

void gui_fm_open_path(const char *path) {
    if (!path) return;
    
    fm_strncpy(fm_current_path, path, 127);
    fm_entry_count = 0;
    fm_scroll_offset = 0;
    fm_selected_index = -1;

    vfs_node_t *dir = vfs_get_node_by_path(vfs_root, fm_current_path);
    if (!dir || !(dir->flags & VFS_DIRECTORY)) {
        return;
    }

    int i = 0;
    struct vfs_dirent *dirent = 0;
    while ((dirent = vfs_readdir(dir, i)) && fm_entry_count < FM_MAX_ENTRIES) {
        if (strcmp(dirent->name, ".") == 0) {
            i++;
            continue;
        }

        fm_strncpy(fm_entries[fm_entry_count].name, dirent->name, 63);

        vfs_node_t *file_node = vfs_finddir(dir, dirent->name);
        if (file_node) {
            fm_entries[fm_entry_count].is_dir = (file_node->flags & VFS_DIRECTORY) ? 1 : 0;
            fm_entries[fm_entry_count].size = file_node->length;
            if (file_node != vfs_root && file_node != dir) {
                vfs_close(file_node);
            }
        } else {
            fm_entries[fm_entry_count].is_dir = 0;
            fm_entries[fm_entry_count].size = 0;
        }

        fm_entry_count++;
        i++;
    }

    if (dir != vfs_root) {
        vfs_close(dir);
    }
}

static void fm_go_up() {
    if (strcmp(fm_current_path, "/") == 0) return;

    int len = strlen(fm_current_path);
    if (len > 0 && fm_current_path[len - 1] == '/') {
        fm_current_path[len - 1] = '\0';
        len--;
    }
    
    char *last_slash = 0;
    for (int i = len - 1; i >= 0; i--) {
        if (fm_current_path[i] == '/') {
            last_slash = &fm_current_path[i];
            break;
        }
    }

    if (last_slash) {
        if (last_slash == fm_current_path) {
            strcpy(fm_current_path, "/");
        } else {
            *last_slash = '\0';
        }
    } else {
        strcpy(fm_current_path, "/");
    }

    gui_fm_open_path(fm_current_path);
}

static void fm_open_entry(int index) {
    if (index < 0 || index >= fm_entry_count) return;

    if (fm_entries[index].is_dir) {
        char new_path[128];
        if (strcmp(fm_current_path, "/") == 0) {
            strcpy(new_path, "/");
            fm_strcat(new_path, fm_entries[index].name);
        } else {
            strcpy(new_path, fm_current_path);
            fm_strcat(new_path, "/");
            fm_strcat(new_path, fm_entries[index].name);
        }
        gui_fm_open_path(new_path);
    } else {
        fm_selected_index = index;
        // Dosyayi Notepad ile ac
        char file_path[128];
        if (strcmp(fm_current_path, "/") == 0) {
            strcpy(file_path, "/");
            fm_strcat(file_path, fm_entries[index].name);
        } else {
            strcpy(file_path, fm_current_path);
            fm_strcat(file_path, "/");
            fm_strcat(file_path, fm_entries[index].name);
        }
        if (!notepad_window || !notepad_window->active) {
            notepad_window = gui_create_window("Not Defteri", 220, 140, 480, 360, 0x008E44AD);
        }
        gui_notepad_init(notepad_window, file_path);
        gui_bring_to_front(notepad_window);
    }
}

void gui_fm_init(window_t *win) {
    fm_window = win;
    gui_fm_open_path("/");
}

void gui_fm_scroll(int delta) {
    if (delta > 0) {
        if (fm_scroll_offset > 0) fm_scroll_offset--;
    } else if (delta < 0) {
        if (fm_scroll_offset + 5 < fm_entry_count) fm_scroll_offset++;
    }
}

void gui_fm_on_click(int rel_x, int rel_y) {
    // Üst Bar Butonları (rel_y: 30 - 58)
    if (rel_y >= 30 && rel_y <= 58) {
        // [Yukari] Butonu (x: 12 - 76)
        if (rel_x >= 12 && rel_x <= 76) {
            fm_go_up();
            return;
        }
        // [Yenile] Butonu (x: 82 - 146)
        if (rel_x >= 82 && rel_x <= 146) {
            gui_fm_open_path(fm_current_path);
            return;
        }
    }

    // Dosya Listesi Alanı (rel_y: 64'ten itibaren, her satır 22px)
    if (rel_y >= 64) {
        int row = (rel_y - 64) / 22;
        int target_idx = row + fm_scroll_offset;
        if (target_idx >= 0 && target_idx < fm_entry_count) {
            fm_open_entry(target_idx);
        }
    }
}
/* ==================================================================== */

void gui_init() {
    window_list_head = 0;
    window_list_tail = 0;
}

void gui_destroy_window(window_t *win) {
    if (!win) return;
    if (win == term_window) {
        term_window = 0;
    }
    if (win == fm_window) {
        fm_window = 0;
    }
    if (win == notepad_window) {
        notepad_window = 0;
    }

    // Listeden cikar
    if (win->prev) win->prev->next = win->next;
    if (win->next) win->next->prev = win->prev;
    if (win == window_list_head) window_list_head = win->next;
    if (win == window_list_tail) window_list_tail = win->prev;

    kfree(win);
}

void gui_bring_to_front(window_t *win) {
    if (!win || win == window_list_tail) return;

    // Listeden cikar
    if (win->prev) win->prev->next = win->next;
    if (win->next) win->next->prev = win->prev;
    if (win == window_list_head) window_list_head = win->next;

    // En sona ekle
    win->next = 0;
    win->prev = window_list_tail;
    if (window_list_tail) window_list_tail->next = win;
    window_list_tail = win;
    if (!window_list_head) window_list_head = win;
}

void gui_on_mouse_event(int x, int y, int buttons, int scroll) {
    static int last_buttons = 0;
    static window_t *dragged_window = 0;
    static int drag_off_x = 0, drag_off_y = 0;

    // Mouse tekerleği ile kaydırma (scrolling)
    if (scroll != 0) {
        if (term_window && term_window->active) {
            if (x >= term_window->x && x <= term_window->x + term_window->w &&
                y >= term_window->y && y <= term_window->y + term_window->h) {
                gui_term_scroll(scroll);
            }
        }
        if (fm_window && fm_window->active) {
            if (x >= fm_window->x && x <= fm_window->x + fm_window->w &&
                y >= fm_window->y && y <= fm_window->y + fm_window->h) {
                gui_fm_scroll(scroll);
            }
        }
    }

    int left_click = (buttons & 1) && !(last_buttons & 1);
    int left_release = !(buttons & 1) && (last_buttons & 1);

    if (left_click) {
        // 1. Ikonlara mi tiklandi? (Sol taraf 0-100 arasi)
        if (x < 100) {
            if (y >= 40 && y <= 90) {
                window_t *w = gui_create_window("UI Test Paneli", 200, 150, 400, 250, 0x00F39C12);
                gui_create_label(w, 20, 40, "Merhaba ScratchOS GUI!", 0x002F3542);
                gui_create_button(w, 20, 80, 120, 30, "Tikla Bana", test_btn_click);
            }
            else if (y >= 110 && y <= 160) {
                if (!term_window || !term_window->active) {
                    term_window = gui_create_window("Terminal", 100, 100, 520, 360, 0x00000000);
                    gui_term_init(term_window);
                } else {
                    gui_bring_to_front(term_window);
                }
            }
            else if (y >= 180 && y <= 230) {
                if (!fm_window || !fm_window->active) {
                    fm_window = gui_create_window("Dosya Yoneticisi", 180, 100, 480, 340, 0x003498DB);
                    gui_fm_init(fm_window);
                } else {
                    gui_bring_to_front(fm_window);
                }
            }
            else if (y >= 320 && y <= 370) {
                if (!notepad_window || !notepad_window->active) {
                    notepad_window = gui_create_window("Not Defteri", 220, 140, 480, 360, 0x008E44AD);
                    gui_notepad_init(notepad_window, "/not.txt");
                } else {
                    gui_bring_to_front(notepad_window);
                }
            }
        }

        // 2. Pencerelere mi tiklandi? (En öndekinden ara)
        window_t *curr = window_list_tail;
        while (curr) {
            if (x >= curr->x && x <= curr->x + curr->w &&
                y >= curr->y && y <= curr->y + curr->h) {
                
                // Kapatma butonu...
                if (x >= curr->x + curr->w - 20 && x <= curr->x + curr->w - 5 &&
                    y >= curr->y + 5 && y <= curr->y + 20) {
                    gui_destroy_window(curr);
                    break;
                }

                gui_bring_to_front(curr);
                
                // Drag...
                if (y <= curr->y + 25) {
                    dragged_window = curr;
                    drag_off_x = x - curr->x;
                    drag_off_y = y - curr->y;
                } else if (curr == fm_window) {
                    int rel_x = x - curr->x;
                    int rel_y = y - curr->y;
                    gui_fm_on_click(rel_x, rel_y);
                } else if (curr == notepad_window) {
                    int rel_x = x - curr->x;
                    int rel_y = y - curr->y;
                    gui_notepad_on_click(rel_x, rel_y);
                } else {
                    // Widget click check
                    gui_widget_t *w = curr->widgets_head;
                    while (w) {
                        int wx = curr->x + w->x;
                        int wy = curr->y + w->y;
                        if (x >= wx && x <= wx + w->w && y >= wy && y <= wy + w->h) {
                            if (w->on_click) w->on_click(w);
                            break;
                        }
                        w = w->next;
                    }
                }
                break;
            }
            curr = curr->prev;
        }
    }

    if (left_release) {
        dragged_window = 0;
    }

    if (dragged_window && (buttons & 1)) {
        dragged_window->x = x - drag_off_x;
        dragged_window->y = y - drag_off_y;
    }

    last_buttons = buttons;
}

static void test_btn_click(gui_widget_t* btn) {
    if (strcmp(btn->text, "Tikla Bana") == 0) {
        strcpy(btn->text, "Tiklandi!");
        btn->bg_color = 0x002ED573; // Green
    } else {
        strcpy(btn->text, "Tikla Bana");
        btn->bg_color = 0x00DFE4EA; // Default
    }
}

window_t* gui_create_window(char* title, int x, int y, int w, int h, unsigned int color) {
    // 1. EKRANDA HİÇ PENCERE YOKSA:
    // Sıfırla; böylece ilk pencere her zaman tam kendi orijinal (x, y) konumunda açılır.
    if (window_list_head == 0) {
        window_offset = 0;
        cascade_dir = 1; // İlk pencereden sonrakiler sağa doğru başlasın
    }
    window_t *win = kmalloc(sizeof(window_t));
    memset(win, 0, sizeof(window_t));
    
    // 2. KOORDİNATLARI OFSETLİ ATA
    win->x = x + window_offset;
    win->y = y + (window_offset > 0 ? (window_offset / 2) : 0); // Y ekseninde de hafif aşağı insin istersen
    win->w = w; 
    win->h = h;
    win->color = color;
    win->active = 1;
    strcpy(win->title, title);
    // 3. BİR SONRAKİ PENCERE İÇİN PING-PONG HESAPLAMASI
    // Örneğin 150 piksel sağa gittikten sonra sola dönmeye başlasın:
    window_offset += (cascade_dir * 25);
    // Sağ sınıra ulaştı mı? (Örn: 150px veya ekran genişliğine göre kontrol)
    if (window_offset >= 150) {
        cascade_dir = -1; // Yönü SOLA çevir
    } 
    // Sola dönüp tekrar başlangıca (0'a) geldi mi?
    else if (window_offset <= 0) {
        window_offset = 0;
        cascade_dir = 1;  // Yönü tekrar SAĞA çevir
    }
    
    win->widgets_head = 0;
    win->widgets_tail = 0;

    // Listeye ekle (En öne)
    if (!window_list_head) {
        window_list_head = win;
        window_list_tail = win;
    } else {
        window_list_tail->next = win;
        win->prev = window_list_tail;
        window_list_tail = win;
    }

    return win; 
}

gui_widget_t* gui_create_button(window_t* win, int x, int y, int w, int h, char* text, void (*on_click)(gui_widget_t*)) {
    if (!win) return 0;
    gui_widget_t *btn = kmalloc(sizeof(gui_widget_t));
    memset(btn, 0, sizeof(gui_widget_t));
    
    btn->type = WIDGET_BUTTON;
    btn->x = x; btn->y = y; btn->w = w; btn->h = h;
    strcpy(btn->text, text);
    btn->bg_color = 0x00DFE4EA; 
    btn->fg_color = 0x002F3542; 
    btn->on_click = on_click;
    
    if (!win->widgets_head) {
        win->widgets_head = btn;
        win->widgets_tail = btn;
    } else {
        win->widgets_tail->next = btn;
        btn->prev = win->widgets_tail;
        win->widgets_tail = btn;
    }
    return btn;
}

gui_widget_t* gui_create_label(window_t* win, int x, int y, char* text, unsigned int color) {
    if (!win) return 0;
    gui_widget_t *lbl = kmalloc(sizeof(gui_widget_t));
    memset(lbl, 0, sizeof(gui_widget_t));
    
    lbl->type = WIDGET_LABEL;
    lbl->x = x; lbl->y = y; lbl->w = 0; lbl->h = 0;
    strcpy(lbl->text, text);
    lbl->fg_color = color;
    
    if (!win->widgets_head) {
        win->widgets_head = lbl;
        win->widgets_tail = lbl;
    } else {
        win->widgets_tail->next = lbl;
        lbl->prev = win->widgets_tail;
        win->widgets_tail = lbl;
    }
    return lbl;
}

gui_widget_t* gui_create_menu(window_t* win, int x, int y, int w, int h, char* text) {
    if (!win) return 0;
    gui_widget_t *menu = kmalloc(sizeof(gui_widget_t));
    memset(menu, 0, sizeof(gui_widget_t));
    menu->type = WIDGET_MENU;
    menu->x = x; menu->y = y; menu->w = w; menu->h = h;
    strcpy(menu->text, text);
    menu->bg_color = 0x00D3D3D3;
    menu->fg_color = 0x002F3542;
    // add to window widget list
    if (!win->widgets_head) {
        win->widgets_head = menu;
        win->widgets_tail = menu;
    } else {
        win->widgets_tail->next = menu;
        menu->prev = win->widgets_tail;
        win->widgets_tail = menu;
    }
    return menu;
}

gui_widget_t* gui_create_menu_item(window_t* win, int x, int y, int w, int h, char* text, void (*on_click)(gui_widget_t*)) {
    if (!win) return 0;
    gui_widget_t *item = kmalloc(sizeof(gui_widget_t));
    memset(item, 0, sizeof(gui_widget_t));
    item->type = WIDGET_MENU_ITEM;
    item->x = x; item->y = y; item->w = w; item->h = h;
    strcpy(item->text, text);
    item->bg_color = 0x00EAEAEA;
    item->fg_color = 0x002F3542;
    item->on_click = on_click;
    if (!win->widgets_head) {
        win->widgets_head = item;
        win->widgets_tail = item;
    } else {
        win->widgets_tail->next = item;
        item->prev = win->widgets_tail;
        win->widgets_tail = item;
    }
    return item;
}

void gui_draw_icon(int x, int y, char* label, unsigned int color) {
    // Ikon Govdesi (Sembolik kare)
    vbe_draw_rect(x + 5, y, 32, 32, color);
    vbe_draw_rect(x + 5, y, 32, 1, 0x00FFFFFF); // Ust parilti
    
    // Ikon Etiketi
    vbe_write_at(x, y + 35, label, 0x00FFFFFF);
}

void gui_draw_window(window_t* win) {
    if (!win->active) return;

    int is_terminal = (strcmp(win->title, "Terminal") == 0);

    // 0. Gölge Efekti (Daha yumusak)
    vbe_draw_rect(win->x + 3, win->y + 3, win->w, win->h, 0x002F3640); 

    // 1. Pencere Gövdesi ve Başlık Çubuğu
    if (is_terminal) {
        vbe_draw_rect(win->x, win->y, win->w, win->h, 0x00121212); // Koyu Siyah Terminal Arka Planı
        vbe_draw_rect(win->x, win->y, win->w, 25, 0x002C3E50);     // Koyu Başlık Çubuğu
        vbe_write_at(win->x + 10, win->y + 5, win->title, 0x00ECF0F1); // Beyaz Başlık Yazısı
    } else {
        vbe_draw_rect(win->x, win->y, win->w, win->h, 0x00F1F2F6);
        vbe_draw_rect(win->x, win->y, win->w, 25, 0x0074B9FF); 
        vbe_write_at(win->x + 10, win->y + 5, win->title, 0x002F3542);
    }
    
    // 3. Kapatma Butonu (Sağ üstte SkyOS tarzi)
    vbe_draw_rect(win->x + win->w - 20, win->y + 5, 15, 15, 0x00FF4757);
    
    // 4. Kenarlıklar (Soft Blue/Gray veya Koyu Kenarlık)
    unsigned int border_color = is_terminal ? 0x0034495E : 0x002F3542;
    vbe_draw_rect(win->x, win->y, win->w, 1, border_color);
    vbe_draw_rect(win->x, win->y + win->h - 1, win->w, 1, border_color);
    vbe_draw_rect(win->x, win->y, 1, win->h, border_color);
    vbe_draw_rect(win->x + win->w - 1, win->y, 1, win->h, border_color);

    // Eger Terminal penceresi ise metin tamponunu ve imleci ciz
    if (is_terminal) {
        int inner_x = win->x + 10;
        int inner_y = win->y + 32;
        int inner_h = win->h - 40;
        int visible_rows = inner_h / 16;
        int max_text_x = win->x + win->w - 18; // Sag sinir (scrollbar ve kenarlik payi)
        
        if (visible_rows > 0) {
            int end_line = term_total_lines - term_scroll_offset;
            int start_line = end_line - visible_rows;
            if (start_line < 0) start_line = 0;
            if (end_line > term_total_lines) end_line = term_total_lines;
            
            int cur_y = inner_y;
            for (int i = start_line; i < end_line; i++) {
                int cur_x = inner_x;
                const char *s = term_lines[i];
                while (*s && cur_x + 8 <= max_text_x) {
                    vbe_draw_char(cur_x, cur_y, *s, 0x00FFFFFF);
                    cur_x += 8;
                    s++;
                }
                cur_y += 16;
            }
            
            // Yanip sonen imlec (en alttaysak)
            if (term_scroll_offset == 0 && term_total_lines > 0) {
                term_cursor_blink = (term_cursor_blink + 1) % 40;
                if (term_cursor_blink < 20) {
                    int line_len = strlen(term_lines[term_total_lines - 1]);
                    int cursor_x = inner_x + (line_len * 8);
                    int cursor_y = cur_y - 16;
                    if (cursor_x + 8 <= max_text_x && cursor_y + 15 <= win->y + win->h - 6) {
                        vbe_draw_rect(cursor_x, cursor_y, 8, 15, 0x00FFFFFF);
                    }
                }
            }
            
            // Scrollbar (Dikey kaydirma cubugu)
            if (term_total_lines > visible_rows) {
                int sb_x = win->x + win->w - 9;
                int sb_y = win->y + 30;
                int sb_h = inner_h;
                vbe_draw_rect(sb_x, sb_y, 4, sb_h, 0x002F3542);
                
                int thumb_h = (visible_rows * sb_h) / term_total_lines;
                if (thumb_h < 15) thumb_h = 15;
                
                int max_scroll = term_total_lines - visible_rows;
                int thumb_y = sb_y + ((max_scroll - term_scroll_offset) * (sb_h - thumb_h)) / max_scroll;
                vbe_draw_rect(sb_x, thumb_y, 4, thumb_h, 0x002ED573);
            }
        }
        return; // Terminal bilesen icermez
    }
    
    int is_fm = (strcmp(win->title, "Dosya Yoneticisi") == 0);
    if (is_fm) {
        // 1. Üst Adres / Araç Çubuğu
        int bar_x = win->x + 8;
        int bar_y = win->y + 28;
        int bar_w = win->w - 16;
        int bar_h = 28;
        
        vbe_draw_rect(bar_x, bar_y, bar_w, bar_h, 0x00E2E8F0);
        vbe_draw_rect(bar_x, bar_y + bar_h - 1, bar_w, 1, 0x00CBD5E1);

        // [Yukari] Butonu
        vbe_draw_rect(bar_x + 4, bar_y + 3, 62, 22, 0x003B82F6);
        vbe_draw_rect(bar_x + 4, bar_y + 3, 62, 1, 0x0060A5FA);
        vbe_draw_rect(bar_x + 4, bar_y + 24, 62, 1, 0x001D4ED8);
        vbe_write_at(bar_x + 12, bar_y + 6, "Yukari", 0x00FFFFFF);

        // [Yenile] Butonu
        vbe_draw_rect(bar_x + 70, bar_y + 3, 62, 22, 0x0010B981);
        vbe_draw_rect(bar_x + 70, bar_y + 3, 62, 1, 0x0034D399);
        vbe_draw_rect(bar_x + 70, bar_y + 24, 62, 1, 0x00047857);
        vbe_write_at(bar_x + 78, bar_y + 6, "Yenile", 0x00FFFFFF);

        // Konum Metni (Adres Çubuğu)
        vbe_write_at(bar_x + 140, bar_y + 6, "Yol: ", 0x0064748B);
        vbe_write_at(bar_x + 176, bar_y + 6, fm_current_path, 0x001E293B);

        // 2. Dosya Listesi Alanı
        int list_x = win->x + 8;
        int list_y = bar_y + bar_h + 4;
        int list_w = win->w - 16;
        int list_h = win->h - (bar_h + 38);
        
        vbe_draw_rect(list_x, list_y, list_w, list_h, 0x00FFFFFF);
        vbe_draw_rect(list_x, list_y, list_w, 1, 0x00CBD5E1);
        vbe_draw_rect(list_x, list_y + list_h - 1, list_w, 1, 0x00CBD5E1);
        vbe_draw_rect(list_x, list_y, 1, list_h, 0x00CBD5E1);
        vbe_draw_rect(list_x + list_w - 1, list_y, 1, list_h, 0x00CBD5E1);

        int visible_items = list_h / 22;

        if (fm_entry_count == 0) {
            vbe_write_at(list_x + 20, list_y + 15, "(Dizin bos)", 0x0094A3B8);
        } else {
            for (int i = 0; i < visible_items; i++) {
                int idx = i + fm_scroll_offset;
                if (idx >= fm_entry_count) break;

                int item_y = list_y + 2 + (i * 22);

                // Seçili satır vurgusu
                if (idx == fm_selected_index) {
                    vbe_draw_rect(list_x + 2, item_y, list_w - 4, 20, 0x00E0E7FF);
                } else if (i % 2 == 1) {
                    vbe_draw_rect(list_x + 2, item_y, list_w - 4, 20, 0x00F8FAFC);
                }

                if (fm_entries[idx].is_dir) {
                    // Klasör simgesi (Sarı/Turuncu küçük dosya klasörü)
                    vbe_draw_rect(list_x + 8, item_y + 4, 16, 12, 0x00F59E0B);
                    vbe_draw_rect(list_x + 10, item_y + 2, 8, 3, 0x00D97706);
                    
                    // Klasör adı
                    char dir_display[68];
                    strcpy(dir_display, fm_entries[idx].name);
                    fm_strcat(dir_display, "/");
                    vbe_write_at(list_x + 30, item_y + 2, dir_display, 0x00B45309);

                    // Sağda [DIR] etiketi
                    vbe_write_at(list_x + list_w - 75, item_y + 2, "<DIR>", 0x00D97706);
                } else {
                    // Normal dosya simgesi (Mavi/Gri yaprak simgesi)
                    vbe_draw_rect(list_x + 9, item_y + 3, 12, 14, 0x0094A3B8);
                    vbe_draw_rect(list_x + 10, item_y + 4, 10, 12, 0x00F1F5F9);
                    
                    // Dosya adı
                    vbe_write_at(list_x + 30, item_y + 2, fm_entries[idx].name, 0x001E293B);

                    // Dosya boyutu
                    char size_str[24];
                    fm_format_size(fm_entries[idx].size, size_str);
                    vbe_write_at(list_x + list_w - 75, item_y + 2, size_str, 0x0064748B);
                }
            }

            // Scrollbar (Eğer liste sığmıyorsa)
            if (fm_entry_count > visible_items) {
                int sb_x = list_x + list_w - 6;
                int sb_y = list_y + 2;
                int sb_h = list_h - 4;
                vbe_draw_rect(sb_x, sb_y, 4, sb_h, 0x00E2E8F0);

                int thumb_h = (visible_items * sb_h) / fm_entry_count;
                if (thumb_h < 12) thumb_h = 12;

                int max_scroll = fm_entry_count - visible_items;
                int thumb_y = sb_y + (fm_scroll_offset * (sb_h - thumb_h)) / max_scroll;
                vbe_draw_rect(sb_x, thumb_y, 4, thumb_h, 0x003B82F6);
            }
        }
        return; // Dosya yöneticisi çizildi
    }

    int is_notepad = (strcmp(win->title, "Not Defteri") == 0);
    if (is_notepad) {
        // 1. Üst Araç Çubuğu
        int bar_x = win->x + 8;
        int bar_y = win->y + 28;
        int bar_w = win->w - 16;
        int bar_h = 28;

        vbe_draw_rect(bar_x, bar_y, bar_w, bar_h, 0x00E2E8F0);
        vbe_draw_rect(bar_x, bar_y + bar_h - 1, bar_w, 1, 0x00CBD5E1);

        // [Kaydet] Butonu
        vbe_draw_rect(bar_x + 4, bar_y + 3, 62, 22, 0x008E44AD); // Mor
        vbe_draw_rect(bar_x + 4, bar_y + 3, 62, 1, 0x00A569BD);
        vbe_draw_rect(bar_x + 4, bar_y + 24, 62, 1, 0x006C3483);
        vbe_write_at(bar_x + 12, bar_y + 6, "Kaydet", 0x00FFFFFF);

        // [Temizle] Butonu
        vbe_draw_rect(bar_x + 70, bar_y + 3, 62, 22, 0x00E74C3C); // Kırmızı
        vbe_draw_rect(bar_x + 70, bar_y + 3, 62, 1, 0x00EC7063);
        vbe_draw_rect(bar_x + 70, bar_y + 24, 62, 1, 0x00C0392B);
        vbe_write_at(bar_x + 74, bar_y + 6, "Temizle", 0x00FFFFFF);

        // Dosya Yolu & Durum
        vbe_write_at(bar_x + 140, bar_y + 6, notepad_filepath, 0x002C3E50);
        vbe_write_at(bar_x + bar_w - 95, bar_y + 6, notepad_status, 0x007F8C8D);

        // 2. Metin Yazma Alanı (Beyaz Defter Sayfası)
        int text_area_x = win->x + 8;
        int text_area_y = bar_y + bar_h + 4;
        int text_area_w = win->w - 16;
        int text_area_h = win->h - (bar_h + 38);

        vbe_draw_rect(text_area_x, text_area_y, text_area_w, text_area_h, 0x00FFFFFF);
        vbe_draw_rect(text_area_x, text_area_y, text_area_w, 1, 0x00CBD5E1);
        vbe_draw_rect(text_area_x, text_area_y + text_area_h - 1, text_area_w, 1, 0x00CBD5E1);
        vbe_draw_rect(text_area_x, text_area_y, 1, text_area_h, 0x00CBD5E1);
        vbe_draw_rect(text_area_x + text_area_w - 1, text_area_y, 1, text_area_h, 0x00CBD5E1);

        // Metni çiz (Çok satırlı, \n ve wrap destekli)
        int cur_x = text_area_x + 8;
        int cur_y = text_area_y + 8;
        int max_x = text_area_x + text_area_w - 12;
        int max_y = text_area_y + text_area_h - 18;

        for (int i = 0; i < notepad_len; i++) {
            char ch = notepad_buffer[i];
            if (ch == '\n') {
                cur_x = text_area_x + 8;
                cur_y += 16;
                if (cur_y > max_y) break;
                continue;
            }

            if (cur_x + 8 > max_x) {
                cur_x = text_area_x + 8;
                cur_y += 16;
                if (cur_y > max_y) break;
            }

            vbe_draw_char(cur_x, cur_y, ch, 0x002C3E50);
            cur_x += 8;
        }

        // Yanıp sönen dikey metin imleci (Eğer en öndeyse)
        if (window_list_tail == notepad_window && cur_y <= max_y) {
            notepad_cursor_blink = (notepad_cursor_blink + 1) % 40;
            if (notepad_cursor_blink < 20) {
                vbe_draw_rect(cur_x, cur_y, 2, 14, 0x002C3E50);
            }
        }

        return; // Notepad çizildi
    }

    // 6. Widget'ları Çiz
    gui_widget_t *w = win->widgets_head;
    while (w) {
        int abs_x = win->x + w->x;
        int abs_y = win->y + w->y;
        
        if (w->type == WIDGET_BUTTON) {
            vbe_draw_rect(abs_x, abs_y, w->w, w->h, w->bg_color);
            vbe_draw_rect(abs_x, abs_y, w->w, 1, 0x002F3542);
            vbe_draw_rect(abs_x, abs_y + w->h - 1, w->w, 1, 0x002F3542);
            vbe_draw_rect(abs_x, abs_y, 1, w->h, 0x002F3542);
            vbe_draw_rect(abs_x + w->w - 1, abs_y, 1, w->h, 0x002F3542);
            int text_len = strlen(w->text);
            int text_x = abs_x + (w->w / 2) - (text_len * 4);
            int text_y = abs_y + (w->h / 2) - 8;
            vbe_write_at(text_x, text_y, w->text, w->fg_color);
        } else if (w->type == WIDGET_LABEL) {
            vbe_write_at(abs_x, abs_y, w->text, w->fg_color);
        } else if (w->type == WIDGET_MENU) {
            // Draw menu background (slightly darker)
            vbe_draw_rect(abs_x, abs_y, w->w, w->h, 0x00D3D3D3);
            // Draw border
            vbe_draw_rect(abs_x, abs_y, w->w, 1, 0x002F3542);
            vbe_draw_rect(abs_x, abs_y + w->h - 1, w->w, 1, 0x002F3542);
            vbe_draw_rect(abs_x, abs_y, 1, w->h, 0x002F3542);
            vbe_draw_rect(abs_x + w->w - 1, abs_y, 1, w->h, 0x002F3542);
            // Title text
            (void)strlen(w->text);
            int text_x = abs_x + 4; // left aligned
            int text_y = abs_y + (w->h / 2) - 8;
            vbe_write_at(text_x, text_y, w->text, w->fg_color);
        } else if (w->type == WIDGET_MENU_ITEM) {
            // Simple item rectangle
            vbe_draw_rect(abs_x, abs_y, w->w, w->h, 0x00EAEAEA);
            vbe_draw_rect(abs_x, abs_y, w->w, 1, 0x002F3542);
            vbe_draw_rect(abs_x, abs_y + w->h - 1, w->w, 1, 0x002F3542);
            vbe_draw_rect(abs_x, abs_y, 1, w->h, 0x002F3542);
            vbe_draw_rect(abs_x + w->w - 1, abs_y, 1, w->h, 0x002F3542);
            (void)strlen(w->text);
            int text_x = abs_x + 4;
            int text_y = abs_y + (w->h / 2) - 8;
            vbe_write_at(text_x, text_y, w->text, w->fg_color);
        }
        w = w->next;
    }
}

void gui_render() {
    // 1. SkyOS Tarzi Arka Plan (Mavi Gradyan)
    for (int y = 0; y < 600; y++) {
        unsigned char b = 180 + (y * 70) / 600;
        unsigned char g = 120 + (y * 40) / 600;
        vbe_draw_rect(0, y, 800, 1, (g << 8) | b);
    }
    
    // 2. Masaüstü Simgeleri (Sol Taraf)
    gui_draw_icon(20, 40, "Storage", 0x00ffa502);
    gui_draw_icon(20, 110, "Terminal", 0x002f3542);
    gui_draw_icon(20, 180, "Files", 0x001e90ff);
    gui_draw_icon(20, 250, "Network", 0x002ed573);
    gui_draw_icon(20, 320, "Notepad", 0x008e44ad);
    
    // 3. Pencereleri Çiz (Arkadan Öne)
    window_t *curr = window_list_head;
    while (curr) {
        if (curr->active) {
            gui_draw_window(curr);
        }
        curr = curr->next;
    }
    
    // 4. SkyOS Üst Panel (Top Bar) - Ortalanmış ve şık
    vbe_draw_rect(300, 0, 200, 30, 0x00DCDDE1); // Panel gövdesi
    vbe_draw_rect(300, 30, 200, 1, 0x007F8C8D);  // Alt çizgi
    
    // Panel İçindeki Saat (Gercek Zamanli)
    int h, m, s;
    extern void rtc_get_time(int *h, int *m, int *s);
    rtc_get_time(&h, &m, &s);

    char time_str[16];
    // Basit itoa/format mantigi (kprintf olmadigi icin manuel)
    time_str[0] = (h / 10) + '0'; time_str[1] = (h % 10) + '0'; time_str[2] = ':';
    time_str[3] = (m / 10) + '0'; time_str[4] = (m % 10) + '0'; time_str[5] = ':';
    time_str[6] = (s / 10) + '0'; time_str[7] = (s % 10) + '0'; time_str[8] = '\0';
    
    vbe_write_at(360, 8, time_str, 0x002F3640);
}
