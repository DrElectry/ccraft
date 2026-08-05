#include "core/item.h"
#include "core/tile.h"
#include "core/chunk.h"
#include "core/main.h"
#include <string.h>

Item items[19];
InventoryEntry inventory[INVENTORY_SLOTS] = {0};

static const char* item_names[19] = {
    "EMPTY",
    "GRASS",
    "DIRT",
    "LEAVES",
    "STONE",
    "IRON BLOCK",
    "WATER",
    "LOG",
    "GLASS",
    "COAL ORE",
    "IRON ORE",
    "GOLD ORE",
    "SAND",
    "GRAVEL",
    "LAVA",
    "ROSE",
    "GRASS CROSS",
    "BORDER",
    "PLANKS",
};

void items_init(void) {
    for (uint16_t id = 0; id <= LAST_TILE; id++) {
        Item* it = &items[id];
        it->id = id;
        it->stack_size = 64;
        it->viewmodel = tile_render_cache[id];
        it->viewmodel_offset[0] = 0.0f;
        it->viewmodel_offset[1] = 0.0f;
        it->viewmodel_offset[2] = 0.0f;
        it->viewmodel_rotation[0] = 0.0f;
        it->viewmodel_rotation[1] = 0.0f;
        it->viewmodel_rotation[2] = 0.0f;
        it->viewmodel_scale[0] = 1.0f;
        it->viewmodel_scale[1] = 1.0f;
        it->viewmodel_scale[2] = 1.0f;
        it->inventory_slot.id = tile_fbos[id].color_attachments[0];
        strncpy(it->name, item_names[id], MAX_NICKNAME - 1);
        it->name[MAX_NICKNAME - 1] = '\0';
    }
}

void inventory_init(void) {
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        inventory[i].item = items[0];
        inventory[i].amount = 0;
        glm_vec2_copy((vec2){30.0f + ((float)i * 54.0f), 0.0f}, inventory[i].request.pos);
        glm_vec2_copy((vec2){48.0f, 48.0f}, inventory[i].request.scale);
        inventory[i].request.rotation = 0.0f;
        inventory[i].request.alpha = 1.0f;
        gfx_canvas_packet_static_request(&inventory[i].request);
    }

    inventory[0].item = items[GRASS];
    inventory[0].amount = INFINITE_AMOUNT;
}

void inventory_draw_icons(Program* active_program) {
    program_use(active_program);

    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        InventoryEntry* e = &inventory[i];
        if (e->amount == 0 || e->item.id == 0) continue;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, e->item.inventory_slot.id);
        program_set_int(active_program, "tex", 0);

        e->request.pos[1] = (float)HEIGHT - 30.0f;
        gfx_canvas_render(&e->request, active_program);
    }
}
