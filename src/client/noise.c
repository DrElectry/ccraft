#include "noise.h"
#include "rand.h"
#include <immintrin.h>
#include <math.h>
#include <stdint.h>

#if defined(_MSC_VER)
#define ALIGN16 __declspec(align(16))
#elif defined(__GNUC__) || defined(__clang__)
#define ALIGN16 __attribute__((aligned(16)))
#else
#define ALIGN16
#endif

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

static uint64_t g_world_seed = 0;

void noise_set_seed(uint64_t seed) {
    g_world_seed = seed;
}

static inline uint32_t hash_int(uint32_t x) {
    uint64_t s = g_world_seed;
    x ^= (uint32_t)s;
    x ^= (uint32_t)(s >> 32);
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    x += (uint32_t)(s * 0x9E3779B9u);
    return x;
}

static inline uint8_t hash2(int i, int j) {
    return (uint8_t)(hash_int((uint32_t)(i ^ (j * 57))) & 255);
}

static inline uint8_t hash3(int i, int j, int k) {
    return (uint8_t)(hash_int((uint32_t)(i ^ (j * 57) ^ (k * 113))) & 255);
}

static inline float dot2(uint8_t gi, float x, float y) {
    return grad2d[gi & 7][0] * x + grad2d[gi & 7][1] * y;
}

static inline float dot3(uint8_t gi, float x, float y, float z) {
    return grad3d[gi % 12][0] * x +
           grad3d[gi % 12][1] * y +
           grad3d[gi % 12][2] * z;
}

static inline __m128 floor_ps(__m128 x) {
    __m128i xi = _mm_cvttps_epi32(x);
    __m128 xf = _mm_cvtepi32_ps(xi);
    __m128 mask = _mm_cmplt_ps(x, xf);
    return _mm_sub_ps(xf, _mm_and_ps(mask, _mm_set1_ps(1.0f)));
}

static inline __m128 noise2d_simd(__m128 x, __m128 y) {
    __m128 s = _mm_mul_ps(_mm_add_ps(x, y), _mm_set1_ps(F2));
    __m128 xs = _mm_add_ps(x, s);
    __m128 ys = _mm_add_ps(y, s);

    __m128 i_f = floor_ps(xs);
    __m128 j_f = floor_ps(ys);

    __m128i i = _mm_cvtps_epi32(i_f);
    __m128i j = _mm_cvtps_epi32(j_f);

    __m128 t = _mm_mul_ps(_mm_add_ps(i_f, j_f), _mm_set1_ps(G2));

    __m128 X0 = _mm_add_ps(_mm_sub_ps(x, i_f), t);
    __m128 Y0 = _mm_add_ps(_mm_sub_ps(y, j_f), t);

    __m128 cmp = _mm_cmpgt_ps(X0, Y0);

    __m128 i1f = _mm_and_ps(cmp, _mm_set1_ps(1.0f));
    __m128 j1f = _mm_andnot_ps(cmp, _mm_set1_ps(1.0f));

    __m128 x1 = _mm_add_ps(_mm_sub_ps(X0, i1f), _mm_set1_ps(G2));
    __m128 y1 = _mm_add_ps(_mm_sub_ps(Y0, j1f), _mm_set1_ps(G2));

    __m128 x2 = _mm_add_ps(_mm_sub_ps(X0, _mm_set1_ps(1.0f)), _mm_set1_ps(2.0f * G2));
    __m128 y2 = _mm_add_ps(_mm_sub_ps(Y0, _mm_set1_ps(1.0f)), _mm_set1_ps(2.0f * G2));

    ALIGN16 int ii[4];
    ALIGN16 int jj[4];

    ALIGN16 float X0a[4];
    ALIGN16 float Y0a[4];

    ALIGN16 float x1a[4];
    ALIGN16 float y1a[4];

    ALIGN16 float x2a[4];
    ALIGN16 float y2a[4];

    ALIGN16 float i1a[4];
    ALIGN16 float j1a[4];

    _mm_store_si128((__m128i*)ii, i);
    _mm_store_si128((__m128i*)jj, j);

    _mm_store_ps(X0a, X0);
    _mm_store_ps(Y0a, Y0);

    _mm_store_ps(x1a, x1);
    _mm_store_ps(y1a, y1);

    _mm_store_ps(x2a, x2);
    _mm_store_ps(y2a, y2);

    _mm_store_ps(i1a, i1f);
    _mm_store_ps(j1a, j1f);

    ALIGN16 float out[4];

    for (int n = 0; n < 4; n++) {
        int i1 = (int)i1a[n];
        int j1 = (int)j1a[n];

        float t0 = 0.5f - X0a[n] * X0a[n] - Y0a[n] * Y0a[n];
        float t1 = 0.5f - x1a[n] * x1a[n] - y1a[n] * y1a[n];
        float t2 = 0.5f - x2a[n] * x2a[n] - y2a[n] * y2a[n];

        float n0 = 0.0f;
        float n1 = 0.0f;
        float n2 = 0.0f;

        if (t0 > 0.0f) {
            t0 *= t0;
            n0 = t0 * t0 * dot2(hash2(ii[n], jj[n]), X0a[n], Y0a[n]);
        }

        if (t1 > 0.0f) {
            t1 *= t1;
            n1 = t1 * t1 * dot2(hash2(ii[n] + i1, jj[n] + j1), x1a[n], y1a[n]);
        }

        if (t2 > 0.0f) {
            t2 *= t2;
            n2 = t2 * t2 * dot2(hash2(ii[n] + 1, jj[n] + 1), x2a[n], y2a[n]);
        }

        out[n] = 70.0f * (n0 + n1 + n2);
    }

    return _mm_load_ps(out);
}

static inline __m128 noise3d_simd(__m128 x, __m128 y, __m128 z) {
    ALIGN16 float xs[4];
    ALIGN16 float ys[4];
    ALIGN16 float zs[4];

    _mm_store_ps(xs, x);
    _mm_store_ps(ys, y);
    _mm_store_ps(zs, z);

    ALIGN16 float out[4];

    for (int n = 0; n < 4; n++) {
        float xf = xs[n];
        float yf = ys[n];
        float zf = zs[n];

        float s = (xf + yf + zf) * F3;

        int i = (int)floorf(xf + s);
        int j = (int)floorf(yf + s);
        int k = (int)floorf(zf + s);

        float t = (float)(i + j + k) * G3;

        float X0 = xf - ((float)i - t);
        float Y0 = yf - ((float)j - t);
        float Z0 = zf - ((float)k - t);

        int i1, j1, k1;
        int i2, j2, k2;

        if (X0 >= Y0) {
            if (Y0 >= Z0) {
                i1 = 1; j1 = 0; k1 = 0;
                i2 = 1; j2 = 1; k2 = 0;
            } else if (X0 >= Z0) {
                i1 = 1; j1 = 0; k1 = 0;
                i2 = 1; j2 = 0; k2 = 1;
            } else {
                i1 = 0; j1 = 0; k1 = 1;
                i2 = 1; j2 = 0; k2 = 1;
            }
        } else {
            if (Y0 < Z0) {
                i1 = 0; j1 = 0; k1 = 1;
                i2 = 0; j2 = 1; k2 = 1;
            } else if (X0 < Z0) {
                i1 = 0; j1 = 1; k1 = 0;
                i2 = 0; j2 = 1; k2 = 1;
            } else {
                i1 = 0; j1 = 1; k1 = 0;
                i2 = 1; j2 = 1; k2 = 0;
            }
        }

        float x1 = X0 - (float)i1 + G3;
        float y1 = Y0 - (float)j1 + G3;
        float z1 = Z0 - (float)k1 + G3;

        float x2 = X0 - (float)i2 + 2.0f * G3;
        float y2 = Y0 - (float)j2 + 2.0f * G3;
        float z2 = Z0 - (float)k2 + 2.0f * G3;

        float x3 = X0 - 1.0f + 3.0f * G3;
        float y3 = Y0 - 1.0f + 3.0f * G3;
        float z3 = Z0 - 1.0f + 3.0f * G3;

        float n0 = 0.0f;
        float n1 = 0.0f;
        float n2 = 0.0f;
        float n3 = 0.0f;

        float t0 = 0.6f - X0 * X0 - Y0 * Y0 - Z0 * Z0;
        if (t0 > 0.0f) {
            t0 *= t0;
            n0 = t0 * t0 * dot3(hash3(i, j, k), X0, Y0, Z0);
        }

        float t1 = 0.6f - x1 * x1 - y1 * y1 - z1 * z1;
        if (t1 > 0.0f) {
            t1 *= t1;
            n1 = t1 * t1 * dot3(hash3(i + i1, j + j1, k + k1), x1, y1, z1);
        }

        float t2 = 0.6f - x2 * x2 - y2 * y2 - z2 * z2;
        if (t2 > 0.0f) {
            t2 *= t2;
            n2 = t2 * t2 * dot3(hash3(i + i2, j + j2, k + k2), x2, y2, z2);
        }

        float t3 = 0.6f - x3 * x3 - y3 * y3 - z3 * z3;
        if (t3 > 0.0f) {
            t3 *= t3;
            n3 = t3 * t3 * dot3(hash3(i + 1, j + 1, k + 1), x3, y3, z3);
        }

        out[n] = 32.0f * (n0 + n1 + n2 + n3);
    }

    return _mm_load_ps(out);
}

float noise2d(float x, float y) {
    return _mm_cvtss_f32(noise2d_simd(_mm_set1_ps(x), _mm_set1_ps(y)));
}

float noise3d(float x, float y, float z) {
    return _mm_cvtss_f32(noise3d_simd(_mm_set1_ps(x), _mm_set1_ps(y), _mm_set1_ps(z)));
}

void noise2d_batch4(const float* xs, const float* ys, float* out) {
    _mm_storeu_ps(out, noise2d_simd(_mm_loadu_ps(xs), _mm_loadu_ps(ys)));
}

void noise3d_batch4(const float* xs, const float* ys, const float* zs, float* out) {
    _mm_storeu_ps(out, noise3d_simd(_mm_loadu_ps(xs), _mm_loadu_ps(ys), _mm_loadu_ps(zs)));
}

float fbm2d(float x, float y, int octaves, float persistence, float lacunarity) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float max_value = 0.0f;

    for (int i = 0; i < octaves; i++) {
        total += noise2d(x * frequency, y * frequency) * amplitude;
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
        total += noise3d(x * frequency, y * frequency, z * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / max_value;
}

void fbm2d_batch4(const float* xs, const float* ys, float* out, int octaves, float persistence, float lacunarity) {
    __m128 total = _mm_setzero_ps();
    __m128 frequency = _mm_set1_ps(1.0f);
    __m128 amplitude = _mm_set1_ps(1.0f);
    float max_value = 0.0f;

    __m128 xv = _mm_loadu_ps(xs);
    __m128 yv = _mm_loadu_ps(ys);

    for (int i = 0; i < octaves; i++) {
        __m128 n = noise2d_simd(_mm_mul_ps(xv, frequency), _mm_mul_ps(yv, frequency));
        total = _mm_add_ps(total, _mm_mul_ps(n, amplitude));
        max_value += _mm_cvtss_f32(amplitude);
        amplitude = _mm_mul_ps(amplitude, _mm_set1_ps(persistence));
        frequency = _mm_mul_ps(frequency, _mm_set1_ps(lacunarity));
    }

    _mm_storeu_ps(out, _mm_div_ps(total, _mm_set1_ps(max_value)));
}

void fbm3d_batch4(const float* xs, const float* ys, const float* zs, float* out, int octaves, float persistence, float lacunarity) {
    __m128 total = _mm_setzero_ps();
    __m128 frequency = _mm_set1_ps(1.0f);
    __m128 amplitude = _mm_set1_ps(1.0f);
    float max_value = 0.0f;

    __m128 xv = _mm_loadu_ps(xs);
    __m128 yv = _mm_loadu_ps(ys);
    __m128 zv = _mm_loadu_ps(zs);

    for (int i = 0; i < octaves; i++) {
        __m128 n = noise3d_simd(_mm_mul_ps(xv, frequency), _mm_mul_ps(yv, frequency), _mm_mul_ps(zv, frequency));
        total = _mm_add_ps(total, _mm_mul_ps(n, amplitude));
        max_value += _mm_cvtss_f32(amplitude);
        amplitude = _mm_mul_ps(amplitude, _mm_set1_ps(persistence));
        frequency = _mm_mul_ps(frequency, _mm_set1_ps(lacunarity));
    }

    _mm_storeu_ps(out, _mm_div_ps(total, _mm_set1_ps(max_value)));
}

float noise_height(int world_x, int world_z, int sea_level) {
    float h = fbm2d(world_x * 0.015f, world_z * 0.015f, 4, 0.5f, 2.0f);
    float detail = fbm2d(world_x * 0.08f, world_z * 0.08f, 2, 0.5f, 2.0f) * 0.25f;
    return (float)(sea_level + (int)((h + detail) * 16.0f));
}