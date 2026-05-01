#ifndef NOISE_H
#define NOISE_H

#include <stdint.h>

float noise2d(float x, float y);
float noise3d(float x, float y, float z);
float fbm2d(float x, float y, int octaves, float persistence, float lacunarity);
float fbm3d(float x, float y, float z, int octaves, float persistence, float lacunarity);
float noise_height(int world_x, int world_z, int sea_level);

#endif
