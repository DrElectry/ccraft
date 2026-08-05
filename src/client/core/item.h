#ifndef ITEM_H
#define ITEM_H

#include "core/gfx.h"
#include <cglm/cglm.h>
#include "gl/tex.h"

#define MAX_NICKNAME 64
#define INVENTORY_SLOTS 8
#define INFINITE_AMOUNT 255

typedef struct {
    uint16_t id;
    uint8_t stack_size;

    Render_request viewmodel;
    vec3 viewmodel_offset, viewmodel_rotation, viewmodel_scale;

    Program render_program;
    Texture base, roughness, inventory_slot;

    char name[MAX_NICKNAME];
} Item;

typedef struct {
    Canvas_Render_Request request;
    Item item;
    uint8_t amount;
} InventoryEntry;

extern Item items[19];
extern InventoryEntry inventory[INVENTORY_SLOTS];

void items_init(void);
void inventory_init(void);
void inventory_draw_icons(Program* active_program);

#endif

