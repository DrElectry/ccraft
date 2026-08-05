#ifndef ITEM_H
#define ITEM_H

#include "core/gfx.h"
#include <cglm/cglm.h>
#include "gl/tex.h"

#define MAX_NICKNAME 64
#define INVENTORY_SLOTS 8
#define INFINITE_AMOUNT 255

struct World;

typedef struct Item Item;

typedef uint8_t ItemRarity;

enum {
    RARITY_COMMON = 0,
    RARITY_RARE,
    RARITY_MYTHIC,
    RARITY_LEGENDARY,
    RARITY_COUNT
};

typedef void (*ItemRenderFn)(Item* item, Program* active_program, mat4 model);
typedef void (*ItemUseFn)(Item* item, struct World* world, int x, int y, int z);
typedef void (*ItemSelectFn)(Item* item);
typedef void (*ItemUpdateFn)(Item* item, float dt);

typedef struct Item {
    uint16_t id;
    uint8_t stack_size;
    ItemRarity rarity;

    Render_request viewmodel;
    vec3 viewmodel_offset, viewmodel_rotation, viewmodel_scale;

    int no_animation;

    // optional
    Skinned_render_request* skinned;
    Program* skinned_program;

    Program render_program;
    Texture base, roughness, inventory_slot;

    char name[MAX_NICKNAME];

    ItemRenderFn on_render;
    ItemUseFn on_use;
    ItemSelectFn on_select;
    ItemUpdateFn on_update;
} Item;

typedef struct ItemSpec {
    uint16_t id;
    uint8_t stack_size;
    ItemRarity rarity;

    Render_request viewmodel;
    vec3 viewmodel_offset, viewmodel_rotation, viewmodel_scale;

    int no_animation;

    // optional
    Skinned_render_request* skinned;
    Program* skinned_program;

    Program render_program;
    Texture base, roughness, inventory_slot;

    char name[MAX_NICKNAME];

    ItemRenderFn on_render;
    ItemUseFn on_use;
    ItemSelectFn on_select;
    ItemUpdateFn on_update;
} ItemSpec;

typedef struct {
    Canvas_Render_Request request;
    Item* item;
    uint8_t amount;
} InventoryEntry;

extern InventoryEntry inventory[INVENTORY_SLOTS];

void items_init(void);
void inventory_init(void);
void inventory_draw_icons(Program* active_program);

Item* item_register(const ItemSpec* spec);
Item* item_get(uint16_t id);
uint8_t item_rarity_color(const Item* item);

void item_transform_viewmodel(mat4 model, Item* item);
void item_render_block(Item* item, Program* active_program, mat4 model);
void item_use_block(Item* item, struct World* world, int x, int y, int z);
int item_has_skinned(Item* item);
void item_render_skinned(Item* item, Program* active_program, mat4 model);

void inventory_set(int slot, Item* item, uint8_t amount);
InventoryEntry* inventory_get(int slot);
void inventory_select(int slot);
int inventory_selected(void);
Item* inventory_selected_item(void);

void inventory_switch(int slot);
void item_switch_update(float dt);
float item_switch_offset(void);
Item* inventory_displayed_item(void);

#endif
