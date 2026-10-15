#include "mouse.h"
#include "vbe.h"
#include "idt.h"

static unsigned char mouse_cycle = 0;
static unsigned char mouse_byte[4]; // 4-byte IntelliMouse paketi
static int mouse_has_wheel = 0;
static mouse_state_t m_state;

void mouse_wait(unsigned char type) {
    unsigned int timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((inb(0x64) & 2) == 0) return;
        }
    }
}

void mouse_write(unsigned char a) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, a);
}

unsigned char mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_init() {
    unsigned char _status;

    // 1. Mouse Portunu Aktif Et
    mouse_wait(1);
    outb(0x64, 0xA8);
    
    // 2. ACK'ları temizle ve varsayılanları ayarla
    mouse_write(0xF6); // Set Default
    mouse_read();
    
    // --- IntelliMouse (Scroll Wheel) Aktivasyon Dizisi ---
    // Sample Rate: 200 -> 100 -> 80
    mouse_write(0xF3); mouse_read();
    mouse_write(200);  mouse_read();
    mouse_write(0xF3); mouse_read();
    mouse_write(100);  mouse_read();
    mouse_write(0xF3); mouse_read();
    mouse_write(80);   mouse_read();

    // Device ID oku (3 veya 4 ise tekerlek destekleniyor)
    mouse_write(0xF2); mouse_read();
    unsigned char dev_id = mouse_read();
    if (dev_id == 3 || dev_id == 4) {
        mouse_has_wheel = 1;
    } else {
        mouse_has_wheel = 0;
    }

    mouse_write(0xF4); // Enable Data Reporting
    mouse_read();

    // 3. Kontrolcü Komut Baytını Ayarla
    mouse_wait(1);
    outb(0x64, 0x20); 
    mouse_wait(0);
    _status = inb(0x60) | 2;
    _status &= ~0x20;
    
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, _status);

    // 4. Kesmeleri aç
    outb(0x21, inb(0x21) & ~(1 << 2)); // IRQ2
    outb(0xA1, inb(0xA1) & ~(1 << 4)); // IRQ12

    irq_install_handler(12, mouse_handler);
    
    m_state.x = 400;
    m_state.y = 300;
    m_state.scroll = 0;
    mouse_cycle = 0;
}

void mouse_handler(struct registers *r) {
    unsigned char status = inb(0x64);
    
    // Sadece fare verisi geldiğinde oku
    if ((status & 0x01) && (status & 0x20)) {
        unsigned char data = inb(0x60);
        
        switch(mouse_cycle) {
            case 0:
                // İlk byte'ın 3. biti her zaman 1 olmalı
                if (!(data & 0x08)) return; 
                mouse_byte[0] = data;
                mouse_cycle++;
                return;
            case 1:
                mouse_byte[1] = data;
                mouse_cycle++;
                return;
            case 2:
                mouse_byte[2] = data;
                if (mouse_has_wheel) {
                    mouse_cycle++;
                    return;
                }
                break;
            case 3:
                mouse_byte[3] = data;
                break;
        }

        // Paket tamamlandı, hesapla
        mouse_cycle = 0;
        unsigned char flags = mouse_byte[0];
        int x_move = (int)mouse_byte[1];
        int y_move = (int)mouse_byte[2];
        int scroll = 0;
        if (mouse_has_wheel) {
            scroll = (signed char)mouse_byte[3];
        }

        m_state.buttons = flags & 0x07; // 0x07 = 00000111 (Sol, Sağ, Orta tık)
        m_state.scroll = scroll;

        // İşaret bitlerini (Sign Bits) manuel işle
        if (flags & 0x10) x_move -= 256;
        if (flags & 0x20) y_move -= 256;

        // Taşma kontrolü
        if (flags & 0x40 || flags & 0x80) return;

        m_state.x += x_move;
        m_state.y -= y_move;

        // Sınırları koru
        if (m_state.x < 0) m_state.x = 0;
        if (m_state.y < 0) m_state.y = 0;
        if (m_state.x > 795) m_state.x = 795;
        if (m_state.y > 595) m_state.y = 595;

        // GUI'yi bilgilendir
        extern void gui_on_mouse_event(int x, int y, int buttons, int scroll);
        gui_on_mouse_event(m_state.x, m_state.y, m_state.buttons, scroll);
    }
}

mouse_state_t* mouse_get_state() {
    return &m_state;
}
