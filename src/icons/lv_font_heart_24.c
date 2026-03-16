/*******************************************************************************
 * Size: 24 px
 * Bpp: 4
 * Opts: --font E:\PlatformIO\ESP32\MaterialSymbols-Simple-24.ttf --size 24 --bpp 4 --format lvgl --range 0xe87d --no-compress -o E:\PlatformIO\ESP32\src\icons\lv_font_heart_24.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_FONT_HEART_24
#define LV_FONT_HEART_24 1
#endif

#if LV_FONT_HEART_24

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+E87D "" */
    0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x10,
    0x0, 0x0, 0x0, 0x4c, 0xff, 0xfc, 0x40, 0x4,
    0xcf, 0xff, 0xc4, 0x0, 0x7, 0xff, 0xfe, 0xff,
    0xf8, 0x8f, 0xff, 0xef, 0xff, 0x70, 0x3f, 0xf8,
    0x0, 0x7, 0xff, 0xff, 0x70, 0x0, 0x8f, 0xf3,
    0xbf, 0x90, 0x0, 0x0, 0x5f, 0xf5, 0x0, 0x0,
    0x9, 0xfa, 0xef, 0x20, 0x0, 0x0, 0x3, 0x30,
    0x0, 0x0, 0x2, 0xfe, 0xff, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0xff, 0xef, 0x20,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0xfe,
    0x9f, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x8, 0xf9, 0x3f, 0xf2, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x2f, 0xf3, 0x9, 0xfd, 0x10, 0x0,
    0x0, 0x0, 0x0, 0x1, 0xdf, 0x90, 0x0, 0xdf,
    0xb0, 0x0, 0x0, 0x0, 0x0, 0xc, 0xfd, 0x0,
    0x0, 0x1e, 0xfb, 0x0, 0x0, 0x0, 0x0, 0xbf,
    0xe1, 0x0, 0x0, 0x2, 0xef, 0xb0, 0x0, 0x0,
    0xb, 0xfe, 0x20, 0x0, 0x0, 0x0, 0x2e, 0xfb,
    0x0, 0x0, 0xbf, 0xe2, 0x0, 0x0, 0x0, 0x0,
    0x2, 0xef, 0xc1, 0x1c, 0xfe, 0x20, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x2d, 0xfd, 0xdf, 0xd1, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0xcf, 0xfc,
    0x10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0xb, 0xb0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 384, .box_w = 20, .box_h = 20, .ofs_x = 2, .ofs_y = 2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 59517, .range_length = 1, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t lv_font_heart_24 = {
#else
lv_font_t lv_font_heart_24 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 20,          /*The maximum line height required by the font*/
    .base_line = -2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if LV_FONT_HEART_24*/

