#ifndef MOUSE_H
#define MOUSE_H
#define MOUSE_LEFT   0x01
#define MOUSE_RIGHT  0x02
#define MOUSE_MIDDLE 0x04

#include "common.h"

// Mouse Durumları
typedef struct {
    unsigned char x_relative;
    unsigned char y_relative;
    unsigned char buttons;
    int x;
    int y;
    int scroll; // Tekerlek dikey hareketi (+1: yukari, -1: asagi)
} mouse_state_t;

// Fonksiyonlar
void mouse_init();
void mouse_handler(struct registers *r);
mouse_state_t* mouse_get_state();

#endif
