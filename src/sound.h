#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>

typedef struct sound_t sound_t;

bool sound_init(void);
void sound_shutdown(void);

sound_t* sound_load(const char* path);
void sound_free(sound_t* sound);

void sound_play(sound_t* sound);
void sound_stop(sound_t* sound);

void sound_set_volume(sound_t* sound, float volume);
void sound_set_looping(sound_t* sound, bool looping);

bool sound_is_playing(sound_t* sound);

#endif