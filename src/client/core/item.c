#include "core/item.h"
#include "core/tile.h"
#include "core/chunk.h"
#include "core/world.h"
#include "core/main.h"
#include "core/game.h"
#include "gl/fbo.h"
#include "gl/vao.h"
#include "gl/tex.h"
#include "glad.h"
#include "sound/sound.h"
#include "sound/sound_pack.h"
#include "network/network.h"
#include "utils/rand.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define MAX_ITEMS 1024

extern Texture texture_atlas, roughness, normal;

static Item items[MAX_ITEMS];
static uint16_t item_count = 0;
int g_selected_slot = 0;

InventoryEntry inventory[INVENTORY_SLOTS] = {0};

#define SWITCH_SLIDE_DEPTH 0.7f
#define SWITCH_SPRING_K   360.0f
#define SWITCH_DAMPING    8.0f
#define SWITCH_EPSILON    0.0005f

static Item* g_displayed_item = NULL;
static Item* g_pending_item  = NULL;
static float g_slide = 0.0f;
static float g_slide_vel = 0.0f;
static float g_slide_target = 0.0f;
static int g_switching = 0;

void item_transform_viewmodel(mat4 model, Item* item) {
    glm_translate(model, item->viewmodel_offset);
    glm_rotate(model, item->viewmodel_rotation[0], (vec3){1.0f, 0.0f, 0.0f});
    glm_rotate(model, item->viewmodel_rotation[1], (vec3){0.0f, 1.0f, 0.0f});
    glm_rotate(model, item->viewmodel_rotation[2], (vec3){0.0f, 0.0f, 1.0f});
    glm_scale(model, item->viewmodel_scale);
}

Item* item_register(const ItemSpec* spec) {
    if (!spec || item_count >= MAX_ITEMS) return NULL;

    Item* it = &items[item_count];
    memset(it, 0, sizeof(Item));
    it->id = spec->id;
    it->stack_size = spec->stack_size;
    it->rarity = spec->rarity;
    it->viewmodel = spec->viewmodel;
    it->viewmodel_offset[0] = spec->viewmodel_offset[0];
    it->viewmodel_offset[1] = spec->viewmodel_offset[1];
    it->viewmodel_offset[2] = spec->viewmodel_offset[2];
    it->viewmodel_rotation[0] = spec->viewmodel_rotation[0];
    it->viewmodel_rotation[1] = spec->viewmodel_rotation[1];
    it->viewmodel_rotation[2] = spec->viewmodel_rotation[2];
    it->viewmodel_scale[0] = spec->viewmodel_scale[0];
    it->viewmodel_scale[1] = spec->viewmodel_scale[1];
    it->viewmodel_scale[2] = spec->viewmodel_scale[2];
    it->no_animation = spec->no_animation;
    it->render_program = spec->render_program;
    it->base = spec->base;
    it->roughness = spec->roughness;
    it->inventory_slot = spec->inventory_slot;
    it->skinned = spec->skinned;
    it->skinned_program = spec->skinned_program;
    it->on_render = spec->on_render;
    it->on_use = spec->on_use;
    it->on_select = spec->on_select;
    it->on_update = spec->on_update;
    strncpy(it->name, spec->name, MAX_NICKNAME - 1);
    it->name[MAX_NICKNAME - 1] = '\0';

    item_count++;
    return it;
}

Item* item_get(uint16_t id) {
    for (uint16_t i = 0; i < item_count; i++) {
        if (items[i].id == id) return &items[i];
    }
    return NULL;
}

uint8_t item_rarity_color(const Item* item) {
    if (!item) return 0x0F;
    switch (item->rarity) {
        case RARITY_RARE:     return 0x0E; // yellow
        case RARITY_MYTHIC:   return 0x05; // purple
        case RARITY_LEGENDARY: return 0x0B; // aqua
        case RARITY_COMMON:
        default:              return 0x0F; // white
    }
}

void item_render_block(Item* item, Program* active_program, mat4 model) {
    item_transform_viewmodel(model, item);

    program_use(active_program);
    texture_bind(&texture_atlas, 0);
    texture_bind(&roughness, 1);
    texture_bind(&normal, 2);
    program_set_int(active_program, "tex", 0);
    program_set_int(active_program, "roug", 1);
    program_set_int(active_program, "normal", 2);
    program_set_mat4(active_program, "proj", (float*)projection);
    program_set_mat4(active_program, "view", (float*)view);
    program_set_mat4(active_program, "model", (float*)model);

    glDisable(GL_CULL_FACE);
    vao_bind(&item->viewmodel.cache.vao);
    glDrawElements(GL_TRIANGLES, item->viewmodel.tri_count * 3, GL_UNSIGNED_INT, NULL);
    glEnable(GL_CULL_FACE);
}

int item_has_skinned(Item* item) {
    return item && item->skinned && item->skinned->skinned &&
           item->skinned->skeleton && item->skinned->look.enabled;
}

void item_render_skinned(Item* item, Program* active_program, mat4 model) {
    if (!item || !item->skinned || !item->skinned->skinned) return;

    Skinned_render_request* req = item->skinned;
    Skinned* sk = req->skinned;

    // transform the skinned model with the viewmodel transform
    glm_translate(model, item->viewmodel_offset);
    glm_rotate(model, item->viewmodel_rotation[0], (vec3){1.0f, 0.0f, 0.0f});
    glm_rotate(model, item->viewmodel_rotation[1], (vec3){0.0f, 1.0f, 0.0f});
    glm_rotate(model, item->viewmodel_rotation[2], (vec3){0.0f, 0.0f, 1.0f});
    glm_scale(model, item->viewmodel_scale);

    if (!item->skinned->look.enabled) {
        item->skinned->look.enabled = 1;
    }
    glm_mat4_copy(model, sk->node_transform);

    // skinned_render() only sets the model uniform, so we must set
    // projection/view on the skinned program before drawing
    Program* sp = item->skinned_program ? item->skinned_program : active_program;
    program_use(sp);
    program_set_mat4(sp, "projection", (float*)projection);
    program_set_mat4(sp, "view", (float*)view);

    gfx_skinned_render(req, sp);
}

void item_use_block(Item* item, World* world, int x, int y, int z) {
    int variant = RAND(0, 3);
    sound_t* s = pick_pack_sound(item->id, variant);
    if (s) {
        sound_set_looping(s, false);
        sound_set_volume(s, 1.0f);
        sound_play(s);
    }

    if (!__onserv) {
        world_set_block(world, x, y, z, item->id);
        rebuild_chunks_for_block(world, x, y, z);
    } else {
        network_send_block_change(network_get_local_client_id(), x, y, z, item->id);
    }
    game_mark_shadow_dirty();
}

static void item_empty_render(Item* item, Program* active_program, mat4 model) {
    (void)item;
    (void)active_program;
    (void)model;
}

static void item_empty_use(Item* item, World* world, int x, int y, int z) {
    (void)item;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
}

static const char* tile_names[LAST_TILE + 1] = {
    [GRASS] = "GRASS",
    [DIRT] = "DIRT",
    [LEAVES] = "LEAVES",
    [STONE] = "STONE",
    [IRON_BLOCK] = "IRON BLOCK",
    [WATER] = "WATER",
    [LOG] = "LOG",
    [GLASS] = "GLASS",
    [COAL_ORE] = "COAL ORE",
    [IRON_ORE] = "IRON ORE",
    [GOLD_ORE] = "GOLD ORE",
    [SAND] = "SAND",
    [GRAVEL] = "GRAVEL",
    [LAVA] = "LAVA",
    [ROSE] = "ROSE",
    [GRASS_CROSS] = "GRASS",
    [BORDER] = "BORDER",
    [PLANKS] = "PLANKS",
};

static void default_block_spec(ItemSpec* spec, uint16_t id) {
    memset(spec, 0, sizeof(ItemSpec));
    spec->id = id;
    spec->stack_size = 64;
    spec->rarity = RARITY_COMMON;
    spec->viewmodel = tile_render_cache[id];
    spec->inventory_slot.id = tile_fbos[id].color_attachments[0];
    spec->viewmodel_offset[0] = 0.15f;
    spec->viewmodel_offset[1] = -0.50f;
    spec->viewmodel_offset[2] = 0.60f;
    spec->viewmodel_rotation[1] = 45.0f;
    spec->viewmodel_scale[0] = 0.25f;
    spec->viewmodel_scale[1] = 0.25f;
    spec->viewmodel_scale[2] = 0.25f;
    spec->on_render = item_render_block;
    spec->on_use = item_use_block;

    if (id <= LAST_TILE && tile_names[id]) {
        strncpy(spec->name, tile_names[id], MAX_NICKNAME - 1);
        spec->name[MAX_NICKNAME - 1] = '\0';
    }
}

void items_init(void) {
    item_count = 0;

    ItemSpec spec;

    memset(&spec, 0, sizeof(ItemSpec));
    spec.id = 0;
    spec.stack_size = 0;
    spec.on_render = item_empty_render;
    spec.on_use = item_empty_use;
    strncpy(spec.name, "Null", MAX_NICKNAME - 1);
    item_register(&spec);

    for (uint16_t id = FIRST_TILE; id <= LAST_TILE; id++) {
        default_block_spec(&spec, id);
        item_register(&spec);
    }
}

void inventory_init(void) {
    Item* empty = item_get(0);

    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        inventory[i].item = empty;
        inventory[i].amount = 0;
        glm_vec2_copy((vec2){30.0f + ((float)i * 54.0f), 0.0f}, inventory[i].request.pos);
        glm_vec2_copy((vec2){48.0f, 48.0f}, inventory[i].request.scale);
        inventory[i].request.rotation = 0.0f;
        inventory[i].request.alpha = 1.0f;
        gfx_canvas_packet_static_request(&inventory[i].request);
    }

    inventory_set(0, item_get(GRASS), INFINITE_AMOUNT);
    inventory_set(1, item_get(DIRT), INFINITE_AMOUNT);
    inventory_set(2, item_get(STONE), INFINITE_AMOUNT);
    inventory_set(3, item_get(IRON_BLOCK), INFINITE_AMOUNT);
    inventory_set(4, item_get(LOG), INFINITE_AMOUNT);
    inventory_set(5, item_get(PLANKS), INFINITE_AMOUNT);
    inventory_set(6, item_get(GLASS), INFINITE_AMOUNT);
    inventory_set(7, item_get(SAND), INFINITE_AMOUNT);

    g_displayed_item = inventory[g_selected_slot].item;
}

void inventory_draw_icons(Program* active_program) {
    program_use(active_program);

    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        InventoryEntry* e = &inventory[i];
        if (e->amount == 0 || !e->item) continue;
        if (e->item->inventory_slot.id == 0) continue;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, e->item->inventory_slot.id);
        program_set_int(active_program, "tex", 0);

        e->request.pos[1] = (float)HEIGHT - 30.0f;
        gfx_canvas_render(&e->request, active_program);
    }
}

void inventory_set(int slot, Item* item, uint8_t amount) {
    if (slot < 0 || slot >= INVENTORY_SLOTS) return;
    inventory[slot].item = item;
    inventory[slot].amount = amount;
}

InventoryEntry* inventory_get(int slot) {
    if (slot < 0 || slot >= INVENTORY_SLOTS) return NULL;
    return &inventory[slot];
}

void inventory_select(int slot) {
    if (slot < 0 || slot >= INVENTORY_SLOTS) return;
    g_selected_slot = slot;
}

int inventory_selected(void) {
    return g_selected_slot;
}

Item* inventory_selected_item(void) {
    return inventory[g_selected_slot].item;
}

Item* inventory_displayed_item(void) {
    return g_displayed_item;
}

float item_switch_offset(void) {
    return g_slide;
}

void inventory_switch(int slot) {
    if (slot < 0 || slot >= INVENTORY_SLOTS) return;

    Item* next = inventory[slot].item;
    if (!next) return;

    if (next->no_animation) {
        g_displayed_item = next;
        g_pending_item  = NULL;
        g_switching     = 0;
        g_slide         = 0.0f;
        g_slide_vel     = 0.0f;
        g_slide_target  = 0.0f;
        g_selected_slot = slot;
        return;
    }

    if (next == g_displayed_item && !g_switching) {
        g_selected_slot = slot;
        return;
    }

    g_pending_item = next;
    g_switching    = 1;
    g_slide_target = SWITCH_SLIDE_DEPTH;
    g_selected_slot = slot;
}

void item_switch_update(float dt) {
    if (dt <= 0.0f) return;

    float accel = SWITCH_SPRING_K * (g_slide_target - g_slide) - SWITCH_DAMPING * g_slide_vel;
    g_slide_vel += accel * dt;
    g_slide += g_slide_vel * dt;

    if (g_slide_target > 0.0f) {
        if (g_slide >= SWITCH_SLIDE_DEPTH) {
            g_slide = SWITCH_SLIDE_DEPTH;
            g_slide_vel = 0.0f;
        }
    } else {
        if (g_slide <= 0.0f) {
            g_slide = 0.0f;
            g_slide_vel = 0.0f;
        }
    }

    if (g_switching && g_slide >= SWITCH_SLIDE_DEPTH - SWITCH_EPSILON) {
        if (g_pending_item) {
            g_displayed_item = g_pending_item;
        }
        g_pending_item = NULL;
        g_switching    = 0;
        g_slide_target = 0.0f;
    }

    if (!g_switching && g_slide_target == 0.0f &&
        g_slide < SWITCH_EPSILON && fabsf(g_slide_vel) < 0.01f) {
        g_slide = 0.0f;
        g_slide_vel = 0.0f;
    }
}
