#include "sound_pack.h"
#include "sound.h"

#include <stdio.h>
#include <stdlib.h>

sound_t* sound_pack_load(uint16_t pack_index, const char* base_dir)
{
    (void)pack_index;
    (void)base_dir;
    return NULL;
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

