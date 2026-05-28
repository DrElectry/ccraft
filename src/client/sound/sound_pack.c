#include "sound/sound_pack.h"
#include "sound/sound.h"

#include "core/chunk.h"

#include <stdio.h>
#include <stdlib.h>

static sound_t* g_packs[SOUND_PACK_COUNT][SOUND_VARIANTS_PER_PACK] = {{0}};
static bool g_packs_loaded = false;

sound_t* sound_pack_load(uint16_t pack_index, const char* base_dir)
{
    (void)pack_index;
    (void)base_dir;
    return NULL;
}

void sound_pack_destroy() {
    if (g_packs_loaded) {
        for (int p = 0; p < SOUND_PACK_COUNT; p++) {
            for (int v = 0; v < SOUND_VARIANTS_PER_PACK; v++) {
                if (g_packs[p][v]) {
                    sound_free(g_packs[p][v]);
                    g_packs[p][v] = NULL;
                }
            }
        }
        g_packs_loaded = false;
    }
}

void sound_pack_free(sound_t* s[4], uint16_t pack_index)
{
    (void)pack_index;
    for (int i = 0; i < 4; i++) {
        if (s[i]) {
            sound_free(s[i]);
            s[i] = NULL;
        }
    }
}

void sound_pack_play(sound_t* s[4], uint16_t variant)
{
    if (!s)
        return;
    sound_t* snd = s[variant & 3];
    if (snd)
    sound_play(snd);
}

void packs_ensure_loaded(void) {
    if (g_packs_loaded)
        return;

    int pack = 0;
    g_packs[pack][0] = sound_load("assets/sounds/grass1.wav");
    g_packs[pack][1] = sound_load("assets/sounds/grass2.wav");
    g_packs[pack][2] = sound_load("assets/sounds/grass3.wav");
    g_packs[pack][3] = sound_load("assets/sounds/grass4.wav");

    pack = 1;
    g_packs[pack][0] = sound_load("assets/sounds/stone1.wav");
    g_packs[pack][1] = sound_load("assets/sounds/stone2.wav");
    g_packs[pack][2] = sound_load("assets/sounds/stone3.wav");
    g_packs[pack][3] = sound_load("assets/sounds/stone4.wav");

    pack = 2;
    g_packs[pack][0] = sound_load("assets/sounds/gravel1.wav");
    g_packs[pack][1] = sound_load("assets/sounds/gravel2.wav");
    g_packs[pack][2] = sound_load("assets/sounds/gravel3.wav");
    g_packs[pack][3] = sound_load("assets/sounds/gravel4.wav");

    pack = 3;
    g_packs[pack][0] = sound_load("assets/sounds/wood1.wav");
    g_packs[pack][1] = sound_load("assets/sounds/wood2.wav");
    g_packs[pack][2] = sound_load("assets/sounds/wood3.wav");
    g_packs[pack][3] = sound_load("assets/sounds/wood4.wav");

    g_packs_loaded = true;
}

sound_t* pick_pack_sound(uint16_t tile_id, int variant) {
    uint16_t pack = chunk_sound_pack(tile_id);
    if (pack >= SOUND_PACK_COUNT)
        pack = 0;

    uint16_t v = (uint16_t)(variant & (SOUND_VARIANTS_PER_PACK - 1));
    return g_packs[pack][v];
}

