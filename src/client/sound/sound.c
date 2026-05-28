#define MINIAUDIO_IMPLEMENTATION // pentupling the compile times
#include "miniaudio.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct {
    ma_sound sound;
} sound_t;

static ma_engine g_engine;
static bool g_initialized = false;

bool sound_init(void)
{
    if (g_initialized)
        return true;

    ma_result result = ma_engine_init(NULL, &g_engine);

    if (result != MA_SUCCESS)
        return false;

    g_initialized = true;
    return true;
}

void sound_shutdown(void)
{
    if (!g_initialized)
        return;

    ma_engine_uninit(&g_engine);
    g_initialized = false;
}

sound_t* sound_load(const char* path)
{
    if (!g_initialized)
        return NULL;

    sound_t* sound = malloc(sizeof(sound_t));
    if (!sound)
        return NULL;

    ma_result result = ma_sound_init_from_file(
        &g_engine,
        path,
        0,
        NULL,
        NULL,
        &sound->sound
    );

    if (result != MA_SUCCESS)
    {
        free(sound);
        return NULL;
    }

    return sound;
}

void sound_free(sound_t* sound)
{
    if (!sound)
        return;

    ma_sound_uninit(&sound->sound);
    free(sound);
}

void sound_play(sound_t* sound)
{
    if (!sound)
        return;

    ma_sound_start(&sound->sound);
}

void sound_stop(sound_t* sound)
{
    if (!sound)
        return;

    ma_sound_stop(&sound->sound);
}

void sound_set_volume(sound_t* sound, float volume)
{
    if (!sound)
        return;

    ma_sound_set_volume(&sound->sound, volume);
}

void sound_set_looping(sound_t* sound, bool looping)
{
    if (!sound)
        return;

    ma_sound_set_looping(&sound->sound, looping ? MA_TRUE : MA_FALSE);
}

bool sound_is_playing(sound_t* sound)
{
    if (!sound)
        return false;

    return ma_sound_is_playing(&sound->sound) == MA_TRUE;
}