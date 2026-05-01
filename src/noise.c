#include "noise.h"
#include "rand.h"
#include <math.h>

static const float F2 = 0.3660254037844386f;
static const float G2 = 0.21132486540518713f;
static const float F3 = 0.3333333333333333f;
static const float G3 = 0.1666666666666667f;

static const float grad2d[8][2] = {
    {1,1}, {-1,1}, {1,-1}, {-1,-1},
    {1,0}, {-1,0}, {0,1}, {0,-1}
};

static const float grad3d[12][3] = {
    {1,1,0}, {-1,1,0}, {1,-1,0}, {-1,-1,0},
    {1,0,1}, {-1,0,1}, {1,0,-1}, {-1,0,-1},
    {0,1,1}, {0,-1,1}, {0,1,-1}, {0,-1,-1}
};

static inline float dot2(uint8_t gi, float x, float y) {
    return grad2d[gi & 7][0] * x + grad2d[gi & 7][1] * y;
}

static inline float dot3(uint8_t gi, float x, float y, float z) {
    return grad3d[gi & 11][0] * x + grad3d[gi & 11][1] * y + grad3d[gi & 11][2] * z;
}

static inline uint8_t hash2(int i, int j) {
    return (uint8_t)((global_rng.state >> ((i + j * 17) & 63)) & 255);
}

static inline uint8_t hash3(int i, int j, int k) {
    return (uint8_t)((global_rng.state >> ((i + j * 23 + k * 31) & 63)) & 255);
}

static float noise2d_impl(float x, float y) {
    float s = (x + y) * F2;
    int i = (int)floorf(x + s);
    int j = (int)floorf(y + s);
    
    float t = (float)(i + j) * G2;
    float X0 = x - (float)i + t;
    float Y0 = y - (float)j + t;
    
    int i1, j1;
    if (X0 > Y0) { i1 = 1; j1 = 0; }
    else { i1 = 0; j1 = 1; }
    
    float x1 = X0 - (float)i1 + G2;
    float y1 = Y0 - (float)j1 + G2;
    float x2 = X0 - 1.0f + 2.0f * G2;
    float y2 = Y0 - 1.0f + 2.0f * G2;
    
    float n0, n1, n2;
    float t0 = 0.5f - X0*X0 - Y0*Y0;
    float t1 = 0.5f - x1*x1 - y1*y1;
    float t2 = 0.5f - x2*x2 - y2*y2;
    
    n0 = t0 > 0 ? t0 * t0 * dot2(hash2(i, j), X0, Y0) : 0.0f;
    n1 = t1 > 0 ? t1 * t1 * dot2(hash2(i+i1, j+j1), x1, y1) : 0.0f;
    n2 = t2 > 0 ? t2 * t2 * dot2(hash2(i+1, j+1), x2, y2) : 0.0f;
    
    return 70.0f * (n0 + n1 + n2);
}

static float noise3d_impl(float x, float y, float z) {
    float s = (x + y + z) * F3;
    int i = (int)floorf(x + s);
    int j = (int)floorf(y + s);
    int k = (int)floorf(z + s);
    
    float t = (float)(i + j + k) * G3;
    float X0 = x - (float)i + t;
    float Y0 = y - (float)j + t;
    float Z0 = z - (float)k + t;
    
    int i1, j1, k1, i2, j2, k2;
    if (X0 >= Y0) {
        if (Y0 >= Z0) { i1=1;j1=0;k1=0;i2=1;j2=1;k2=0; }
        else if (X0 >= Z0) { i1=1;j1=0;k1=0;i2=1;j2=0;k2=1; }
        else { i1=0;j1=0;k1=1;i2=1;j2=0;k2=1; }
    } else {
        if (Y0 < Z0) { i1=0;j1=0;k1=1;i2=0;j2=1;k2=1; }
        else if (X0 < Z0) { i1=0;j1=1;k1=0;i2=0;j2=1;k2=1; }
        else { i1=0;j1=1;k1=0;i2=1;j2=1;k2=0; }
    }
    
    float x1 = X0 - (float)i1 + G3;
    float y1 = Y0 - (float)j1 + G3;
    float z1 = Z0 - (float)k1 + G3;
    float x2 = X0 - (float)i2 + 2.0f*G3;
    float y2 = Y0 - (float)j2 + 2.0f*G3;
    float z2 = Z0 - (float)k2 + 2.0f*G3;
    float x3 = X0 - 1.0f + 3.0f*G3;
    float y3 = Y0 - 1.0f + 3.0f*G3;
    float z3 = Z0 - 1.0f + 3.0f*G3;
    
    float n0, n1, n2, n3;
    float t0 = 0.6f - X0*X0 - Y0*Y0 - Z0*Z0;
    float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1;
    float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2;
    float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3;
    
    n0 = t0 > 0 ? t0*t0*dot3(hash3(i,j,k), X0,Y0,Z0) : 0.0f;
    n1 = t1 > 0 ? t1*t1*dot3(hash3(i+i1,j+j1,k+k1), x1,y1,z1) : 0.0f;
    n2 = t2 > 0 ? t2*t2*dot3(hash3(i+i2,j+j2,k+k2), x2,y2,z2) : 0.0f;
    n3 = t3 > 0 ? t3*t3*dot3(hash3(i+1,j+1,k+1), x3,y3,z3) : 0.0f;
    
    return 32.0f * (n0 + n1 + n2 + n3);
}

float noise2d(float x, float y) {
    return noise2d_impl(x, y);
}

float noise3d(float x, float y, float z) {
    return noise3d_impl(x, y, z);
}

float fbm2d(float x, float y, int octaves, float persistence, float lacunarity) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float max_value = 0.0f;
    
    for (int i = 0; i < octaves; i++) {
        total += noise2d_impl(x * frequency, y * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    
    return total / max_value;
}

float fbm3d(float x, float y, float z, int octaves, float persistence, float lacunarity) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float max_value = 0.0f;
    
    for (int i = 0; i < octaves; i++) {
        total += noise3d_impl(x * frequency, y * frequency, z * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    
    return total / max_value;
}

float noise_height(int world_x, int world_z, int sea_level) {
    float h = fbm2d((float)world_x * 0.015f, (float)world_z * 0.015f, 4, 0.5f, 2.0f);
    float detail = fbm2d((float)world_x * 0.08f, (float)world_z * 0.08f, 2, 0.5f, 2.0f) * 0.25f;
    return (float)(sea_level + (int)((h + detail) * 16.0f));
}
