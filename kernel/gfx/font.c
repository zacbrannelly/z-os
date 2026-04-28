#include "font.h"
#include "fonts/roboto_medium.ttf.h"
#include "fonts/roboto_mono.ttf.h"

#include "gfx.h"
#include "../console.h"
#include "../kmalloc.h"

#include <stdint.h>
#include <stddef.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../3rdparty/stb_truetype_impl.h"
#include "../3rdparty/stb_truetype.h"

#define ATLAS_WIDTH 1024
#define ATLAS_HEIGHT 1024
#define ATLAS_NUM_CHARS 96
#define ATLAS_FIRST_CHAR 32
#define ATLAS_PADDING 1
#define ATLAS_OVERSAMPLING_X 2
#define ATLAS_OVERSAMPLING_Y 1

#define DEFAULT_FONT_DATA  (uint8_t*)g_roboto_mono_ttf_data
#define DEFAULT_FONT_INDEX 0
#define DEFAULT_FONT_SIZE  18

typedef struct font_atlas_t {
    uint8_t *ttf_data;
    uint8_t atlas[ATLAS_WIDTH * ATLAS_HEIGHT];
    stbtt_packedchar packed_chars[96];
    float font_size;
    uint32_t font_index;

    stbtt_fontinfo info;
    float scale;
    float ascent;
    float descent;
    float line_gap;
    float line_height;
} font_atlas_t;

static font_atlas_t g_default_font;

static int load_font(font_atlas_t *font, uint8_t *ttf_data, float font_size, uint32_t font_index) {
    font->ttf_data = ttf_data;
    font->font_size = font_size;
    font->font_index = font_index;


    int font_offset = stbtt_GetFontOffsetForIndex(ttf_data, font->font_index);
    if (!stbtt_InitFont(&font->info, ttf_data, font_offset)) {
        console_write("Failed to initialize font\r\n");
        return -1;
    }

    font->scale = stbtt_ScaleForPixelHeight(&font->info, font_size);

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(
        &font->info,
        &ascent,
        &descent,
        &line_gap
    );

    font->ascent = ascent * font->scale;
    font->descent = descent * font->scale;
    font->line_gap = line_gap * font->scale;
    font->line_height = (ascent - descent + line_gap) * font->scale;

    stbtt_pack_context pc;
    if (!stbtt_PackBegin(
        &pc,
        font->atlas,
        ATLAS_WIDTH,
        ATLAS_HEIGHT,
        0, // stride_in_bytes, 0 means width
        ATLAS_PADDING,
        NULL // alloc context
    )) {
        console_write("Failed to pack font\r\n");
        return -1;
    }

    stbtt_PackSetOversampling(&pc, ATLAS_OVERSAMPLING_X, ATLAS_OVERSAMPLING_Y);

    if (!stbtt_PackFontRange(
        &pc,
        font->ttf_data,
        font->font_index,
        font->font_size,
        ATLAS_FIRST_CHAR,
        ATLAS_NUM_CHARS,
        font->packed_chars
    )) {
        console_write("Failed to pack font range\r\n");
        return -1;
    }

    stbtt_PackEnd(&pc);
    return 0;
}

static int load_default_font(void) {
    return load_font(&g_default_font, DEFAULT_FONT_DATA, DEFAULT_FONT_SIZE, DEFAULT_FONT_INDEX);
}

int font_init(void) {
    memory_set(&g_default_font, 0, sizeof(font_atlas_t)); 

    if (load_default_font() < 0) {
        console_write("Failed to load default font\r\n");
        return -1;
    }

    return 0;
}

int font_get_line_height(void) {
    return g_default_font.line_height;
}

int font_get_ascent(void) {
    return g_default_font.ascent;
}

int font_get_descent(void) {
    return g_default_font.descent;
}

int font_get_line_gap(void) {
    return g_default_font.line_gap;
}

int font_calculate_cursor_pos(
    const char *text,
    uint32_t glyph_index,
    int32_t x,
    int32_t baseline,
    int32_t *out_cursor_x,
    int32_t *out_cursor_y
) {
    uint32_t cursor_x = x;
    uint32_t cursor_y = baseline;

    if (glyph_index > strlen(text)) {
        console_write("font_get_glyph_position: Index out of bounds\r\n");
        return -1;
    }

    for (uint32_t i = 0; i <= glyph_index; i++) {
        int codepoint = text[i];

        if (codepoint == '\n') {
            cursor_x = x;
            cursor_y += g_default_font.line_height;

            *out_cursor_x = cursor_x;
            *out_cursor_y = cursor_y;
            continue;
        }

        if (codepoint < ATLAS_FIRST_CHAR || codepoint > ATLAS_FIRST_CHAR + ATLAS_NUM_CHARS) continue;

        int glyph_index = codepoint - ATLAS_FIRST_CHAR;
        const stbtt_packedchar *chardata = &g_default_font.packed_chars[glyph_index];

        *out_cursor_x = cursor_x;
        *out_cursor_y = cursor_y;
        cursor_x += chardata->xadvance;
    }
    return 0;
}

int font_draw_text(const char *text, int32_t x, int32_t baseline, uint32_t color) {
    int32_t cursor_x = x;
    int32_t cursor_y = baseline;

    for (uint32_t i = 0; i < strlen(text); i++) {
        int codepoint = text[i];

        if (codepoint == '\n') {
            cursor_x = x;
            cursor_y += g_default_font.line_height;
            continue;
        }

        if (codepoint < ATLAS_FIRST_CHAR || codepoint > ATLAS_FIRST_CHAR + ATLAS_NUM_CHARS) continue;

        int glyph_index = codepoint - ATLAS_FIRST_CHAR;
        const stbtt_packedchar *chardata = &g_default_font.packed_chars[glyph_index];

        int src_x0 = chardata->x0;
        int src_y0 = chardata->y0;
        int src_width = chardata->x1 - src_x0;
        int src_height = chardata->y1 - src_y0;

        int dst_x0 = cursor_x + chardata->xoff;
        int dst_y0 = cursor_y + chardata->yoff;
        int dst_width = chardata->xoff2 - chardata->xoff;
        int dst_height = chardata->yoff2 - chardata->yoff;

        gfx_draw_alpha_bitmap_scaled(
            g_default_font.atlas,
            ATLAS_WIDTH,
            src_x0,
            src_y0,
            src_width,
            src_height,
            dst_x0,
            dst_y0,
            dst_width,
            dst_height,
            color
        );

        cursor_x += chardata->xadvance;
    }
    return 0;
}
