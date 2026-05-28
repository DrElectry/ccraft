#ifndef SOUND_PACK_H
#define SOUND_PACK_H

#include <stdint.h>

#include "sound/sound.h"

#define SOUND_VARIANTS_PER_PACK 4
#define SOUND_PACK_COUNT 4

sound_t* sound_pack_load(uint16_t pack_index, const char* base_dir);
void sound_pack_free(sound_t* s[4], uint16_t pack_index);

void sound_pack_play(sound_t* s[4], uint16_t variant);
void packs_ensure_loaded(void);
sound_t* pick_pack_sound(uint16_t tile_id, int variant);
void sound_pack_destroy();


#endif

