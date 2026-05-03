#include "noise.h"
#include <immintrin.h>
#include <math.h>
#include <stdint.h>

// Alignment macro for portability
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

static inline uint32_t hash_int(uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
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
    return grad3d[gi % 12][0] * x + grad3d[gi % 12][1] * y + grad3d[gi % 12][2] * z;
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

    // Declare arrays with proper alignment
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

    float out[4];

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

float noise2d(float x, float y) {
    __m128 xv = _mm_set1_ps(x);
    __m128 yv = _mm_set1_ps(y);
    __m128 r = noise2d_simd(xv, yv);
    return _mm_cvtss_f32(r);
}

void noise2d_batch4(const float* xs, const float* ys, float* out) {
    __m128 xv = _mm_loadu_ps(xs);
    __m128 yv = _mm_loadu_ps(ys);
    __m128 r = noise2d_simd(xv, yv);
    _mm_storeu_ps(out, r);
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

void fbm2d_batch4(
    const float* xs,
    const float* ys,
    float* out,
    int octaves,
    float persistence,
    float lacunarity
) {
    __m128 total = _mm_setzero_ps();
    __m128 frequency = _mm_set1_ps(1.0f);
    __m128 amplitude = _mm_set1_ps(1.0f);

    float max_value = 0.0f;

    __m128 xv = _mm_loadu_ps(xs);
    __m128 yv = _mm_loadu_ps(ys);

    for (int i = 0; i < octaves; i++) {
        __m128 nx = _mm_mul_ps(xv, frequency);
        __m128 ny = _mm_mul_ps(yv, frequency);

        __m128 n = noise2d_simd(nx, ny);

        total = _mm_add_ps(total, _mm_mul_ps(n, amplitude));

        max_value += _mm_cvtss_f32(amplitude);

        amplitude = _mm_mul_ps(amplitude, _mm_set1_ps(persistence));
        frequency = _mm_mul_ps(frequency, _mm_set1_ps(lacunarity));
    }

    total = _mm_div_ps(total, _mm_set1_ps(max_value));

    _mm_storeu_ps(out, total);
}

float noise_height(int world_x, int world_z, int sea_level) {
    float h = fbm2d((float)world_x * 0.015f, (float)world_z * 0.015f, 4, 0.5f, 2.0f);
    float detail = fbm2d((float)world_x * 0.08f, (float)world_z * 0.08f, 2, 0.5f, 2.0f) * 0.25f;
    return (float)(sea_level + (int)((h + detail) * 16.0f));
}

void noise_height_batch4(
    const int* world_x,
    const int* world_z,
    float* out,
    int sea_level
) {
    float xs0[4];
    float zs0[4];
    float xs1[4];
    float zs1[4];

    for (int i = 0; i < 4; i++) {
        xs0[i] = (float)world_x[i] * 0.015f;
        zs0[i] = (float)world_z[i] * 0.015f;
        xs1[i] = (float)world_x[i] * 0.08f;
        zs1[i] = (float)world_z[i] * 0.08f;
    }

    float h[4];
    float d[4];

    fbm2d_batch4(xs0, zs0, h, 4, 0.5f, 2.0f);
    fbm2d_batch4(xs1, zs1, d, 2, 0.5f, 2.0f);

    for (int i = 0; i < 4; i++) {
        out[i] = (float)(sea_level + (int)((h[i] + d[i] * 0.25f) * 16.0f));
    }
}