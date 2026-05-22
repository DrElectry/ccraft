#ifndef SOUND_PACK_H
#define SOUND_PACK_H

#include <stdint.h>

#include "sound.h"

sound_t* sound_pack_load(uint16_t pack_index, const char* base_dir);
void sound_pack_free(sound_t* s[4], uint16_t pack_index);

void sound_pack_play(sound_t* s[4], uint16_t variant);


#endif

