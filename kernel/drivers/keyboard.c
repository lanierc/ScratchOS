#include "idt.h"
#include "shell.h"

// Türkçe Q Klavye Düzeni (Scancode Set 1)
static int shift_pressed = 0;

extern int shell_active;

#define KBD_BUF_SIZE 256
static unsigned char kbd_buffer[KBD_BUF_SIZE];
static int kbd_buf_head = 0;
static int kbd_buf_tail = 0;

extern int vbe_is_active();
extern void gui_on_key_event(char c);

void keyboard_put(unsigned char c) {
    if (vbe_is_active()) {
        gui_on_key_event(c);
    } else if (shell_active) {
        shell_input(c);
    } else {
        kbd_buffer[kbd_buf_head] = c;
        kbd_buf_head = (kbd_buf_head + 1) % KBD_BUF_SIZE;
    }
}

unsigned char keyboard_get() {
    if (kbd_buf_head == kbd_buf_tail) return 0; // Boş
    unsigned char c = kbd_buffer[kbd_buf_tail];
    kbd_buf_tail = (kbd_buf_tail + 1) % KBD_BUF_SIZE;
    return c;
}

// Boyutu derleyiciye bırakalım [] kullanarak
static const unsigned char scancode_normal[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '*', '-', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', 'g', 0x81, '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 's', 'i', '"', 0,
    ',', 'z', 'x', 'c', 'v', 'b', 'n', 'm', 0x94, 0x87, '.', 0, '*', 0, ' ', 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char scancode_shift[] = {
    0, 27, '!', '\'', '^', '+', '%', '&', '/', '(', ')', '=', '?', '_', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 'G', 0x9A, '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 'S', 'I', '\'', 0,
    ';', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', 0x99, 0x80, ':', 0, '*', 0, ' ', 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void keyboard_handler(struct registers *r) {
    unsigned char scancode = inb(0x60);

    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }
    if (scancode == (0x2A | 0x80) || scancode == (0x36 | 0x80)) {
        shift_pressed = 0;
        return;
    }

    if (scancode & 0x80) return;

    // Yön tuşları
    if (scancode == 0x48) { // Yukarı ok
        if (vbe_is_active()) {
            gui_on_key_event('\x11');
            return;
        }
        shell_input('\x11');
        return;
    }
    if (scancode == 0x50) { // Aşağı ok
        if (vbe_is_active()) {
            gui_on_key_event('\x12');
            return;
        }
        shell_input('\x12');
        return;
    }

    // Scancode dizinin boyutundan büyükse işlem yapma (güvenlik için)
    if (scancode >= sizeof(scancode_normal)) return;

    unsigned char c = (shift_pressed) ? scancode_shift[scancode] : scancode_normal[scancode];

    if (c) {
        keyboard_put(c);
    }
}

void init_keyboard() {
    // Klavye handler'ını kaydet
    irq_install_handler(1, keyboard_handler);

    // IRQ1'i (klavye) aç - Master PIC maskeleme kaydını güncelle
    unsigned char mask = inb(0x21);
    mask &= ~(1 << 1);  // Bit 1'i temizle = IRQ1 açık
    outb(0x21, mask);
}