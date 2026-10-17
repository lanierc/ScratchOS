#ifndef GUI_H
#define GUI_H

#include "vbe.h"

#define MAX_WINDOWS 10

enum gui_event_type {
    GUI_EVENT_CLICK,
    GUI_EVENT_MOUSE_ENTER,
    GUI_EVENT_MOUSE_LEAVE,
    GUI_EVENT_KEYPRESS
};

enum gui_widget_type {
    WIDGET_BUTTON,
    WIDGET_LABEL,
    WIDGET_TEXTBOX,
    WIDGET_MENU,       // New menu widget
    WIDGET_MENU_ITEM   // Items belonging to a menu
};

typedef struct gui_widget {
    int type;
    int x, y;
    int w, h;
    char text[64];
    unsigned int bg_color;
    unsigned int fg_color;
    
    void (*on_click)(struct gui_widget* sender);
    
    struct gui_widget *next;
    struct gui_widget *prev;
} gui_widget_t;

typedef struct window {
    int x, y;
    int w, h;
    char title[64];
    unsigned int color;
    int active;
    int focus;
    int is_dragging;
    int minimized;
    
    gui_widget_t *widgets_head;
    gui_widget_t *widgets_tail;
    
    struct window *next;
    struct window *prev;
} window_t;

void gui_init();
gui_widget_t* gui_create_menu(window_t* win, int x, int y, int w, int h, char* text);
gui_widget_t* gui_create_menu_item(window_t* win, int x, int y, int w, int h, char* text, void (*on_click)(gui_widget_t*));
void gui_render();
void gui_on_mouse_event(int x, int y, int buttons, int scroll);
void gui_on_key_event(char c);
window_t* gui_create_window(char* title, int x, int y, int w, int h, unsigned int color);
void gui_destroy_window(window_t *win);
void gui_minimize_window(window_t *win);
void gui_restore_window(window_t *win);
gui_widget_t* gui_create_button(window_t* win, int x, int y, int w, int h, char* text, void (*on_click)(gui_widget_t*));
gui_widget_t* gui_create_label(window_t* win, int x, int y, char* text, unsigned int color);

void gui_term_init(window_t *win);
void gui_term_put_char(char c);
void gui_term_put_str(const char *str);
void gui_term_scroll(int delta);

void gui_fm_init(window_t *win);
void gui_fm_open_path(const char *path);
void gui_fm_on_click(int rel_x, int rel_y);
void gui_fm_scroll(int delta);

void gui_notepad_init(window_t *win, const char *filepath);
void gui_notepad_on_key(char c);
void gui_notepad_on_click(int rel_x, int rel_y);
void gui_notepad_save(void);
void gui_notepad_clear(void);

void gui_sysinfo_init(window_t *win);
void gui_net_init(window_t *win);
void gui_calc_init(window_t *win);
void gui_calc_on_click(int rel_x, int rel_y);
void gui_toggle_start_menu(void);

#endif
