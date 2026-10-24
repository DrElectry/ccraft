#include "font.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LOADED_FONTS 16
#define TAG(a,b,c,d) ((uint32_t)(a)<<24 | (uint32_t)(b)<<16 | (uint32_t)(c)<<8 | (uint32_t)(d))

_Static_assert(sizeof(t_baked_glyph) == sizeof(stbtt_packedchar), "baked glyph layout");

typedef struct {
    char name[64];
    char path[256];
    char style[32];
    unsigned char *data;
    int data_size;
    stbtt_fontinfo info;
    t_ttf_familly family;
    t_ttf_props props;
    t_ttf_glyph glyphs[256];
    uint8_t glyph_ready[256];
    int kern_prev;
    int used;
} LoadedFont;

static LoadedFont g_fonts[MAX_LOADED_FONTS];

static uint16_t ru16(const unsigned char *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static int16_t ri16(const unsigned char *p) {
    return (int16_t)ru16(p);
}

static uint32_t ru32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int32_t ri32(const unsigned char *p) {
    return (int32_t)ru32(p);
}

static uint64_t ru64(const unsigned char *p) {
    return ((uint64_t)ru32(p) << 32) | (uint64_t)ru32(p + 4);
}

static LoadedFont *find_font(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < MAX_LOADED_FONTS; i++) {
        if (g_fonts[i].used && strcmp(g_fonts[i].name, name) == 0)
            return &g_fonts[i];
    }
    return NULL;
}

static LoadedFont *as_font(void *font) {
    return (LoadedFont *)font;
}

static const unsigned char *find_table(const unsigned char *data, uint32_t tag) {
    uint16_t n = ru16(data + 4);
    const unsigned char *rec = data + 12;
    for (uint16_t i = 0; i < n; i++) {
        if (ru32(rec) == tag)
            return data + ru32(rec + 8);
        rec += 16;
    }
    return NULL;
}

static char *utf16be_to_ascii(const unsigned char *s, int len) {
    int n = len / 2;
    char *out = (char *)malloc((size_t)n + 1);
    if (!out) return NULL;
    int w = 0;
    for (int i = 0; i < n; i++) {
        unsigned char hi = s[i * 2];
        unsigned char lo = s[i * 2 + 1];
        if (hi == 0 && lo >= 32 && lo < 127)
            out[w++] = (char)lo;
        else if (hi == 0)
            out[w++] = '?';
    }
    out[w] = '\0';
    return out;
}

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

static void parse_name_table(LoadedFont *f) {
    const unsigned char *name = find_table(f->data, TAG('n','a','m','e'));
    if (!name) {
        f->family.font_family = dupstr(f->name);
        f->family.font_sub_family = dupstr(f->style);
        f->family.full_name = dupstr(f->name);
        f->family.postscript_name = dupstr(f->name);
        return;
    }

    f->family.format = ru16(name);
    uint16_t count = ru16(name + 2);
    uint16_t storage = ru16(name + 4);
    const unsigned char *rec = name + 6;

    char *found[8] = {0};

    for (uint16_t i = 0; i < count; i++) {
        uint16_t plat = ru16(rec);
        uint16_t enc = ru16(rec + 2);
        uint16_t lang = ru16(rec + 4);
        uint16_t id = ru16(rec + 6);
        uint16_t len = ru16(rec + 8);
        uint16_t off = ru16(rec + 10);
        rec += 12;

        if (id > 6) continue;
        if (!(plat == 3 && enc == 1) && !(plat == 0)) continue;
        if (plat == 3 && lang != 0x0409 && found[id]) continue;

        char *s = utf16be_to_ascii(name + storage + off, len);
        if (!s) continue;
        free(found[id]);
        found[id] = s;
        (void)lang;
    }

    f->family.font_family = found[1] ? found[1] : dupstr(f->name);
    f->family.font_sub_family = found[2] ? found[2] : dupstr(f->style);
    f->family.full_name = found[4] ? found[4] : dupstr(f->family.font_family);
    f->family.postscript_name = found[6] ? found[6] : dupstr(f->name);

    for (int i = 0; i < 8; i++) {
        if (i != 1 && i != 2 && i != 4 && i != 6)
            free(found[i]);
    }
}

static void parse_props(LoadedFont *f) {
    const unsigned char *d = f->data;
    t_ttf_props *p = &f->props;
    memset(p, 0, sizeof(*p));

    p->scalar_type = ru32(d);
    p->num_tables = ru16(d + 4);
    p->search_range = ru16(d + 6);
    p->entry_selector = ru16(d + 8);
    p->range_shift = ru16(d + 10);

    const unsigned char *head = find_table(d, TAG('h','e','a','d'));
    if (head) {
        p->version = ri32(head);
        p->font_revision = ri32(head + 4);
        p->checksum_adjustment = ru32(head + 8);
        p->magic_number = ru32(head + 12);
        p->flags = ru16(head + 16);
        p->units_per_em = ru16(head + 18);
        p->created = ru64(head + 20);
        p->modified = ru64(head + 28);
        p->x_min = ri16(head + 36);
        p->y_min = ri16(head + 38);
        p->x_max = ri16(head + 40);
        p->y_max = ri16(head + 42);
        p->mac_style = ru16(head + 44);
        p->lowest_rec_ppem = ru16(head + 46);
        p->font_direction_hint = ri16(head + 48);
        p->index_to_loc_format = ri16(head + 50);
        p->glyph_data_format = ri16(head + 52);
    }

    const unsigned char *hhea = find_table(d, TAG('h','h','e','a'));
    if (hhea) {
        p->ascent = ri16(hhea + 4);
        p->descent = ri16(hhea + 6);
        p->line_gap = ri16(hhea + 8);
        p->advance_width_max = ru16(hhea + 10);
        p->min_left_side_bearing = ri16(hhea + 12);
        p->min_right_side_bearing = ri16(hhea + 14);
        p->x_max_extent = ri16(hhea + 16);
        p->caret_slope_rise = ri16(hhea + 18);
        p->caret_slope_run = ri16(hhea + 20);
        p->caret_offset = ri16(hhea + 22);
        p->metric_data_format = ri16(hhea + 32);
        p->num_of_long_hor_metrics = ru16(hhea + 34);
    }
}

static void free_family(t_ttf_familly *fam) {
    free(fam->font_family);
    free(fam->font_sub_family);
    free(fam->full_name);
    free(fam->postscript_name);
    memset(fam, 0, sizeof(*fam));
}

static unsigned char *read_file(const char *path, int *out_size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    long sz = ftell(fp);
    if (sz <= 0) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);
    unsigned char *buf = (unsigned char *)malloc((size_t)sz);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (n != (size_t)sz) {
        free(buf);
        return NULL;
    }
    *out_size = (int)sz;
    return buf;
}

int create_new_font(const char *path, char *name, const char *style) {
    if (!path || !name) return 0;
    if (find_font(name))
        delete_font(name);

    LoadedFont *slot = NULL;
    for (int i = 0; i < MAX_LOADED_FONTS; i++) {
        if (!g_fonts[i].used) {
            slot = &g_fonts[i];
            break;
        }
    }
    if (!slot) return 0;

    memset(slot, 0, sizeof(*slot));
    strncpy(slot->name, name, sizeof(slot->name) - 1);
    strncpy(slot->path, path, sizeof(slot->path) - 1);
    strncpy(slot->style, style ? style : "regular", sizeof(slot->style) - 1);

    slot->data = read_file(path, &slot->data_size);
    if (!slot->data) return 0;

    if (slot->data_size < 12 || !stbtt_InitFont(&slot->info, slot->data, stbtt_GetFontOffsetForIndex(slot->data, 0))) {
        free(slot->data);
        memset(slot, 0, sizeof(*slot));
        return 0;
    }

    parse_name_table(slot);
    parse_props(slot);
    slot->used = 1;
    return 1;
}

int delete_font(const char *name) {
    LoadedFont *f = find_font(name);
    if (!f) return 0;
    free_family(&f->family);
    free(f->data);
    memset(f, 0, sizeof(*f));
    return 1;
}

void *get_font(const char *name) {
    return find_font(name);
}

const char *get_font_path(const char *name) {
    LoadedFont *f = find_font(name);
    return f ? f->path : NULL;
}

t_ttf_familly *get_font_family(const char *name) {
    LoadedFont *f = find_font(name);
    return f ? &f->family : NULL;
}

t_ttf_props *get_font_props(const char *name) {
    LoadedFont *f = find_font(name);
    return f ? &f->props : NULL;
}

t_ttf_familly *font_family(void *font) {
    LoadedFont *f = as_font(font);
    return f ? &f->family : NULL;
}

t_ttf_props *font_props(void *font) {
    LoadedFont *f = as_font(font);
    return f ? &f->props : NULL;
}

int glyph_index(void *font, uint8_t c) {
    LoadedFont *f = as_font(font);
    if (!f) return 0;
    return stbtt_FindGlyphIndex(&f->info, c);
}

uint16_t horizontal_metrics(void *font, uint16_t glyph) {
    LoadedFont *f = as_font(font);
    if (!f) return 0;
    int adv = 0, lsb = 0;
    stbtt_GetGlyphHMetrics(&f->info, glyph, &adv, &lsb);
    (void)lsb;
    return (uint16_t)adv;
}

t_pointi2 reset_kerns(void *font) {
    LoadedFont *f = as_font(font);
    t_pointi2 z = {0, 0};
    if (f) f->kern_prev = 0;
    return z;
}

t_pointi2 next_kern(void *font, uint16_t glyph) {
    LoadedFont *f = as_font(font);
    t_pointi2 k = {0, 0};
    if (!f) return k;
    if (f->kern_prev)
        k.x = stbtt_GetGlyphKernAdvance(&f->info, f->kern_prev, glyph);
    f->kern_prev = glyph;
    return k;
}

t_ttf_glyph *get_glyph(const char *name, char c) {
    LoadedFont *f = find_font(name);
    if (!f) return NULL;
    unsigned char uc = (unsigned char)c;
    if (!f->glyph_ready[uc]) {
        t_ttf_glyph *g = &f->glyphs[uc];
        memset(g, 0, sizeof(*g));
        g->index = stbtt_FindGlyphIndex(&f->info, uc);
        int x0, y0, x1, y1;
        stbtt_GetGlyphBox(&f->info, g->index, &x0, &y0, &x1, &y1);
        g->x_min = (int16_t)x0;
        g->y_min = (int16_t)y0;
        g->x_max = (int16_t)x1;
        g->y_max = (int16_t)y1;
        int adv = 0, lsb = 0;
        stbtt_GetGlyphHMetrics(&f->info, g->index, &adv, &lsb);
        g->advance_width = (uint16_t)adv;
        g->left_side_bearing = (int16_t)lsb;
        f->glyph_ready[uc] = 1;
    }
    return &f->glyphs[uc];
}

float font_pixel_scale(void *font, float pixel_height) {
    LoadedFont *f = as_font(font);
    if (!f) return 0.0f;
    return stbtt_ScaleForPixelHeight(&f->info, pixel_height);
}

void font_vmetrics(void *font, float pixel_height, float *ascent, float *descent, float *line_gap) {
    LoadedFont *f = as_font(font);
    if (!f) return;
    int a = 0, d = 0, g = 0;
    stbtt_GetFontVMetrics(&f->info, &a, &d, &g);
    float s = stbtt_ScaleForPixelHeight(&f->info, pixel_height);
    if (ascent) *ascent = (float)a * s;
    if (descent) *descent = (float)d * s;
    if (line_gap) *line_gap = (float)g * s;
}

float font_kern(void *font, int ch0, int ch1, float pixel_height) {
    LoadedFont *f = as_font(font);
    if (!f) return 0.0f;
    float s = stbtt_ScaleForPixelHeight(&f->info, pixel_height);
    return s * (float)stbtt_GetCodepointKernAdvance(&f->info, ch0, ch1);
}

unsigned char *font_glyph_bitmap(void *font, int codepoint, float pixel_height, int *w, int *h, int *xoff, int *yoff) {
    LoadedFont *f = as_font(font);
    if (!f) return NULL;
    float s = stbtt_ScaleForPixelHeight(&f->info, pixel_height);
    return stbtt_GetCodepointBitmap(&f->info, s, s, codepoint, w, h, xoff, yoff);
}

int font_bake(void *font, float pixel_height, unsigned char *pixels, int pw, int ph,
              int first_char, int num_chars, t_baked_glyph *out) {
    LoadedFont *f = as_font(font);
    if (!f || !pixels || !out || pw <= 0 || ph <= 0 || num_chars <= 0)
        return 0;

    memset(pixels, 0, (size_t)pw * (size_t)ph);
    memset(out, 0, (size_t)num_chars * sizeof(t_baked_glyph));

    stbtt_pack_context pc;
    if (!stbtt_PackBegin(&pc, pixels, pw, ph, 0, 1, NULL))
        return 0;

    stbtt_PackSetOversampling(&pc, 2, 2);

    stbtt_pack_range range;
    memset(&range, 0, sizeof(range));
    range.font_size = pixel_height;
    range.first_unicode_codepoint_in_range = first_char;
    range.num_chars = num_chars;
    range.chardata_for_range = (stbtt_packedchar *)out;

    int ok = stbtt_PackFontRanges(&pc, f->data, 0, &range, 1);
    stbtt_PackEnd(&pc);
    return ok;
}

void font_quad(t_baked_glyph *glyphs, int pw, int ph, int index,
               float *xpos, float *ypos, t_glyph_quad *q, int align) {
    stbtt_aligned_quad aq;
    stbtt_GetPackedQuad((stbtt_packedchar *)glyphs, pw, ph, index, xpos, ypos, &aq, align);
    q->x0 = aq.x0; q->y0 = aq.y0; q->x1 = aq.x1; q->y1 = aq.y1;
    q->s0 = aq.s0; q->t0 = aq.t0; q->s1 = aq.s1; q->t1 = aq.t1;
}

uint16_t text_width(void *font, const char *str, uint32_t size) {
    LoadedFont *f = as_font(font);
    if (!f || !str) return 0;
    float s = stbtt_ScaleForPixelHeight(&f->info, (float)size);
    float w = 0.0f;
    int prev = 0;
    for (const unsigned char *p = (const unsigned char *)str; *p; p++) {
        int adv = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&f->info, *p, &adv, &lsb);
        w += (float)adv * s;
        if (prev)
            w += (float)stbtt_GetCodepointKernAdvance(&f->info, prev, *p) * s;
        prev = *p;
    }
    return (uint16_t)(w + 0.5f);
}

uint16_t text_height(void *font, const char *str, uint32_t size) {
    (void)str;
    float a = 0, d = 0, g = 0;
    font_vmetrics(font, (float)size, &a, &d, &g);
    float h = a - d + g;
    if (h < 0) h = -h;
    return (uint16_t)(h + 0.5f);
}

t_img *ft_new_image(void *mlx, int w, int h) {
    (void)mlx;
    if (w <= 0 || h <= 0) return NULL;
    t_img *img = (t_img *)calloc(1, sizeof(t_img));
    if (!img) return NULL;
    img->width = w;
    img->height = h;
    img->bits_per_pixel = 32;
    img->line_length = w * 4;
    img->size = h * img->line_length;
    img->max_addr = img->size;
    img->addr = (char *)calloc(1, (size_t)img->size);
    if (!img->addr) {
        free(img);
        return NULL;
    }
    return img;
}

void ft_delete_image(void *mlx, t_img **img) {
    (void)mlx;
    if (!img || !*img) return;
    free((*img)->addr);
    free(*img);
    *img = NULL;
}

void ft_pixel_put(t_img *img, int x, int y, uint32_t color) {
    if (!img || !img->addr) return;
    if (x < 0 || y < 0 || x >= img->width || y >= img->height) return;
    uint32_t *px = (uint32_t *)(img->addr + y * img->line_length + x * 4);
    *px = color;
}

uint32_t ft_get_pixel(t_img *img, int x, int y) {
    if (!img || !img->addr) return 0;
    if (x < 0 || y < 0 || x >= img->width || y >= img->height) return 0;
    uint32_t *px = (uint32_t *)(img->addr + y * img->line_length + x * 4);
    return *px;
}

t_img *ft_create_string(void *mlx, void *image, const char *text,
                        uint32_t size, uint32_t color, const char *font_name, int *width, int *height) {
    (void)image;
    LoadedFont *f = find_font(font_name);
    if (!f || !text) return NULL;

    int w = (int)text_width(f, text, size);
    int h = (int)text_height(f, text, size);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (width) *width = w;
    if (height) *height = h;

    t_img *img = ft_new_image(mlx, w, h);
    if (!img) return NULL;

    float scale = stbtt_ScaleForPixelHeight(&f->info, (float)size);
    float ascent, descent, gap;
    font_vmetrics(f, (float)size, &ascent, &descent, &gap);
    (void)descent;
    (void)gap;

    int baseline = (int)(ascent + 0.5f);
    float x = 0.0f;
    int prev = 0;

    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (prev)
            x += (float)stbtt_GetCodepointKernAdvance(&f->info, prev, *p) * scale;

        int gw, gh, xoff, yoff;
        unsigned char *bmp = stbtt_GetCodepointBitmap(&f->info, scale, scale, *p, &gw, &gh, &xoff, &yoff);
        if (bmp) {
            int ox = (int)x + xoff;
            int oy = baseline + yoff;
            for (int gy = 0; gy < gh; gy++) {
                for (int gx = 0; gx < gw; gx++) {
                    unsigned char a = bmp[gy * gw + gx];
                    if (!a) continue;
                    uint32_t src = (color & 0x00FFFFFFu) | ((uint32_t)a << 24);
                    ft_pixel_put(img, ox + gx, oy + gy, src);
                }
            }
            stbtt_FreeBitmap(bmp, NULL);
        }

        int adv = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&f->info, *p, &adv, &lsb);
        x += (float)adv * scale;
        prev = *p;
    }

    return img;
}
