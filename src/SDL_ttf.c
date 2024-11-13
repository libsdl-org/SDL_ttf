/*
  SDL_ttf:  A companion library to SDL for working with TrueType (tm) fonts
  Copyright (C) 2001-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_ttf/SDL_textengine.h>

#include "SDL_hashtable.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_STROKER_H
#include FT_GLYPH_H
#include FT_TRUETYPE_IDS_H
#include FT_IMAGE_H

/* Enable rendering with color
 * Freetype may need to be compiled with FT_CONFIG_OPTION_USE_PNG */
#if defined(FT_HAS_COLOR)
#  define TTF_USE_COLOR 1
#else
#  define TTF_USE_COLOR 0
#endif

// Enable Signed Distance Field rendering (requires latest FreeType version)
#if defined(FT_RASTER_FLAG_SDF)
#  define TTF_USE_SDF 1
#else
#  define TTF_USE_SDF 0
#endif

#if TTF_USE_SDF
#include FT_MODULE_H
#endif

/* Enable HarfBuzz for Complex text rendering
 * Freetype may need to be compiled with FT_CONFIG_OPTION_USE_HARFBUZZ */
#ifndef TTF_USE_HARFBUZZ
#  define TTF_USE_HARFBUZZ 0
#endif

#ifndef TTF_DEFAULT_DPI
#define TTF_DEFAULT_DPI 72
#endif

/**
 * ZERO WIDTH NO-BREAKSPACE (Unicode byte order mark)
 */
#define UNICODE_BOM_NATIVE  0xFEFF
#define UNICODE_BOM_SWAPPED 0xFFFE

#if defined(SDL_BUILD_MAJOR_VERSION)
SDL_COMPILE_TIME_ASSERT(SDL_BUILD_MAJOR_VERSION,
                        SDL_TTF_MAJOR_VERSION == SDL_BUILD_MAJOR_VERSION);
SDL_COMPILE_TIME_ASSERT(SDL_BUILD_MINOR_VERSION,
                        SDL_TTF_MINOR_VERSION == SDL_BUILD_MINOR_VERSION);
SDL_COMPILE_TIME_ASSERT(SDL_BUILD_MICRO_VERSION,
                        SDL_TTF_MICRO_VERSION == SDL_BUILD_MICRO_VERSION);
#endif

// Limited by its encoding in SDL_VERSIONNUM
SDL_COMPILE_TIME_ASSERT(SDL_TTF_MAJOR_VERSION_min, SDL_TTF_MAJOR_VERSION >= 0);
SDL_COMPILE_TIME_ASSERT(SDL_TTF_MAJOR_VERSION_max, SDL_TTF_MAJOR_VERSION <= 10);
SDL_COMPILE_TIME_ASSERT(SDL_TTF_MINOR_VERSION_min, SDL_TTF_MINOR_VERSION >= 0);
SDL_COMPILE_TIME_ASSERT(SDL_TTF_MINOR_VERSION_max, SDL_TTF_MINOR_VERSION <= 999);
SDL_COMPILE_TIME_ASSERT(SDL_TTF_MICRO_VERSION_min, SDL_TTF_MICRO_VERSION >= 0);
SDL_COMPILE_TIME_ASSERT(SDL_TTF_MICRO_VERSION_max, SDL_TTF_MICRO_VERSION <= 999);

#if TTF_USE_HARFBUZZ
#include <hb.h>
#include <hb-ft.h>
#endif

// Round glyph to 16 bytes width and use SSE2 instructions
#if defined(__SSE2__)
#  define HAVE_SSE2_INTRINSICS 1
#include <emmintrin.h>
#endif

// Round glyph width to 16 bytes use NEON instructions
#if defined(__ARM_NEON)
#  define HAVE_NEON_INTRINSICS 1
#include <arm_neon.h>
#endif

// Round glyph width to 8 bytes
#define HAVE_BLIT_GLYPH_64

/* Android armeabi-v7a doesn't like int64 (Maybe all other __ARM_ARCH < 7 ?),
 * un-activate it, especially if NEON isn't detected */
#if defined(__ARM_ARCH)
#  if __ARM_ARCH < 8
#    if defined(HAVE_BLIT_GLYPH_64)
#      undef HAVE_BLIT_GLYPH_64
#    endif
#  endif
#endif

// Default: round glyph width to 4 bytes to copy them faster
#define HAVE_BLIT_GLYPH_32

// Use Duff's device to unroll loops
//#define USE_DUFFS_LOOP

#if defined(HAVE_SSE2_INTRINSICS)
static SDL_INLINE int hasSSE2(void)
{
    static int val = -1;
    if (val != -1) {
        return val;
    }
    val = SDL_HasSSE2();
    return val;
}
#endif

#if defined(HAVE_NEON_INTRINSICS)
static SDL_INLINE int hasNEON(void)
{
    static int val = -1;
    if (val != -1) {
        return val;
    }
    val = SDL_HasNEON();
    return val;
}
#endif

/* FIXME: Right now we assume the gray-scale renderer Freetype is using
   supports 256 shades of gray, but we should instead key off of num_grays
   in the result FT_Bitmap after the FT_Render_Glyph() call. */
#define NUM_GRAYS       256

/* x offset = cos(((90.0-12)/360) * 2 * M_PI), or 12 degree angle */
// same value as in FT_GlyphSlot_Oblique, fixed point 16.16
#define GLYPH_ITALICS  0x0366AL

// Handy routines for converting from fixed point 26.6
#define FT_FLOOR(X) (int)(((X) & -64) / 64)
#define FT_CEIL(X)  FT_FLOOR((X) + 63)

// Handy routine for converting to fixed point 26.6
#define F26Dot6(X)  ((X) << 6)

/* Faster divide by 255, with same result
 * in range [0; 255]:  (x + 1   + (x >> 8)) >> 8
 * in range [-255; 0]: (x + 255 + (x >> 8)) >> 8 */
#define DIVIDE_BY_255_SIGNED(x, sign_val)  (((x) + (sign_val) + ((x)>>8)) >> 8)

// When x positive
#define DIVIDE_BY_255(x)    DIVIDE_BY_255_SIGNED(x, 1)


#define CACHED_METRICS  0x20

#define CACHED_BITMAP   0x01
#define CACHED_PIXMAP   0x02
#define CACHED_COLOR    0x04
#define CACHED_LCD      0x08
#define CACHED_SUBPIX   0x10


typedef struct {
    unsigned char *buffer; // aligned
    int            left;
    int            top;
    int            width;
    int            rows;
    int            pitch;
    int            is_color;
} TTF_Image;

// Cached glyph information
typedef struct cached_glyph {
    int stored;
    FT_UInt index;
    TTF_Image bitmap;
    TTF_Image pixmap;
    int sz_left;
    int sz_top;
    int sz_width;
    int sz_rows;
    int advance;
    union {
        // TTF_HINTING_LIGHT_SUBPIXEL (only pixmap)
        struct {
            int lsb_minus_rsb;
            int translation;
        } subpixel;
        // Other hinting
        struct {
            int rsb_delta;
            int lsb_delta;
        } kerning_smart;
    };
} c_glyph;

/* Internal buffer to store positions computed by TTF_Size_Internal()
 * for rendered string by Render_Line() */
typedef struct PosBuf {
    FT_UInt index;
    int x;
    int y;
    int offset;
} PosBuf_t;

// The structure used to hold internal font information
struct TTF_Font {
    // Freetype2 maintains all sorts of useful info itself
    FT_Face face;

    // Properties exposed to the application
    SDL_PropertiesID props;

    // The current font generation, changes when glyphs need to be rebuilt
    Uint32 generation;

    // Text objects using this font
    SDL_HashTable *text;

    // We'll cache these ourselves
    float ptsize;
    int hdpi;
    int vdpi;
    int height;
    int ascent;
    int descent;
    int lineskip;

    // The font style
    int style;
    int outline;
    FT_Stroker stroker;

    // Whether kerning is desired
    bool enable_kerning;
#if !TTF_USE_HARFBUZZ
    bool use_kerning;
#endif

    // Extra width in glyph bounds for text styles
    int glyph_overhang;

    // Information in the font for underlining
    int line_thickness;
    int underline_top_row;
    int strikethrough_top_row;

    // Cache for style-transformed glyphs
    c_glyph cache[256];
    FT_UInt cache_index[128];

    // We are responsible for closing the font stream
    SDL_IOStream *src;
    Sint64 src_offset;
    bool closeio;
    FT_Open_Args args;

    /* Internal buffer to store positions computed by TTF_Size_Internal()
     * for rendered string by Render_Line() */
    PosBuf_t *pos_buf;
    int pos_len;
    int pos_max;
    int num_clusters;

    // Hinting modes
    int ft_load_target;
    int render_subpixel;
#if TTF_USE_HARFBUZZ
    hb_font_t *hb_font;
    // If HB_SCRIPT_INVALID, use global default script
    hb_script_t hb_script;
    // If HB_DIRECTION_INVALID, use global default direction
    hb_direction_t hb_direction;
    hb_language_t hb_language;
#endif
    bool render_sdf;

    // Extra layout setting for wrapped text
    TTF_HorizontalAlignment horizontal_align;
};

typedef struct
{
    const char *text;
    size_t length;
} TTF_Line;

// Tell if SDL_ttf has to handle the style
#define TTF_HANDLE_STYLE_BOLD(font)          ((font)->style & TTF_STYLE_BOLD)
#define TTF_HANDLE_STYLE_ITALIC(font)        ((font)->style & TTF_STYLE_ITALIC)
#define TTF_HANDLE_STYLE_UNDERLINE(font)     ((font)->style & TTF_STYLE_UNDERLINE)
#define TTF_HANDLE_STYLE_STRIKETHROUGH(font) ((font)->style & TTF_STYLE_STRIKETHROUGH)

// Font styles that does not impact glyph drawing
#define TTF_STYLE_NO_GLYPH_CHANGE   (TTF_STYLE_UNDERLINE | TTF_STYLE_STRIKETHROUGH)

// The FreeType font engine/library
static struct
{
    SDL_InitState init;
    SDL_AtomicInt refcount;
    SDL_Mutex *lock;
    FT_Library library;
} TTF_state;

#define TTF_CHECK_INITIALIZED(errval)                   \
    if (SDL_ShouldInit(&TTF_state.init)) {              \
        SDL_SetInitialized(&TTF_state.init, false);     \
        SDL_SetError("Library not initialized");        \
        return errval;                                  \
    }

#define TTF_CHECK_POINTER(name, p, errval)              \
    if (!(p)) {                                         \
        SDL_InvalidParamError(name);                    \
        return errval;                                  \
    }

#define TTF_CHECK_FONT(font, errval)                    \
    TTF_CHECK_POINTER("font", font, errval)

typedef enum {
    RENDER_SOLID = 0,
    RENDER_SHADED,
    RENDER_BLENDED,
    RENDER_LCD
} render_mode_t;

#define NO_MEASUREMENT  \
        false, 0, NULL, NULL


static bool Find_GlyphByIndex(TTF_Font *font, FT_UInt idx, int want_bitmap, int want_pixmap, int want_color, int want_lcd, int want_subpixel, int translation, c_glyph **out_glyph, TTF_Image **out_image);

#if defined(USE_DUFFS_LOOP)

// 4-times unrolled loop
#define DUFFS_LOOP4(pixel_copy_increment, width)                        \
{ int n = (width+3)/4;                                                  \
    switch (width & 3) {                                                \
    case 0: do {    pixel_copy_increment;   /* fallthrough */           \
    case 3:     pixel_copy_increment;       /* fallthrough */           \
    case 2:     pixel_copy_increment;       /* fallthrough */           \
    case 1:     pixel_copy_increment;       /* fallthrough */           \
        } while (--n > 0);                                              \
    }                                                                   \
}

#else

// Don't use Duff's device to unroll loops
#define DUFFS_LOOP(pixel_copy_increment, width)                         \
{ int n;                                                                \
    for ( n=width; n > 0; --n ) {                                       \
        pixel_copy_increment;                                           \
    }                                                                   \
}

#define DUFFS_LOOP4(pixel_copy_increment, width)                        \
    DUFFS_LOOP(pixel_copy_increment, width)

#endif


// Blend colored glyphs
static void BG_Blended_Color(const TTF_Image *image, Uint32 *destination, Sint32 srcskip, Uint32 dstskip, Uint8 fg_alpha)
{
    const Uint32 *src   = (Uint32 *)image->buffer;
    Uint32      *dst    = destination;
    Uint32       width  = image->width;
    Uint32       height = image->rows;

    if (fg_alpha == SDL_ALPHA_OPAQUE) {
        while (height--) {
            /* *INDENT-OFF* */
            DUFFS_LOOP4(
                *dst++ = *src++;
            , width);
            /* *INDENT-ON* */
            src = (const Uint32 *)((const Uint8 *)src + srcskip);
            dst = (Uint32 *)((Uint8 *)dst + dstskip);
        }
    } else {
        Uint32 alpha;
        Uint32 tmp;

        while (height--) {
            /* *INDENT-OFF* */
            DUFFS_LOOP4(
                    /* prevent misaligned load: tmp = *src++; */
                    // eventually, we can expect the compiler to replace the memcpy call with something optimized
                    SDL_memcpy(&tmp, src++, sizeof(tmp));
                    alpha = tmp >> 24;
                    tmp &= ~0xFF000000;
                    alpha = fg_alpha * alpha;
                    alpha =  DIVIDE_BY_255(alpha) << 24;
                    *dst++ = tmp | alpha
                    , width);
            /* *INDENT-ON* */
            src = (const Uint32 *)((const Uint8 *)src + srcskip);
            dst = (Uint32 *)((Uint8 *)dst + dstskip);
        }
    }
}

// Blend with LCD rendering
static void BG_Blended_LCD(const TTF_Image *image, Uint32 *destination, Sint32 srcskip, Uint32 dstskip, SDL_Color *fg)
{
    const Uint32 *src   = (Uint32 *)image->buffer;
    Uint32      *dst    = destination;
    Uint32       width  = image->width;
    Uint32       height = image->rows;

    Uint32 tmp, bg;
    Uint32 r, g, b;
    Uint8 fg_r, fg_g, fg_b;
    Uint8 bg_r, bg_g, bg_b;
    Uint32 bg_a;

    fg_r = fg->r;
    fg_g = fg->g;
    fg_b = fg->b;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
                /* prevent misaligned load: tmp = *src++; */
                SDL_memcpy(&tmp, src++, sizeof(tmp));

                if (tmp) {
                    bg = *dst;

                    bg_a = bg & 0xff000000;
                    bg_r = (bg >> 16) & 0xff;
                    bg_g = (bg >> 8) & 0xff;
                    bg_b = (bg >> 0) & 0xff;

                    r = (tmp >> 16) & 0xff;
                    g = (tmp >> 8) & 0xff;
                    b = (tmp >> 0) & 0xff;

                    r = fg_r * r + bg_r * (255 - r) + 127;
                    r = DIVIDE_BY_255(r);

                    g = fg_g * g + bg_g * (255 - g) + 127;
                    g = DIVIDE_BY_255(g);

                    b = fg_b * b + bg_b * (255 - b) + 127;
                    b = DIVIDE_BY_255(b);

                    r <<= 16;
                    g <<= 8;
                    b <<= 0;

                    *dst = r | g | b | bg_a;
                }
                dst++;

                , width);
        /* *INDENT-ON* */
        src = (const Uint32 *)((const Uint8 *)src + srcskip);
        dst = (Uint32 *)((Uint8 *)dst + dstskip);
    }

}

#if TTF_USE_SDF

// Blended Opaque SDF
static void BG_Blended_Opaque_SDF(const TTF_Image *image, Uint32 *destination, Sint32 srcskip, Uint32 dstskip)
{
    const Uint8 *src    = image->buffer;
    Uint32      *dst    = destination;
    Uint32       width  = image->width;
    Uint32       height = image->rows;

    Uint32 s;
    Uint32 d;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            d = *dst;
            s = ((Uint32)*src++) << 24;
            if (s > d) {
                *dst = s;
            }
            dst++;
        , width);
        /* *INDENT-ON* */
        src += srcskip;
        dst  = (Uint32 *)((Uint8 *)dst + dstskip);
    }
}

// Blended non-opaque SDF
static void BG_Blended_SDF(const TTF_Image *image, Uint32 *destination, Sint32 srcskip, Uint32 dstskip, Uint8 fg_alpha)
{
    const Uint8 *src    = image->buffer;
    Uint32      *dst    = destination;
    Uint32       width  = image->width;
    Uint32       height = image->rows;

    Uint32 s;
    Uint32 d;

    Uint32 tmp;
    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            d = *dst;
            tmp = fg_alpha * (*src++);
            s = DIVIDE_BY_255(tmp) << 24;
            if (s > d) {
                *dst = s;
            }
            dst++;
        , width);
        /* *INDENT-ON* */
        src += srcskip;
        dst  = (Uint32 *)((Uint8 *)dst + dstskip);
    }
}

#endif // TTF_USE_SDF

// Blended Opaque
static void BG_Blended_Opaque(const TTF_Image *image, Uint32 *destination, Sint32 srcskip, Uint32 dstskip)
{
    const Uint8 *src    = image->buffer;
    Uint32      *dst    = destination;
    Uint32       width  = image->width;
    Uint32       height = image->rows;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            *dst++ |= ((Uint32)*src++) << 24;
        , width);
        /* *INDENT-ON* */
        src += srcskip;
        dst  = (Uint32 *)((Uint8 *)dst + dstskip);
    }
}

// Blended non-opaque
static void BG_Blended(const TTF_Image *image, Uint32 *destination, Sint32 srcskip, Uint32 dstskip, Uint8 fg_alpha)
{
    const Uint8 *src    = image->buffer;
    Uint32      *dst    = destination;
    Uint32       width  = image->width;
    Uint32       height = image->rows;

    Uint32 tmp;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            tmp     = fg_alpha * (*src++);
            *dst++ |= DIVIDE_BY_255(tmp) << 24;
        , width);
        /* *INDENT-ON* */
        src += srcskip;
        dst  = (Uint32 *)((Uint8 *)dst + dstskip);
    }
}

#if defined(HAVE_BLIT_GLYPH_32) || defined(HAVE_BLIT_GLYPH_64)
static void BG_Blended_Opaque_32(const TTF_Image *image, Uint32 *destination, Sint32 srcskip, Uint32 dstskip)
{
    const Uint8 *src    = image->buffer;
    Uint32      *dst    = destination;
    Uint32       width  = image->width / 4;
    Uint32       height = image->rows;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            *dst++ |= ((Uint32)*src++) << 24;
            *dst++ |= ((Uint32)*src++) << 24;
            *dst++ |= ((Uint32)*src++) << 24;
            *dst++ |= ((Uint32)*src++) << 24;
        , width);
        /* *INDENT-ON* */
        src += srcskip;
        dst  = (Uint32 *)((Uint8 *)dst + dstskip);
    }
}

static void BG_Blended_32(const TTF_Image *image, Uint32 *destination, Sint32 srcskip, Uint32 dstskip, Uint8 fg_alpha)
{
    const Uint8 *src    = image->buffer;
    Uint32      *dst    = destination;
    Uint32       width  = image->width / 4;
    Uint32       height = image->rows;

    Uint32 tmp0, tmp1, tmp2, tmp3;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            tmp0    = fg_alpha * (*src++);
            tmp1    = fg_alpha * (*src++);
            tmp2    = fg_alpha * (*src++);
            tmp3    = fg_alpha * (*src++);
            *dst++ |= DIVIDE_BY_255(tmp0) << 24;
            *dst++ |= DIVIDE_BY_255(tmp1) << 24;
            *dst++ |= DIVIDE_BY_255(tmp2) << 24;
            *dst++ |= DIVIDE_BY_255(tmp3) << 24;
        , width);
        /* *INDENT-ON* */
        src += srcskip;
        dst  = (Uint32 *)((Uint8 *)dst + dstskip);
    }
}
#endif

#if defined(HAVE_SSE2_INTRINSICS)
// Apply: alpha_table[i] = i << 24;
static void BG_Blended_Opaque_SSE(const TTF_Image *image, Uint32 *destination, Sint32 srcskip, Uint32 dstskip)
{
    const __m128i *src    = (__m128i *)image->buffer;
    __m128i       *dst    = (__m128i *)destination;
    Uint32         width  = image->width / 16;
    Uint32         height = image->rows;

    __m128i s, s0, s1, s2, s3, d0, d1, d2, d3, r0, r1, r2, r3, L, H;
    const __m128i zero  = _mm_setzero_si128();

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            // Read 16 Uint8 at once and put into 4 __m128i
            s  = _mm_loadu_si128(src);          // load unaligned
            d0 = _mm_load_si128(dst);           // load
            d1 = _mm_load_si128(dst + 1);       // load
            d2 = _mm_load_si128(dst + 2);       // load
            d3 = _mm_load_si128(dst + 3);       // load

            L  = _mm_unpacklo_epi8(zero, s);
            H  = _mm_unpackhi_epi8(zero, s);

            s0 = _mm_unpacklo_epi8(zero, L);
            s1 = _mm_unpackhi_epi8(zero, L);
            s2 = _mm_unpacklo_epi8(zero, H);
            s3 = _mm_unpackhi_epi8(zero, H);
                                                // already shifted by 24
            r0 = _mm_or_si128(d0, s0);          // or
            r1 = _mm_or_si128(d1, s1);          // or
            r2 = _mm_or_si128(d2, s2);          // or
            r3 = _mm_or_si128(d3, s3);          // or

            _mm_store_si128(dst, r0);           // store
            _mm_store_si128(dst + 1, r1);       // store
            _mm_store_si128(dst + 2, r2);       // store
            _mm_store_si128(dst + 3, r3);       // store

            dst += 4;
            src += 1;
        , width);
        /* *INDENT-ON* */
        src = (const __m128i *)((const Uint8 *)src + srcskip);
        dst = (__m128i *)((Uint8 *)dst + dstskip);
    }
}

static void BG_Blended_SSE(const TTF_Image *image, Uint32 *destination, Sint32 srcskip, Uint32 dstskip, Uint8 fg_alpha)
{
    const __m128i *src    = (__m128i *)image->buffer;
    __m128i       *dst    = (__m128i *)destination;
    Uint32         width  = image->width / 16;
    Uint32         height = image->rows;

    const __m128i alpha = _mm_set1_epi16(fg_alpha);
    const __m128i one   = _mm_set1_epi16(1);
    const __m128i zero  = _mm_setzero_si128();
    __m128i s, s0, s1, s2, s3, d0, d1, d2, d3, r0, r1, r2, r3, L, H, Ls8, Hs8;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            // Read 16 Uint8 at once and put into 4 __m128i
            s  = _mm_loadu_si128(src);          // load unaligned
            d0 = _mm_load_si128(dst);           // load
            d1 = _mm_load_si128(dst + 1);       // load
            d2 = _mm_load_si128(dst + 2);       // load
            d3 = _mm_load_si128(dst + 3);       // load

            L  = _mm_unpacklo_epi8(s, zero);    // interleave, no shifting
            H  = _mm_unpackhi_epi8(s, zero);    // enough room to multiply

            /* Apply: alpha_table[i] = ((i * fg.a / 255) << 24; */
            // Divide by 255 is done as:    (x + 1 + (x >> 8)) >> 8

            L  = _mm_mullo_epi16(L, alpha);     // x := i * fg.a
            H  = _mm_mullo_epi16(H, alpha);

            Ls8 = _mm_srli_epi16(L, 8);         // x >> 8
            Hs8 = _mm_srli_epi16(H, 8);
            L = _mm_add_epi16(L, one);          // x + 1
            H = _mm_add_epi16(H, one);
            L = _mm_add_epi16(L, Ls8);          // x + 1 + (x >> 8)
            H = _mm_add_epi16(H, Hs8);
            L = _mm_srli_epi16(L, 8);           // ((x + 1 + (x >> 8)) >> 8
            H = _mm_srli_epi16(H, 8);

            L = _mm_slli_epi16(L, 8);           // shift << 8, so we're prepared
            H = _mm_slli_epi16(H, 8);           // to have final format << 24

            s0 = _mm_unpacklo_epi8(zero, L);
            s1 = _mm_unpackhi_epi8(zero, L);
            s2 = _mm_unpacklo_epi8(zero, H);
            s3 = _mm_unpackhi_epi8(zero, H);
                                                // already shifted by 24

            r0 = _mm_or_si128(d0, s0);          // or
            r1 = _mm_or_si128(d1, s1);          // or
            r2 = _mm_or_si128(d2, s2);          // or
            r3 = _mm_or_si128(d3, s3);          // or

            _mm_store_si128(dst, r0);           // store
            _mm_store_si128(dst + 1, r1);       // store
            _mm_store_si128(dst + 2, r2);       // store
            _mm_store_si128(dst + 3, r3);       // store

            dst += 4;
            src += 1;
        , width);
        /* *INDENT-ON* */
        src = (const __m128i *)((const Uint8 *)src + srcskip);
        dst = (__m128i *)((Uint8 *)dst + dstskip);
    }
}
#endif

#if defined(HAVE_NEON_INTRINSICS)
// Apply: alpha_table[i] = i << 24;
static void BG_Blended_Opaque_NEON(const TTF_Image *image, Uint32 *destination, Sint32 srcskip, Uint32 dstskip)
{
    const Uint32 *src    = (Uint32 *)image->buffer;
    Uint32       *dst    = destination;
    Uint32        width  = image->width / 16;
    Uint32        height = image->rows;

    uint32x4_t s, d0, d1, d2, d3, r0, r1, r2, r3;
    uint8x16x2_t sx, sx01, sx23;
    const uint8x16_t zero = vdupq_n_u8(0);

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            /* Read 4 Uint32 and put 16 Uint8 into uint32x4x2_t (uint8x16x2_t)
             * takes advantage of vzipq_u8 which produces two lanes */

            s   = vld1q_u32(src);                           // load
            d0  = vld1q_u32(dst);                           // load
            d1  = vld1q_u32(dst + 4);                       // load
            d2  = vld1q_u32(dst + 8);                       // load
            d3  = vld1q_u32(dst + 12);                      // load

            sx   = vzipq_u8(zero, (uint8x16_t)s);           // interleave
            sx01 = vzipq_u8(zero, sx.val[0]);               // interleave
            sx23 = vzipq_u8(zero, sx.val[1]);               // interleave
                                                            // already shifted by 24
            r0  = vorrq_u32(d0, (uint32x4_t)sx01.val[0]);   // or
            r1  = vorrq_u32(d1, (uint32x4_t)sx01.val[1]);   // or
            r2  = vorrq_u32(d2, (uint32x4_t)sx23.val[0]);   // or
            r3  = vorrq_u32(d3, (uint32x4_t)sx23.val[1]);   // or

            vst1q_u32(dst, r0);                             // store
            vst1q_u32(dst + 4, r1);                         // store
            vst1q_u32(dst + 8, r2);                         // store
            vst1q_u32(dst + 12, r3);                        // store

            dst += 16;
            src += 4;
        , width);
        /* *INDENT-ON* */
        src = (const Uint32 *)((const Uint8 *)src + srcskip);
        dst = (Uint32 *)((Uint8 *)dst + dstskip);
    }
}

// Non-opaque, computes alpha blending on the fly
static void BG_Blended_NEON(const TTF_Image *image, Uint32 *destination, Sint32 srcskip, Uint32 dstskip, Uint8 fg_alpha)
{
    const Uint32 *src    = (Uint32 *)image->buffer;
    Uint32       *dst    = destination;
    Uint32        width  = image->width / 16;
    Uint32        height = image->rows;

    uint32x4_t s, d0, d1, d2, d3, r0, r1, r2, r3;
    uint16x8_t Ls8, Hs8;
    uint8x16x2_t sx, sx01, sx23;
    uint16x8x2_t sxm;

    const uint16x8_t alpha = vmovq_n_u16(fg_alpha);
    const uint16x8_t one   = vmovq_n_u16(1);
    const uint8x16_t zero  = vdupq_n_u8(0);

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            /* Read 4 Uint32 and put 16 Uint8 into uint32x4x2_t (uint8x16x2_t)
             * takes advantage of vzipq_u8 which produces two lanes */

            s  = vld1q_u32(src);                                    // load
            d0 = vld1q_u32(dst);                                    // load
            d1 = vld1q_u32(dst + 4);                                // load
            d2 = vld1q_u32(dst + 8);                                // load
            d3 = vld1q_u32(dst + 12);                               // load

            sx = vzipq_u8((uint8x16_t)s, zero);                     // interleave, no shifting
                                                                    // enough room to multiply

            /* Apply: alpha_table[i] = ((i * fg.a / 255) << 24; */
            // Divide by 255 is done as:    (x + 1 + (x >> 8)) >> 8

            sxm.val[0] = vmulq_u16((uint16x8_t)sx.val[0], alpha);   // x := i * fg.a
            sxm.val[1] = vmulq_u16((uint16x8_t)sx.val[1], alpha);

            Ls8 = vshrq_n_u16(sxm.val[0], 8);                       // x >> 8
            Hs8 = vshrq_n_u16(sxm.val[1], 8);

            sxm.val[0] = vaddq_u16(sxm.val[0], one);                // x + 1
            sxm.val[1] = vaddq_u16(sxm.val[1], one);

            sxm.val[0] = vaddq_u16(sxm.val[0], Ls8);                // x + 1 + (x >> 8)
            sxm.val[1] = vaddq_u16(sxm.val[1], Hs8);

            sxm.val[0] = vshrq_n_u16(sxm.val[0], 8);                // ((x + 1 + (x >> 8)) >> 8
            sxm.val[1] = vshrq_n_u16(sxm.val[1], 8);

            sxm.val[0] = vshlq_n_u16(sxm.val[0], 8);                // shift << 8, so we're prepared
            sxm.val[1] = vshlq_n_u16(sxm.val[1], 8);                // to have final format << 24

            sx01 = vzipq_u8(zero, (uint8x16_t)sxm.val[0]);          // interleave
            sx23 = vzipq_u8(zero, (uint8x16_t)sxm.val[1]);          // interleave
                                                                    // already shifted by 24

            r0  = vorrq_u32(d0, (uint32x4_t)sx01.val[0]);           // or
            r1  = vorrq_u32(d1, (uint32x4_t)sx01.val[1]);           // or
            r2  = vorrq_u32(d2, (uint32x4_t)sx23.val[0]);           // or
            r3  = vorrq_u32(d3, (uint32x4_t)sx23.val[1]);           // or

            vst1q_u32(dst, r0);                                     // store
            vst1q_u32(dst + 4, r1);                                 // store
            vst1q_u32(dst + 8, r2);                                 // store
            vst1q_u32(dst + 12, r3);                                // store

            dst += 16;
            src += 4;
        , width);
        /* *INDENT-ON* */
        src = (const Uint32 *)((const Uint8 *)src + srcskip);
        dst = (Uint32 *)((Uint8 *)dst + dstskip);
    }
}
#endif

static void BG(const TTF_Image *image, Uint8 *destination, Sint32 srcskip, Uint32 dstskip)
{
    const Uint8 *src    = image->buffer;
    Uint8       *dst    = destination;
    Uint32       width  = image->width;
    Uint32       height = image->rows;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            *dst++ |= *src++;
        , width);
        /* *INDENT-ON* */
        src += srcskip;
        dst += dstskip;
    }
}

#if defined(HAVE_BLIT_GLYPH_64)
static void BG_64(const TTF_Image *image, Uint8 *destination, Sint32 srcskip, Uint32 dstskip)
{
    const Uint64 *src    = (Uint64 *)image->buffer;
    Uint64       *dst    = (Uint64 *)destination;
    Uint32        width  = image->width / 8;
    Uint32        height = image->rows;
    Uint64        tmp;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
              /* prevent misaligned load: *dst++ |= *src++; */
              SDL_memcpy(&tmp, src++, sizeof(tmp));
              *dst++ |= tmp;
        , width);
        /* *INDENT-ON* */
        src = (const Uint64 *)((const Uint8 *)src + srcskip);
        dst = (Uint64 *)((Uint8 *)dst + dstskip);
    }
}
#elif defined(HAVE_BLIT_GLYPH_32)
static void BG_32(const TTF_Image *image, Uint8 *destination, Sint32 srcskip, Uint32 dstskip)
{
    const Uint32 *src    = (Uint32 *)image->buffer;
    Uint32       *dst    = (Uint32 *)destination;
    Uint32        width  = image->width / 4;
    Uint32        height = image->rows;
    Uint32        tmp;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            /* prevent misaligned load: *dst++ |= *src++; */
            SDL_memcpy(&tmp, src++, sizeof(tmp));
            *dst++ |= tmp;
        , width);
        /* *INDENT-ON* */
        src = (const Uint32 *)((const Uint8 *)src + srcskip);
        dst = (Uint32 *)((Uint8 *)dst + dstskip);
    }
}
#endif

#if defined(HAVE_SSE2_INTRINSICS)
static void BG_SSE(const TTF_Image *image, Uint8 *destination, Sint32 srcskip, Uint32 dstskip)
{
    const __m128i *src    = (__m128i *)image->buffer;
    __m128i       *dst    = (__m128i *)destination;
    Uint32         width  = image->width / 16;
    Uint32         height = image->rows;

    __m128i s, d, r;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            s = _mm_loadu_si128(src);   // load unaligned
            d = _mm_load_si128(dst);    // load
            r = _mm_or_si128(d, s);     // or
            _mm_store_si128(dst, r);    // store
            src += 1;
            dst += 1;
        , width);
        /* *INDENT-ON* */
        src = (const __m128i *)((const Uint8 *)src + srcskip);
        dst = (__m128i *)((Uint8 *)dst + dstskip);
    }
}
#endif

#if defined(HAVE_NEON_INTRINSICS)
static void BG_NEON(const TTF_Image *image, Uint8 *destination, Sint32 srcskip, Uint32 dstskip)
{
    const Uint8 *src    = image->buffer;
    Uint8       *dst    = destination;
    Uint32       width  = image->width / 16;
    Uint32       height = image->rows;

    uint8x16_t s, d, r;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            s = vld1q_u8(src);  // load
            d = vld1q_u8(dst);  // load
            r = vorrq_u8(d, s); // or
            vst1q_u8(dst, r);   // store
            src += 16;
            dst += 16;
        , width);
        /* *INDENT-ON* */
        src = (const Uint8 *)((const Uint8 *)src + srcskip);
        dst += dstskip;
    }
}
#endif

// Underline and Strikethrough style. Draw a line at the given row.
static void Draw_Line(TTF_Font *font, const SDL_Surface *textbuf, int column, int row, int line_width, int line_thickness, Uint32 color, const render_mode_t render_mode)
{
    int tmp    = row + line_thickness - textbuf->h;
    int x_offset = column * SDL_BYTESPERPIXEL(textbuf->format);
    Uint8 *dst = (Uint8 *)textbuf->pixels + row * textbuf->pitch + x_offset;
#if TTF_USE_HARFBUZZ
    hb_direction_t hb_direction = font->hb_direction;

    // No Underline/Strikethrough style if direction is vertical
    if (hb_direction == HB_DIRECTION_TTB || hb_direction == HB_DIRECTION_BTT) {
        return;
    }
#else
    (void) font;
#endif

    /* Not needed because of "font->height = SDL_max(font->height, bottom_row);".
     * But if you patch to render textshaping and break line in middle of a cluster,
     * (which is a bad usage and a corner case), you need this to prevent out of bounds.
     * You can get an "ystart" for the "whole line", which is different (and smaller)
     * than the ones of the "splitted lines". */
    if (tmp > 0) {
        line_thickness -= tmp;
    }
    /* Previous case also happens with SDF (render_sdf) , because 'spread' property
     * requires to increase 'ystart'
     * Check for valid value anyway.  */
    if (line_thickness <= 0) {
        return;
    }

    // Wrapped mode with an unbroken line: 'line_width' is greater that 'textbuf->w'
    line_width = SDL_min(line_width, textbuf->w);

    if (render_mode == RENDER_BLENDED || render_mode == RENDER_LCD) {
        while (line_thickness--) {
            SDL_memset4(dst, color, line_width);
            dst += textbuf->pitch;
        }
    } else {
        while (line_thickness--) {
            SDL_memset(dst, color, line_width);
            dst += textbuf->pitch;
        }
    }
}

static void Draw_Line_TextEngine(TTF_Font *font, int width, int height, int column, int row, int line_width, int line_thickness, TTF_DrawOperation *ops, int *current_op)
{
    int op_index = *current_op;
    TTF_DrawOperation *op = &ops[op_index];
    int tmp    = row + line_thickness - height;
#if TTF_USE_HARFBUZZ
    hb_direction_t hb_direction = font->hb_direction;

    // No Underline/Strikethrough style if direction is vertical
    if (hb_direction == HB_DIRECTION_TTB || hb_direction == HB_DIRECTION_BTT) {
        return;
    }
#else
    (void) font;
#endif

    /* Not needed because of "font->height = SDL_max(font->height, bottom_row);".
     * But if you patch to render textshaping and break line in middle of a cluster,
     * (which is a bad usage and a corner case), you need this to prevent out of bounds.
     * You can get an "ystart" for the "whole line", which is different (and smaller)
     * than the ones of the "splitted lines". */
    if (tmp > 0) {
        line_thickness -= tmp;
    }
    /* Previous case also happens with SDF (render_sdf) , because 'spread' property
     * requires to increase 'ystart'
     * Check for valid value anyway.  */
    if (line_thickness <= 0) {
        return;
    }

    // Wrapped mode with an unbroken line: 'line_width' is greater that 'width'
    line_width = SDL_min(line_width, width);

    op = &ops[op_index++];
    op->cmd = TTF_DRAW_COMMAND_FILL;
    op->fill.rect.x = column;
    op->fill.rect.y = row;
    op->fill.rect.w = line_width;
    op->fill.rect.h = line_thickness;
    *current_op = op_index;
}

static void clip_glyph(int *_x, int *_y, TTF_Image *image, const SDL_Surface *textbuf, int is_lcd)
{
    int above_w;
    int above_h;
    int x = *_x;
    int y = *_y;

    int srcbpp = 1;
    if (image->is_color || is_lcd) {
        srcbpp = 4;
    }

    // Don't go below x=0
    if (x < 0) {
        int tmp = -x;
        x = 0;
        image->width  -= tmp;
        image->buffer += srcbpp * tmp;
    }
    // Don't go above textbuf->w
    above_w = x + image->width - textbuf->w;
    if (above_w > 0) {
        image->width -= above_w;
    }
    // Don't go below y=0
    if (y < 0) {
        int tmp = -y;
        y = 0;
        image->rows   -= tmp;
        image->buffer += tmp * image->pitch;
    }
    // Don't go above textbuf->h
    above_h = y + image->rows - textbuf->h;
    if (above_h > 0) {
        image->rows -= above_h;
    }
    // Could be negative if (x > textbuf->w), or if (x + width < 0)
    image->width = SDL_max(0, image->width);
    image->rows  = SDL_max(0, image->rows);

    /* After 'image->width' clipping:
     * Make sure 'rows' is also 0, so it doesn't break USE_DUFFS_LOOP */
    if (image->width == 0) {
        image->rows = 0;
    }

    *_x = x;
    *_y = y;
}

// Glyph width is rounded, dst addresses are aligned, src addresses are not aligned
static int Get_Alignment(void)
{
#if defined(HAVE_NEON_INTRINSICS)
    if (hasNEON()) {
        return 16;
    }
#endif

#if defined(HAVE_SSE2_INTRINSICS)
    if (hasSSE2()) {
        return 16;
    }
#endif

#if defined(HAVE_BLIT_GLYPH_64)
    return 8;
#elif defined(HAVE_BLIT_GLYPH_32)
    return 4;
#else
    return 1;
#endif
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#endif
#define BUILD_RENDER_LINE(NAME, IS_BLENDED, IS_BLENDED_OPAQUE, IS_LCD, WB_WP_WC, WS, BLIT_GLYPH_BLENDED_OPAQUE_OPTIM, BLIT_GLYPH_BLENDED_OPTIM, BLIT_GLYPH_OPTIM) \
                                                                                                                        \
static bool Render_Line_##NAME(TTF_Font *font, SDL_Surface *textbuf, int xstart, int ystart, SDL_Color *fg)             \
{                                                                                                                       \
    const int alignment = Get_Alignment() - 1;                                                                          \
    const int bpp = ((IS_BLENDED || IS_LCD) ? 4 : 1);                                                                   \
    int i;                                                                                                              \
    Uint8 fg_alpha = (fg ? fg->a : 0);                                                                                  \
    for (i = 0; i < font->pos_len; i++) {                                                                               \
        FT_UInt idx = font->pos_buf[i].index;                                                                           \
        int x       = font->pos_buf[i].x;                                                                               \
        int y       = font->pos_buf[i].y;                                                                               \
        TTF_Image *image;                                                                                               \
                                                                                                                        \
        if (Find_GlyphByIndex(font, idx, WB_WP_WC, WS, x & 63, NULL, &image)) {                                         \
            int above_w, above_h;                                                                                       \
            Uint32 dstskip;                                                                                             \
            Sint32 srcskip; /* Can be negative */                                                                       \
            Uint8 *dst;                                                                                                 \
            int remainder;                                                                                              \
            Uint8 *saved_buffer = image->buffer;                                                                        \
            int saved_width = image->width;                                                                             \
            image->buffer += alignment;                                                                                 \
            /* Position updated after glyph rendering */                                                                \
            x = xstart + FT_FLOOR(x) + image->left;                                                                     \
            y = ystart + FT_FLOOR(y) - image->top;                                                                      \
                                                                                                                        \
            /* Make sure glyph is inside textbuf */                                                                     \
            above_w = x + image->width - textbuf->w;                                                                    \
            above_h = y + image->rows  - textbuf->h;                                                                    \
                                                                                                                        \
            if (x >= 0 && y >= 0 && above_w <= 0 && above_h <= 0) {                                                     \
                /* Most often, glyph is inside textbuf */                                                               \
                /* Compute dst */                                                                                       \
                dst  = (Uint8 *)textbuf->pixels + y * textbuf->pitch + x * bpp;                                         \
                /* Align dst, get remainder, shift & align glyph width */                                               \
                remainder = ((uintptr_t)dst & alignment) / bpp;                                                         \
                dst  = (Uint8 *)((uintptr_t)dst & ~alignment);                                                          \
                image->buffer -= remainder;                                                                             \
                image->width   = (image->width + remainder + alignment) & ~alignment;                                   \
                /* Compute srcskip, dstskip */                                                                          \
                srcskip = image->pitch - image->width;                                                                  \
                dstskip = textbuf->pitch - image->width * bpp;                                                          \
                /* Render glyph at (x, y) with optimized copy functions */                                              \
                if (IS_LCD) {                                                                                           \
                    image->buffer = saved_buffer;                                                                       \
                    image->buffer += alignment;                                                                         \
                    image->width = saved_width;                                                                         \
                    dst = (Uint8 *)textbuf->pixels + y * textbuf->pitch + x * bpp;                                      \
                    /* Compute srcskip, dstskip */                                                                      \
                    srcskip = image->pitch - 4 * image->width;                                                          \
                    dstskip = textbuf->pitch - image->width * bpp;                                                      \
                    BG_Blended_LCD(image, (Uint32 *)dst, srcskip, dstskip, fg);                                         \
                } else if (!IS_BLENDED || image->is_color == 0) {                                                       \
                    if (IS_BLENDED_OPAQUE) {                                                                            \
                        BLIT_GLYPH_BLENDED_OPAQUE_OPTIM(image, (Uint32 *)dst, srcskip, dstskip);                        \
                    } else if (IS_BLENDED) {                                                                            \
                        BLIT_GLYPH_BLENDED_OPTIM(image, (Uint32 *)dst, srcskip, dstskip, fg_alpha);                     \
                    } else {                                                                                            \
                        BLIT_GLYPH_OPTIM(image, dst, srcskip, dstskip);                                                 \
                    }                                                                                                   \
                } else if (IS_BLENDED && image->is_color) {                                                             \
                    image->buffer = saved_buffer;                                                                       \
                    image->buffer += alignment;                                                                         \
                    image->width = saved_width;                                                                         \
                    dst = (Uint8 *)textbuf->pixels + y * textbuf->pitch + x * bpp;                                      \
                    /* Compute srcskip, dstskip */                                                                      \
                    srcskip = image->pitch - 4 * image->width;                                                          \
                    dstskip = textbuf->pitch - image->width * bpp;                                                      \
                    BG_Blended_Color(image, (Uint32 *)dst, srcskip, dstskip, fg_alpha);                                 \
                }                                                                                                       \
                /* restore modification */                                                                              \
                image->width = saved_width;                                                                             \
            } else {                                                                                                    \
                /* Modify a copy, and clip it */                                                                        \
                TTF_Image image_clipped = *image;                                                                       \
                /* Intersect image glyph at (x,y) with textbuf */                                                       \
                clip_glyph(&x, &y, &image_clipped, textbuf, IS_LCD);                                                    \
                /* Compute dst */                                                                                       \
                dst = (Uint8 *)textbuf->pixels + y * textbuf->pitch + x * bpp;                                          \
                /* Compute srcskip, dstskip */                                                                          \
                srcskip = image_clipped.pitch - image_clipped.width;                                                    \
                dstskip = textbuf->pitch - image_clipped.width * bpp;                                                   \
                /* Render glyph at (x, y) */                                                                            \
                if (IS_LCD) {                                                                                           \
                    srcskip -= 3 * image_clipped.width;                                                                 \
                    BG_Blended_LCD(&image_clipped, (Uint32 *)dst, srcskip, dstskip, fg);                                \
                } else if (!IS_BLENDED || image->is_color == 0) {                                                       \
                    if (IS_BLENDED_OPAQUE) {                                                                            \
                        BG_Blended_Opaque(&image_clipped, (Uint32 *)dst, srcskip, dstskip);                             \
                    } else if (IS_BLENDED) {                                                                            \
                        BG_Blended(&image_clipped, (Uint32 *)dst, srcskip, dstskip, fg_alpha);                          \
                    } else {                                                                                            \
                        BG(&image_clipped, dst, srcskip, dstskip);                                                      \
                    }                                                                                                   \
                } else if (IS_BLENDED && image->is_color) {                                                             \
                    srcskip -= 3 * image_clipped.width;                                                                 \
                    BG_Blended_Color(&image_clipped, (Uint32 *)dst, srcskip, dstskip, fg_alpha);                        \
                }                                                                                                       \
            }                                                                                                           \
            image->buffer = saved_buffer;                                                                               \
        } else {                                                                                                        \
            return false;                                                                                               \
        }                                                                                                               \
    }                                                                                                                   \
                                                                                                                        \
    return true;                                                                                                        \
}                                                                                                                       \
                                                                                                                        \

#define BITMAP  CACHED_BITMAP, 0, 0, 0
#define PIXMAP  0, CACHED_PIXMAP, 0, 0
#define COLOR   0, 0, CACHED_COLOR, 0
#define LCD     0, 0, 0, CACHED_LCD

#define SUBPIX  CACHED_SUBPIX

// BUILD_RENDER_LINE(NAME, IS_BLENDED, IS_BLENDED_OPAQUE, WANT_BITMAP_PIXMAP_COLOR_LCD, WANT_SUBPIXEL, BLIT_GLYPH_BLENDED_OPAQUE_OPTIM, BLIT_GLYPH_BLENDED_OPTIM, BLIT_GLYPH_OPTIM)

#if defined(HAVE_SSE2_INTRINSICS)
BUILD_RENDER_LINE(SSE_Shaded            , 0, 0, 0, PIXMAP, 0     ,                       ,                , BG_SSE     )
BUILD_RENDER_LINE(SSE_Blended           , 1, 0, 0,  COLOR, 0     ,                       , BG_Blended_SSE ,            )
BUILD_RENDER_LINE(SSE_Blended_Opaque    , 1, 1, 0,  COLOR, 0     , BG_Blended_Opaque_SSE ,                ,            )
BUILD_RENDER_LINE(SSE_Solid             , 0, 0, 0, BITMAP, 0     ,                       ,                , BG_SSE     )
BUILD_RENDER_LINE(SSE_Shaded_SP         , 0, 0, 0, PIXMAP, SUBPIX,                       ,                , BG_SSE     )
BUILD_RENDER_LINE(SSE_Blended_SP        , 1, 0, 0,  COLOR, SUBPIX,                       , BG_Blended_SSE ,            )
BUILD_RENDER_LINE(SSE_Blended_Opaque_SP , 1, 1, 0,  COLOR, SUBPIX, BG_Blended_Opaque_SSE ,                ,            )
BUILD_RENDER_LINE(SSE_LCD               , 0, 0, 1,    LCD, 0,                            ,                ,            )
BUILD_RENDER_LINE(SSE_LCD_SP            , 0, 0, 1,    LCD, SUBPIX,                       ,                ,            )
#endif

#if defined(HAVE_NEON_INTRINSICS)
BUILD_RENDER_LINE(NEON_Shaded           , 0, 0, 0, PIXMAP, 0     ,                       ,                , BG_NEON    )
BUILD_RENDER_LINE(NEON_Blended          , 1, 0, 0,  COLOR, 0     ,                       , BG_Blended_NEON,            )
BUILD_RENDER_LINE(NEON_Blended_Opaque   , 1, 1, 0,  COLOR, 0     , BG_Blended_Opaque_NEON,                ,            )
BUILD_RENDER_LINE(NEON_Solid            , 0, 0, 0, BITMAP, 0     ,                       ,                , BG_NEON    )
BUILD_RENDER_LINE(NEON_Shaded_SP        , 0, 0, 0, PIXMAP, SUBPIX,                       ,                , BG_NEON    )
BUILD_RENDER_LINE(NEON_Blended_SP       , 1, 0, 0,  COLOR, SUBPIX,                       , BG_Blended_NEON,            )
BUILD_RENDER_LINE(NEON_Blended_Opaque_SP, 1, 1, 0,  COLOR, SUBPIX, BG_Blended_Opaque_NEON,                ,            )
BUILD_RENDER_LINE(NEON_LCD              , 0, 0, 1,    LCD, 0     ,                       ,                ,            )
BUILD_RENDER_LINE(NEON_LCD_SP           , 0, 0, 1,    LCD, SUBPIX,                       ,                ,            )
#endif

#if defined(HAVE_BLIT_GLYPH_64)
BUILD_RENDER_LINE(64_Shaded             , 0, 0, 0, PIXMAP, 0     ,                       ,                , BG_64      )
BUILD_RENDER_LINE(64_Blended            , 1, 0, 0,  COLOR, 0     ,                       , BG_Blended_32  ,            )
BUILD_RENDER_LINE(64_Blended_Opaque     , 1, 1, 0,  COLOR, 0     , BG_Blended_Opaque_32  ,                ,            )
BUILD_RENDER_LINE(64_Solid              , 0, 0, 0, BITMAP, 0     ,                       ,                , BG_64      )
BUILD_RENDER_LINE(64_Shaded_SP          , 0, 0, 0, PIXMAP, SUBPIX,                       ,                , BG_64      )
BUILD_RENDER_LINE(64_Blended_SP         , 1, 0, 0,  COLOR, SUBPIX,                       , BG_Blended_32  ,            )
BUILD_RENDER_LINE(64_Blended_Opaque_SP  , 1, 1, 0,  COLOR, SUBPIX, BG_Blended_Opaque_32  ,                ,            )
BUILD_RENDER_LINE(64_LCD                , 0, 0, 1,    LCD, 0     ,                       ,                ,            )
BUILD_RENDER_LINE(64_LCD_SP             , 0, 0, 1,    LCD, SUBPIX,                       ,                ,            )
#elif defined(HAVE_BLIT_GLYPH_32)
BUILD_RENDER_LINE(32_Shaded             , 0, 0, 0, PIXMAP, 0     ,                       ,                , BG_32      )
BUILD_RENDER_LINE(32_Blended            , 1, 0, 0,  COLOR, 0     ,                       , BG_Blended_32  ,            )
BUILD_RENDER_LINE(32_Blended_Opaque     , 1, 1, 0,  COLOR, 0     , BG_Blended_Opaque_32  ,                ,            )
BUILD_RENDER_LINE(32_Solid              , 0, 0, 0, BITMAP, 0     ,                       ,                , BG_32      )
BUILD_RENDER_LINE(32_Shaded_SP          , 0, 0, 0, PIXMAP, SUBPIX,                       ,                , BG_32      )
BUILD_RENDER_LINE(32_Blended_SP         , 1, 0, 0,  COLOR, SUBPIX,                       , BG_Blended_32  ,            )
BUILD_RENDER_LINE(32_Blended_Opaque_SP  , 1, 1, 0,  COLOR, SUBPIX, BG_Blended_Opaque_32  ,                ,            )
BUILD_RENDER_LINE(32_LCD                , 0, 0, 1,    LCD, 0     ,                       ,                ,            )
BUILD_RENDER_LINE(32_LCD_SP             , 0, 0, 1,    LCD, SUBPIX,                       ,                ,            )
#else
BUILD_RENDER_LINE(8_Shaded              , 0, 0, 0, PIXMAP, 0     ,                       ,                , BG         )
BUILD_RENDER_LINE(8_Blended             , 1, 0, 0,  COLOR, 0     ,                       , BG_Blended     ,            )
BUILD_RENDER_LINE(8_Blended_Opaque      , 1, 1, 0,  COLOR, 0     , BG_Blended_Opaque     ,                ,            )
BUILD_RENDER_LINE(8_Solid               , 0, 0, 0, BITMAP, 0     ,                       ,                , BG         )
BUILD_RENDER_LINE(8_Shaded_SP           , 0, 0, 0, PIXMAP, SUBPIX,                       ,                , BG         )
BUILD_RENDER_LINE(8_Blended_SP          , 1, 0, 0,  COLOR, SUBPIX,                       , BG_Blended     ,            )
BUILD_RENDER_LINE(8_Blended_Opaque_SP   , 1, 1, 0,  COLOR, SUBPIX, BG_Blended_Opaque     ,                ,            )
BUILD_RENDER_LINE(8_LCD                 , 0, 0, 1,    LCD, 0     ,                       ,                ,            )
BUILD_RENDER_LINE(8_LCD_SP              , 0, 0, 1,    LCD, SUBPIX,                       ,                ,            )
#endif


#if TTF_USE_SDF
static int (*Render_Line_SDF_Shaded)(TTF_Font *font, SDL_Surface *textbuf, int xstart, int ystart, SDL_Color *fg) = NULL;
BUILD_RENDER_LINE(SDF_Blended           , 1, 0, 0,  COLOR, 0     ,                       , BG_Blended_SDF ,            )
BUILD_RENDER_LINE(SDF_Blended_Opaque    , 1, 1, 0,  COLOR, 0     , BG_Blended_Opaque_SDF ,                ,            )
static int (*Render_Line_SDF_Solid)(TTF_Font *font, SDL_Surface *textbuf, int xstart, int ystart, SDL_Color *fg) = NULL;
static int (*Render_Line_SDF_Shaded_SP)(TTF_Font *font, SDL_Surface *textbuf, int xstart, int ystart, SDL_Color *fg) = NULL;
BUILD_RENDER_LINE(SDF_Blended_SP        , 1, 0, 0,  COLOR, SUBPIX,                       , BG_Blended_SDF ,            )
BUILD_RENDER_LINE(SDF_Blended_Opaque_SP , 1, 1, 0,  COLOR, SUBPIX, BG_Blended_Opaque_SDF ,                ,            )
static int (*Render_Line_SDF_LCD)(TTF_Font *font, SDL_Surface *textbuf, int xstart, int ystart, SDL_Color *fg) = NULL;
static int (*Render_Line_SDF_LCD_SP)(TTF_Font *font, SDL_Surface *textbuf, int xstart, int ystart, SDL_Color *fg) = NULL;
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static bool Render_Line(const render_mode_t render_mode, int subpixel, TTF_Font *font, SDL_Surface *textbuf, int xstart, int ystart, SDL_Color fg)
{
    // Render line (pos_buf) to textbuf at (xstart, ystart)

    // Subpixel with RENDER_SOLID doesn't make sense.
    // (and 'cached->subpixel.translation' would need to distinguish bitmap/pixmap).
    int is_opaque = (fg.a == SDL_ALPHA_OPAQUE);

#define Call_Specific_Render_Line(NAME)                                                                     \
        if (render_mode == RENDER_SHADED) {                                                                 \
            if (subpixel == 0) {                                                                            \
                return Render_Line_##NAME##_Shaded(font, textbuf, xstart, ystart, NULL);                    \
            } else {                                                                                        \
                return Render_Line_##NAME##_Shaded_SP(font, textbuf, xstart, ystart, NULL);                 \
            }                                                                                               \
        } else if (render_mode == RENDER_BLENDED) {                                                         \
            if (is_opaque) {                                                                                \
                if (subpixel == 0) {                                                                        \
                    return Render_Line_##NAME##_Blended_Opaque(font, textbuf, xstart, ystart, NULL);        \
                } else {                                                                                    \
                    return Render_Line_##NAME##_Blended_Opaque_SP(font, textbuf, xstart, ystart, NULL);     \
                }                                                                                           \
            } else {                                                                                        \
                if (subpixel == 0) {                                                                        \
                    return Render_Line_##NAME##_Blended(font, textbuf, xstart, ystart, &fg);                \
                } else {                                                                                    \
                    return Render_Line_##NAME##_Blended_SP(font, textbuf, xstart, ystart, &fg);             \
                }                                                                                           \
            }                                                                                               \
        } else if (render_mode == RENDER_LCD) {                                                             \
            if (subpixel == 0) {                                                                            \
                return Render_Line_##NAME##_LCD(font, textbuf, xstart, ystart, &fg);                        \
            } else {                                                                                        \
                return Render_Line_##NAME##_LCD_SP(font, textbuf, xstart, ystart, &fg);                     \
            }                                                                                               \
        } else {                                                                                            \
            return Render_Line_##NAME##_Solid(font, textbuf, xstart, ystart, NULL);                         \
        }

#if TTF_USE_SDF
    if (font->render_sdf && render_mode == RENDER_BLENDED) {
        Call_Specific_Render_Line(SDF)
    }
#endif

#if defined(HAVE_NEON_INTRINSICS)
    if (hasNEON()) {
        Call_Specific_Render_Line(NEON)
    }
#endif
#if defined(HAVE_SSE2_INTRINSICS)
    if (hasSSE2()) {
        Call_Specific_Render_Line(SSE)
    }
#endif
#if defined(HAVE_BLIT_GLYPH_64)
    Call_Specific_Render_Line(64)
#elif defined(HAVE_BLIT_GLYPH_32)
    Call_Specific_Render_Line(32)
#else
    Call_Specific_Render_Line(8)
#endif
}

static bool Render_Line_TextEngine(TTF_Font *font, int xstart, int ystart, int width, int height, TTF_DrawOperation *ops, int *current_op, TTF_SubString *clusters, int *current_cluster, int cluster_offset, int line_index)
{
    int i;
    int op_index = *current_op;
    int cluster_index = *current_cluster;
    int last_offset = -1;
    TTF_SubString *cluster = NULL;

    SDL_Rect bounds;
    bounds.x = xstart;
    bounds.y = ystart;
    bounds.w = 0;
    bounds.h = font->height;

    for (i = 0; i < font->pos_len; i++) {
        FT_UInt idx = font->pos_buf[i].index;
        int x       = font->pos_buf[i].x;
        int y       = font->pos_buf[i].y;
        int offset  = font->pos_buf[i].offset;
        c_glyph *glyph;

        if (Find_GlyphByIndex(font, idx, 0, 0, 0, 0, 0, 0, &glyph, NULL)) {
            int above_w, above_h;
            int glyph_x = 0;
            int glyph_y = 0;
            int glyph_width = glyph->sz_width;
            int glyph_rows = glyph->sz_rows;
            TTF_DrawOperation *op;

            // Position updated after glyph rendering
            x = xstart + FT_FLOOR(x) + glyph->sz_left;
            y = ystart + FT_FLOOR(y) - glyph->sz_top;

            // Make sure glyph is inside text area
            above_w = x + glyph_width - width;
            above_h = y + glyph_rows  - height;

            if (x < 0) {
                int tmp = -x;
                x = 0;
                glyph_x += tmp;
                glyph_width -= tmp;
            }
            if (above_w > 0) {
                glyph_width -= above_w;
            }
            if (y < 0) {
                int tmp = -y;
                y = 0;
                glyph_y += tmp;
                glyph_rows -= tmp;
            }
            if (above_h > 0) {
                glyph_rows -= above_h;
            }

            if (glyph_width > 0 && glyph_rows > 0) {
                op = &ops[op_index++];
                op->cmd = TTF_DRAW_COMMAND_COPY;
                op->copy.text_offset = offset;
                op->copy.glyph_index = idx;
                op->copy.src.x = glyph_x;
                op->copy.src.y = glyph_y;
                op->copy.src.w = glyph_width;
                op->copy.src.h = glyph_rows;
                op->copy.dst.x = x;
                op->copy.dst.y = y;
                op->copy.dst.w = op->copy.src.w;
                op->copy.dst.h = op->copy.src.h;
            } else {
                // Use the distance to the next glyph as our bounds width
                glyph_width = FT_FLOOR(glyph->advance);
            }

            bounds.x = x;
            bounds.w = glyph_width;
            if (offset != last_offset) {
                cluster = &clusters[cluster_index++];
                cluster->offset = cluster_offset + offset;
                cluster->line_index = line_index;
                SDL_copyp(&cluster->rect, &bounds);

                last_offset = offset;
            } else if (cluster) {
                SDL_GetRectUnion(&cluster->rect, &bounds, &cluster->rect);
            }
        } else {
            return false;
        }
    }

    *current_op = op_index;
    *current_cluster = cluster_index;
    return true;
}

/* Create a surface with memory:
 * - pitch is rounded to alignment
 * - address is aligned
 *
 * If format is 4 bytes per pixel, bgcolor is used to initialize each
 * 4-byte word in the image data.
 *
 * Otherwise, the low byte of format is used to initialize each byte
 * in the image data.
 */
static SDL_Surface *AllocateAlignedPixels(size_t width, size_t height, SDL_PixelFormat format, Uint32 bgcolor)
{
    const size_t alignment = Get_Alignment() - 1;
    const size_t bytes_per_pixel = SDL_BYTESPERPIXEL(format);
    SDL_Surface *textbuf = NULL;
    size_t size;
    void *pixels;
    size_t pitch;

    /*
     * 1/ Line size is "width * bytes_per_pixel"
     *
     * 2/ We add a right padding, because we process glyph from source to destination by
     * blocks of 'alignment + 1' bytes.  (Using SSE 128 instruction for instance when renderering,
     * but this isn't always the case for all modes).
     *
     * We need to make sure the last transfer doesn't go too much outside!
     *
     * Considerer also for instance, that when we read 1 block of 16 bytes from source, for the blended
     * format (bbp == 4), it writes(and reads) 4 blocks of 16 in the dest, like BG_Blended_SSE()).
     *
     * Remark: for Solid/Shaded, block ratio read/write is 1:1.
     * For Color / LCD / SDF, it is byte vs byte or int. They are also fallback for
     * Solid/Shaded/Blend, when it isn't contained in textbuf, see clip_glyph()
     *
     * So the pitch must contain "width * bytes_per_pixel", plus in the
     * worst case, writing at last pixel (1 * bytes_per_pixel), an extra "alignment * bytes_per_pixel".
     * (Using the destination bytes_per_pixel is a safe upper bound for the ratio).
     *
     * Also, we always write at a block-aligned adresses.
     * - address is aligned 'SDL_aligned_alloc((alignment + 1), size)'
     * - the pitch is aligned 'pitch &= ~alignment'
     * So that each line is aligned.
     *
     * Remark: we can safely align the pitch (pitch &= ~alignment), without adding more to pitch,
     * becaJuse we know we always write blocks at block-aligned addresses.
     * So pitch won't need to be more than a multiple of block size.
     *
     * So pitch is:
     *  ((width * bytes_per_pixel) + (alignment * bytes_per_pixel) ) & ~alignment
     *  ==
     *  ((width + alignment) * bytes_per_pixel ) & ~alignment
     *
     * which is different from:
     *  ((width + alignment) & ~alignment)) * bytes_per_pixel  (which fails.)
     *
     * Remark: to test memory issues, it is useful to patch SDL to use real memalign/free
     * so that valgrind check more precisely out of bounds.
     */
    if (width > SDL_MAX_SINT32 ||
        height > SDL_MAX_SINT32 ||
        !SDL_size_add_check_overflow(width, alignment, &pitch) ||
        !SDL_size_mul_check_overflow(pitch, bytes_per_pixel, &pitch) ||
        pitch > SDL_MAX_SINT32) {
        return NULL;
    }
    pitch &= ~alignment;

    if (!SDL_size_mul_check_overflow(height, pitch, &size)) {
        // Overflow...
        return NULL;
    }

    pixels = SDL_aligned_alloc(alignment + 1, size);
    if (pixels == NULL) {
        return NULL;
    }

    textbuf = SDL_CreateSurfaceFrom((int)width, (int)height, format, pixels, (int)pitch);
    if (textbuf == NULL) {
        SDL_aligned_free(pixels);
        return NULL;
    }

    // Let SDL handle the memory allocation
    textbuf->flags &= ~SDL_SURFACE_PREALLOCATED;
    textbuf->flags |= SDL_SURFACE_SIMD_ALIGNED;

    // Allocate a palette if needed
    if (SDL_ISPIXELFORMAT_INDEXED(format)) {
        if (!SDL_CreateSurfacePalette(textbuf)) {
            SDL_DestroySurface(textbuf);
            return NULL;
        }
    }

    if (bytes_per_pixel == 4) {
        SDL_memset4(pixels, bgcolor, size / 4);
    }
    else {
        SDL_memset(pixels, (bgcolor & 0xff), size);
    }

    return textbuf;
}

static SDL_Surface* Create_Surface_Solid(int width, int height, SDL_Color fg, Uint32 *color)
{
    SDL_Surface *textbuf = AllocateAlignedPixels(width, height, SDL_PIXELFORMAT_INDEX8, 0);
    if (textbuf == NULL) {
        return NULL;
    }

    // Underline/Strikethrough color style
    *color = 1;

    // Fill the palette: 1 is foreground
    {
        SDL_Palette *palette = SDL_GetSurfacePalette(textbuf);
        palette->colors[0].r = 255 - fg.r;
        palette->colors[0].g = 255 - fg.g;
        palette->colors[0].b = 255 - fg.b;
        palette->colors[1].r = fg.r;
        palette->colors[1].g = fg.g;
        palette->colors[1].b = fg.b;
        palette->colors[1].a = fg.a;
    }

    SDL_SetSurfaceColorKey(textbuf, true, 0);

    return textbuf;
}

static SDL_Surface* Create_Surface_Shaded(int width, int height, SDL_Color fg, SDL_Color bg, Uint32 *color)
{
    SDL_Surface *textbuf = AllocateAlignedPixels(width, height, SDL_PIXELFORMAT_INDEX8, 0);
    Uint8 bg_alpha = bg.a;
    if (textbuf == NULL) {
        return NULL;
    }

    // Underline/Strikethrough color style
    *color = NUM_GRAYS - 1;

    // Support alpha blending
    if (fg.a != SDL_ALPHA_OPAQUE || bg.a != SDL_ALPHA_OPAQUE) {
        SDL_SetSurfaceBlendMode(textbuf, SDL_BLENDMODE_BLEND);

        // Would disturb alpha palette
        if (bg.a == SDL_ALPHA_OPAQUE) {
            bg.a = 0;
        }
    }

    // Fill the palette with NUM_GRAYS levels of shading from bg to fg
    {
        SDL_Palette *palette = SDL_GetSurfacePalette(textbuf);
        int rdiff  = fg.r - bg.r;
        int gdiff  = fg.g - bg.g;
        int bdiff  = fg.b - bg.b;
        int adiff  = fg.a - bg.a;
        int sign_r = (rdiff >= 0) ? 1 : 255;
        int sign_g = (gdiff >= 0) ? 1 : 255;
        int sign_b = (bdiff >= 0) ? 1 : 255;
        int sign_a = (adiff >= 0) ? 1 : 255;
        int i;

        for (i = 0; i < NUM_GRAYS; ++i) {
            /* Compute color[i] = (i * color_diff / 255) */
            int tmp_r = i * rdiff;
            int tmp_g = i * gdiff;
            int tmp_b = i * bdiff;
            int tmp_a = i * adiff;
            palette->colors[i].r = (Uint8)(bg.r + DIVIDE_BY_255_SIGNED(tmp_r, sign_r));
            palette->colors[i].g = (Uint8)(bg.g + DIVIDE_BY_255_SIGNED(tmp_g, sign_g));
            palette->colors[i].b = (Uint8)(bg.b + DIVIDE_BY_255_SIGNED(tmp_b, sign_b));
            palette->colors[i].a = (Uint8)(bg.a + DIVIDE_BY_255_SIGNED(tmp_a, sign_a));
        }

        // Make sure background has the correct alpha value
        palette->colors[0].a = bg_alpha;
    }

    return textbuf;
}

static SDL_Surface *Create_Surface_Blended(int width, int height, SDL_Color fg, Uint32 *color)
{
    SDL_Surface *textbuf = NULL;
    Uint32 bgcolor;

    // Background color: initialize with fg and 0 alpha
    bgcolor = (fg.r << 16) | (fg.g << 8) | fg.b;

    // Underline/Strikethrough color style
    *color = bgcolor | ((Uint32)fg.a << 24);

    // Create the target surface if required
    if (width != 0) {
        textbuf = AllocateAlignedPixels(width, height, SDL_PIXELFORMAT_ARGB8888, bgcolor);
        if (textbuf == NULL) {
            return NULL;
        }
    }

    return textbuf;
}

static SDL_Surface* Create_Surface_LCD(int width, int height, SDL_Color fg, SDL_Color bg, Uint32 *color)
{
    SDL_Surface *textbuf = NULL;
    Uint32 bgcolor;

    // Background color
    bgcolor = (((Uint32)bg.a) << 24) | (bg.r << 16) | (bg.g << 8) | bg.b;

    // Underline/Strikethrough color style
    *color = (((Uint32)bg.a) << 24) | (fg.r << 16) | (fg.g << 8) | fg.b;

    // Create the target surface if required
    if (width != 0) {
        textbuf = AllocateAlignedPixels(width, height, SDL_PIXELFORMAT_ARGB8888, bgcolor);
        if (textbuf == NULL) {
            return NULL;
        }
    }

    return textbuf;
}


// rcg06192001 get linked library's version.
int TTF_Version(void)
{
    return SDL_TTF_VERSION;
}

#if defined(USE_FREETYPE_ERRORS)
static bool TTF_SetFTError(const char *msg, FT_Error error)
{
#undef FTERRORS_H_
#define FT_ERRORDEF(e, v, s)    { e, s },
#define FT_ERROR_START_LIST     {
#define FT_ERROR_END_LIST       { 0, NULL } };
    const struct
    {
      int          err_code;
      const char  *err_msg;
    } ft_errors[] =
#include FT_ERRORS_H

    unsigned int i;
    const char *err_msg = NULL;

    for (i = 0; i < sizeof (ft_errors) / sizeof (ft_errors[0]); ++i) {
        if (error == ft_errors[i].err_code) {
            err_msg = ft_errors[i].err_msg;
            break;
        }
    }
    if (!err_msg) {
        err_msg = "unknown FreeType error";
    }
    return SDL_SetError("%s: %s", msg, err_msg);
}
#else
#define TTF_SetFTError(msg, error)    SDL_SetError(msg)
#endif // USE_FREETYPE_ERRORS

bool TTF_Init(void)
{
    bool result = true;

// Some debug to know how it gets compiled
#if 0
    int duffs = 0, sse2 = 0, neon = 0, compil_sse2 = 0, compil_neon = 0;
#  if defined(USE_DUFFS_LOOP)
    duffs = 1;
#  endif
#  if defined(HAVE_SSE2_INTRINSICS)
    sse2 = hasSSE2();
    compil_sse2 = 1;
#  endif
#  if defined(HAVE_NEON_INTRINSICS)
    neon = hasNEON();
    compil_neon = 1;
#  endif
    SDL_Log("SDL_ttf: hasSSE2=%d hasNEON=%d alignment=%d duffs_loop=%d compil_sse2=%d compil_neon=%d",
            sse2, neon, Get_Alignment(), duffs, compil_sse2, compil_neon);

    SDL_Log("Sizeof TTF_Image: %d c_glyph: %d TTF_Font: %d", sizeof (TTF_Image), sizeof (c_glyph), sizeof (TTF_Font));
#endif

    SDL_AtomicIncRef(&TTF_state.refcount);

    if (!SDL_ShouldInit(&TTF_state.init)) {
        return true;
    }

    FT_Error error = FT_Init_FreeType(&TTF_state.library);
    if (error) {
        TTF_SetFTError("Couldn't init FreeType engine", error);
        result = false;
    }

    if (result) {
#if TTF_USE_SDF
#if 0
        // Set various properties of the renderers.
        int spread = 4;
        int overlaps = 0;
        FT_Property_Set(TTF_state.library, "bsdf", "spread", &spread);
        FT_Property_Set(TTF_state.library, "sdf", "spread", &spread);
        FT_Property_Set(TTF_state.library, "sdf", "overlaps", &overlaps);
#endif
#endif
        TTF_state.lock = SDL_CreateMutex();
    } else {
        (void)SDL_AtomicDecRef(&TTF_state.refcount);
    }
    SDL_SetInitialized(&TTF_state.init, result);

    return result;
}

void TTF_GetFreeTypeVersion(int *major, int *minor, int *patch)
{
    FT_Int ft_major = 0;
    FT_Int ft_minor = 0;
    FT_Int ft_patch = 0;

    if (SDL_ShouldInit(&TTF_state.init)) {
        SDL_SetInitialized(&TTF_state.init, false);
    } else {
        SDL_LockMutex(TTF_state.lock);
        FT_Library_Version(TTF_state.library, &ft_major, &ft_minor, &ft_patch);
        SDL_UnlockMutex(TTF_state.lock);
    }

    if (major) {
        *major = (int)ft_major;
    }
    if (minor) {
        *minor = (int)ft_minor;
    }
    if (patch) {
        *patch = (int)ft_patch;
    }
}

void TTF_GetHarfBuzzVersion(int *major, int *minor, int *patch)
{
    unsigned int hb_major = 0;
    unsigned int hb_minor = 0;
    unsigned int hb_micro = 0;

#if TTF_USE_HARFBUZZ
    hb_version(&hb_major, &hb_minor, &hb_micro);
#endif
    if (major) {
        *major = (int)hb_major;
    }
    if (minor) {
        *minor = (int)hb_minor;
    }
    if (patch) {
        *patch = (int)hb_micro;
    }
}

static unsigned long IOread(
    FT_Stream stream,
    unsigned long offset,
    unsigned char *buffer,
    unsigned long count
)
{
    TTF_Font *font = (TTF_Font *)stream->descriptor.pointer;
    SDL_SeekIO(font->src, font->src_offset + offset, SDL_IO_SEEK_SET);
    return (unsigned long)SDL_ReadIO(font->src, buffer, count);
}

TTF_Font *TTF_OpenFontWithProperties(SDL_PropertiesID props)
{
    const char *file = SDL_GetStringProperty(props, TTF_PROP_FONT_CREATE_FILENAME_STRING, NULL);
    SDL_IOStream *src = SDL_GetPointerProperty(props, TTF_PROP_FONT_CREATE_IOSTREAM_POINTER, NULL);
    Sint64 src_offset = SDL_GetNumberProperty(props, TTF_PROP_FONT_CREATE_IOSTREAM_OFFSET_NUMBER, 0);
    bool closeio = SDL_GetBooleanProperty(props, TTF_PROP_FONT_CREATE_IOSTREAM_AUTOCLOSE_BOOLEAN, false);
    float ptsize = SDL_GetFloatProperty(props, TTF_PROP_FONT_CREATE_SIZE_FLOAT, 0);
    long index = (long)SDL_GetNumberProperty(props, TTF_PROP_FONT_CREATE_FACE_NUMBER, 0);
    unsigned int hdpi = (unsigned int)SDL_GetNumberProperty(props, TTF_PROP_FONT_CREATE_HORIZONTAL_DPI_NUMBER, 0);
    unsigned int vdpi = (unsigned int)SDL_GetNumberProperty(props, TTF_PROP_FONT_CREATE_VERTICAL_DPI_NUMBER, 0);
    TTF_Font *font;
    FT_Error error;
    FT_Face face;
    FT_Stream stream;
    FT_CharMap found;
    Sint64 position;
    int i;

    if (SDL_ShouldInit(&TTF_state.init)) {
        SDL_SetInitialized(&TTF_state.init, false);
        SDL_SetError("Library not initialized");
        if (src && closeio) {
            SDL_CloseIO(src);
        }
        return NULL;
    }

    if (!src) {
        if (!file) {
            SDL_SetError("You must set either TTF_PROP_FONT_CREATE_FILENAME_STRING or TTF_PROP_FONT_CREATE_IOSTREAM_POINTER");
            return NULL;
        }

        src = SDL_IOFromFile(file, "rb");
        if (!src) {
            return NULL;
        }
        closeio = true;
    }

    // Check to make sure we can seek in this stream
    position = SDL_TellIO(src);
    if (position < 0) {
        SDL_SetError("Can't seek in stream");
        if (closeio) {
            SDL_CloseIO(src);
        }
        return NULL;
    }

    font = (TTF_Font *)SDL_calloc(1, sizeof (*font));
    if (font == NULL) {
        SDL_SetError("Out of memory");
        if (closeio) {
            SDL_CloseIO(src);
        }
        return NULL;
    }

    font->src = src;
    font->src_offset = src_offset;
    font->closeio = closeio;
    font->generation = 1;
    font->hdpi = TTF_DEFAULT_DPI;
    font->vdpi = TTF_DEFAULT_DPI;

    font->text = SDL_CreateHashTable(NULL, 16, SDL_HashPointer, SDL_KeyMatchPointer, NULL, false);
    if (!font->text) {
        TTF_CloseFont(font);
        return NULL;
    }

    stream = (FT_Stream)SDL_malloc(sizeof (*stream));
    if (stream == NULL) {
        SDL_SetError("Out of memory");
        TTF_CloseFont(font);
        return NULL;
    }
    SDL_memset(stream, 0, sizeof (*stream));

    stream->read = IOread;
    stream->descriptor.pointer = font;
    stream->pos = 0;
    stream->size = (unsigned long)(SDL_GetIOSize(src) - src_offset);

    font->args.flags = FT_OPEN_STREAM;
    font->args.stream = stream;

    SDL_LockMutex(TTF_state.lock);
    error = FT_Open_Face(TTF_state.library, &font->args, index, &font->face);
    SDL_UnlockMutex(TTF_state.lock);
    if (error || font->face == NULL) {
        TTF_SetFTError("Couldn't load font file", error);
        TTF_CloseFont(font);
        return NULL;
    }
    face = font->face;

    // Set charmap for loaded font
    found = 0;
#if 0 // Font debug code
    for (i = 0; i < face->num_charmaps; i++) {
        FT_CharMap charmap = face->charmaps[i];
        SDL_Log("Found charmap: platform id %d, encoding id %d", charmap->platform_id, charmap->encoding_id);
    }
#endif
    if (!found) {
        for (i = 0; i < face->num_charmaps; i++) {
            FT_CharMap charmap = face->charmaps[i];
            if (charmap->platform_id == 3 && charmap->encoding_id == 10) { // UCS-4 Unicode
                found = charmap;
                break;
            }
        }
    }
    if (!found) {
        for (i = 0; i < face->num_charmaps; i++) {
            FT_CharMap charmap = face->charmaps[i];
            if ((charmap->platform_id == 3 && charmap->encoding_id == 1) // Windows Unicode
             || (charmap->platform_id == 3 && charmap->encoding_id == 0) // Windows Symbol
             || (charmap->platform_id == 2 && charmap->encoding_id == 1) // ISO Unicode
             || (charmap->platform_id == 0)) { // Apple Unicode
                found = charmap;
                break;
            }
        }
    }
    if (found) {
        // If this fails, continue using the default charmap
        FT_Set_Charmap(face, found);
    }

    // Set the default font style
    font->style = TTF_STYLE_NORMAL;
    font->outline = 0;
    font->ft_load_target = FT_LOAD_TARGET_NORMAL;
    TTF_SetFontKerning(font, true);

    font->pos_len = 0;
    font->pos_max = 16;
    font->pos_buf = (PosBuf_t *)SDL_malloc(font->pos_max * sizeof (font->pos_buf[0]));
    if (!font->pos_buf) {
        SDL_SetError("Out of memory");
        TTF_CloseFont(font);
        return NULL;
    }

#if TTF_USE_HARFBUZZ
    font->hb_font = hb_ft_font_create(face, NULL);
    if (font->hb_font == NULL) {
        SDL_SetError("Cannot create harfbuzz font");
        TTF_CloseFont(font);
        return NULL;
    }

    /* Default load-flags of hb_ft_font_create is no-hinting.
     * So unless you call hb_ft_font_set_load_flags to match what flags you use for rendering,
     * you will get mismatching advances and raster. */
    hb_ft_font_set_load_flags(font->hb_font, FT_LOAD_DEFAULT | font->ft_load_target);

    font->hb_direction = HB_DIRECTION_LTR;
    font->hb_script = HB_SCRIPT_UNKNOWN;
    font->hb_language = hb_language_from_string("", -1);
#endif

    if (!TTF_SetFontSizeDPI(font, ptsize, hdpi, vdpi)) {
        TTF_SetFTError("Couldn't set font size", error);
        TTF_CloseFont(font);
        return NULL;
    }

    return font;
}

TTF_Font *TTF_OpenFont(const char *file, float ptsize)
{
    TTF_Font *font = NULL;
    SDL_PropertiesID props = SDL_CreateProperties();
    if (props) {
        SDL_SetStringProperty(props, TTF_PROP_FONT_CREATE_FILENAME_STRING, file);
        SDL_SetFloatProperty(props, TTF_PROP_FONT_CREATE_SIZE_FLOAT, ptsize);
        font = TTF_OpenFontWithProperties(props);
        SDL_DestroyProperties(props);
    }
    return font;
}

TTF_Font *TTF_OpenFontIO(SDL_IOStream *src, bool closeio, float ptsize)
{
    TTF_Font *font = NULL;
    SDL_PropertiesID props = SDL_CreateProperties();
    if (props) {
        SDL_SetPointerProperty(props, TTF_PROP_FONT_CREATE_IOSTREAM_POINTER, src);
        SDL_SetBooleanProperty(props, TTF_PROP_FONT_CREATE_IOSTREAM_AUTOCLOSE_BOOLEAN, closeio);
        SDL_SetFloatProperty(props, TTF_PROP_FONT_CREATE_SIZE_FLOAT, ptsize);
        font = TTF_OpenFontWithProperties(props);
        SDL_DestroyProperties(props);
    }
    return font;
}

static bool AddFontTextReference(TTF_Font *font, TTF_Text *text)
{
    return SDL_InsertIntoHashTable(font->text, text, NULL);
}

static bool RemoveFontTextReference(TTF_Font *font, TTF_Text *text)
{
    return SDL_RemoveFromHashTable(font->text, text);
}

static void UpdateFontText(TTF_Font *font)
{
    if (!font->text) {
        return;
    }

    TTF_Text *text = NULL;
    void *iter = NULL;
    while (SDL_IterateHashTable(font->text, (const void **)&text, NULL, &iter)) {
        text->internal->needs_layout_update = true;
    }
}

SDL_PropertiesID TTF_GetFontProperties(TTF_Font *font)
{
    TTF_CHECK_FONT(font, 0);

    if (!font->props) {
        font->props = SDL_CreateProperties();
    }
    return font->props;
}

Uint32 TTF_GetFontGeneration(TTF_Font *font)
{
    TTF_CHECK_FONT(font, 0);

    return font->generation;
}

// Update font parameter depending on a style change
static void TTF_InitFontMetrics(TTF_Font *font)
{
    FT_Face face = font->face;
    int underline_offset;

    // Make sure that our font face is scalable (global metrics)
    if (FT_IS_SCALABLE(face)) {
        // Get the scalable font metrics for this font
        FT_Fixed scale       = face->size->metrics.y_scale;
        font->ascent         = FT_CEIL(FT_MulFix(face->ascender, scale));
        font->descent        = FT_CEIL(FT_MulFix(face->descender, scale));
        font->height         = FT_CEIL(FT_MulFix(face->ascender - face->descender, scale));
        font->lineskip       = FT_CEIL(FT_MulFix(face->height, scale));
        underline_offset     = FT_FLOOR(FT_MulFix(face->underline_position, scale));
        font->line_thickness = FT_FLOOR(FT_MulFix(face->underline_thickness, scale));
    } else {
        // Get the font metrics for this font, for the selected size
        font->ascent         = FT_CEIL(face->size->metrics.ascender);
        font->descent        = FT_CEIL(face->size->metrics.descender);
        font->height         = FT_CEIL(face->size->metrics.height);
        font->lineskip       = FT_CEIL(face->size->metrics.height);
        /* face->underline_position and face->underline_height are only
         * relevant for scalable formats (see freetype.h FT_FaceRec) */
        underline_offset     = font->descent / 2;
        font->line_thickness = 1;
    }

    if (font->line_thickness < 1) {
        font->line_thickness = 1;
    }

    font->underline_top_row     = font->ascent - underline_offset - 1;
    font->strikethrough_top_row = font->height / 2;

    // Adjust OutlineStyle, only for scalable fonts
    /* TTF_Size(): increase w and h by 2 * outline, translate positionning by 1 * outline */
    if (font->outline > 0) {
        int fo = font->outline;
        font->line_thickness        += 2 * fo;
        font->underline_top_row     -= fo;
        font->strikethrough_top_row -= fo;
    }

    // Robustness: no negative values allowed
    font->underline_top_row     = SDL_max(0, font->underline_top_row);
    font->strikethrough_top_row = SDL_max(0, font->strikethrough_top_row);

    // Update height according to the needs of the underline style
    if (TTF_HANDLE_STYLE_UNDERLINE(font)) {
        int bottom_row = font->underline_top_row + font->line_thickness;
        font->height = SDL_max(font->height, bottom_row);
    }
    // Update height according to the needs of the strikethrough style
    if (TTF_HANDLE_STYLE_STRIKETHROUGH(font)) {
        int bottom_row = font->strikethrough_top_row + font->line_thickness;
        font->height = SDL_max(font->height, bottom_row);
    }

#if defined(DEBUG_FONTS)
    SDL_Log("Font metrics:");
    SDL_Log("ascent = %d, descent = %d", font->ascent, font->descent);
    SDL_Log("height = %d, lineskip = %d", font->height, font->lineskip);
    SDL_Log("underline_offset = %d, line_thickness = %d", underline_offset, font->line_thickness);
    SDL_Log("underline_top_row = %d, strikethrough_top_row = %d", font->underline_top_row, font->strikethrough_top_row);
    SDL_Log("scalable=%d fixed_sizes=%d", FT_IS_SCALABLE(face), FT_HAS_FIXED_SIZES(face));
#endif

    font->glyph_overhang = face->size->metrics.y_ppem / 10;
}

static void Flush_Glyph_Image(TTF_Image *image) {
    if (image->buffer) {
        SDL_free(image->buffer);
        image->buffer = NULL;
    }
}

static void Flush_Glyph(c_glyph *glyph)
{
    glyph->stored = 0;
    glyph->index = 0;
    Flush_Glyph_Image(&glyph->pixmap);
    Flush_Glyph_Image(&glyph->bitmap);
}

static void Flush_Cache(TTF_Font *font)
{
    int i;
    int size = sizeof (font->cache) / sizeof (font->cache[0]);

    for (i = 0; i < size; ++i) {
        if (font->cache[i].stored) {
            Flush_Glyph(&font->cache[i]);
        }
    }

    ++font->generation;
    if (font->generation == 0) {
        ++font->generation;
    }
}

static bool Load_Glyph(TTF_Font *font, c_glyph *cached, int want, int translation)
{
    const int alignment = Get_Alignment() - 1;
    FT_GlyphSlot slot;
    FT_Error error;

    int ft_load = FT_LOAD_DEFAULT | font->ft_load_target;

#if TTF_USE_COLOR
    if (want & CACHED_COLOR) {
        ft_load |= FT_LOAD_COLOR;
    }
#endif

    error = FT_Load_Glyph(font->face, cached->index, ft_load);
    if (error) {
        return TTF_SetFTError("FT_Load_Glyph() failed", error);
    }

    // Get our glyph shortcut
    slot = font->face->glyph;

    if (want & CACHED_LCD) {
        if (slot->format == FT_GLYPH_FORMAT_BITMAP) {
            return SDL_SetError("LCD mode not possible with bitmap font");
        }
    }

    // Get the glyph metrics, always needed
    if (cached->stored == 0) {
        cached->sz_left  = slot->bitmap_left;
        cached->sz_top   = slot->bitmap_top;
        cached->sz_rows  = slot->bitmap.rows;
        cached->sz_width = slot->bitmap.width;

        /* Current version of freetype is 2.9.1, but on older freetype (2.8.1) this can be 0.
         * Try to get them from 'FT_Glyph_Metrics' */
        if (cached->sz_left == 0 && cached->sz_top == 0 && cached->sz_rows == 0 && cached->sz_width == 0) {
            FT_Glyph_Metrics *metrics = &slot->metrics;
            if (metrics) {
                int minx = FT_FLOOR(metrics->horiBearingX);
                int maxx = FT_CEIL(metrics->horiBearingX + metrics->width);
                int maxy = FT_FLOOR(metrics->horiBearingY);
                int miny = maxy - FT_CEIL(metrics->height);

                cached->sz_left  = minx;
                cached->sz_top   = maxy;
                cached->sz_rows  = maxy - miny;
                cached->sz_width = maxx - minx;
            }
        }

        // All FP 26.6 are 'long' but 'int' should be engouh
        cached->advance  = (int)slot->metrics.horiAdvance; // FP 26.6

        if (font->render_subpixel == 0) {
            // FT KERNING_MODE_SMART
            cached->kerning_smart.rsb_delta = (int)slot->rsb_delta; // FP 26.6
            cached->kerning_smart.lsb_delta = (int)slot->lsb_delta; // FP 26.6
        } else {
            // FT LCD_MODE_LIGHT_SUBPIXEL
            cached->subpixel.lsb_minus_rsb  = (int)(slot->lsb_delta - slot->rsb_delta); // FP 26.6
            cached->subpixel.translation    = 0; // FP 26.6
        }

#if defined(DEBUG_FONTS)
        SDL_Log("Index=%d sz_left=%d sz_top=%d sz_width=%d sz_rows=%d advance=%d is_outline=%d is_bitmap=%d",
                cached->index, cached->sz_left, cached->sz_top, cached->sz_width, cached->sz_rows, cached->advance,
                slot->format == FT_GLYPH_FORMAT_OUTLINE, slot->format == FT_GLYPH_FORMAT_BITMAP);
#endif

        // Adjust for bold text
        if (TTF_HANDLE_STYLE_BOLD(font)) {
            cached->sz_width += font->glyph_overhang;
            cached->advance  += F26Dot6(font->glyph_overhang);
        }

        // Adjust for italic text
        if (TTF_HANDLE_STYLE_ITALIC(font) && slot->format == FT_GLYPH_FORMAT_OUTLINE) {
            cached->sz_width += (GLYPH_ITALICS * font->height) >> 16;
        }

        // Adjust for subpixel
        if (font->render_subpixel) {
            cached->sz_width += 1;
        }

        // Adjust for SDF
        if (font->render_sdf) {
            // Default 'spread' property
            cached->sz_width += 2 * 8;
            cached->sz_rows  += 2 * 8;
        }

        cached->stored |= CACHED_METRICS;
    }

    if (((want & CACHED_BITMAP) && !(cached->stored & CACHED_BITMAP)) ||
        ((want & CACHED_PIXMAP) && !(cached->stored & CACHED_PIXMAP)) ||
        ((want & CACHED_COLOR) && !(cached->stored & CACHED_COLOR)) ||
        ((want & CACHED_LCD) && !(cached->stored & CACHED_LCD)) ||
         (want & CACHED_SUBPIX)
       ) {
        const int  mono  = (want & CACHED_BITMAP);
        TTF_Image *dst   = (mono ? &cached->bitmap : &cached->pixmap);
        FT_Glyph   glyph = NULL;
        FT_Bitmap *src;
        FT_Render_Mode ft_render_mode;

        if (mono) {
            ft_render_mode = FT_RENDER_MODE_MONO;
        } else {
            ft_render_mode = FT_RENDER_MODE_NORMAL;
#if TTF_USE_SDF
            if ((want & CACHED_COLOR) && font->render_sdf) {
                ft_render_mode = FT_RENDER_MODE_SDF;
            }
#endif
            if ((want & CACHED_LCD)) {
                ft_render_mode = FT_RENDER_MODE_LCD;
            }
        }

        // Subpixel translation, flush previous datas
        if (want & CACHED_SUBPIX) {
            Flush_Glyph_Image(&cached->pixmap);
            FT_Outline_Translate(&slot->outline, translation, 0 );
            cached->subpixel.translation = translation;
        }

        // Handle the italic style, only for scalable fonts
        if (TTF_HANDLE_STYLE_ITALIC(font) && slot->format == FT_GLYPH_FORMAT_OUTLINE) {
            FT_Matrix shear;
            shear.xx = 1 << 16;
            shear.xy = GLYPH_ITALICS;
            shear.yx = 0;
            shear.yy = 1 << 16;
            FT_Outline_Transform(&slot->outline, &shear);
        }

        // Render as outline
        if ((font->outline > 0 && slot->format == FT_GLYPH_FORMAT_OUTLINE) ||
            slot->format == FT_GLYPH_FORMAT_BITMAP) {

            FT_BitmapGlyph bitmap_glyph;

            error = FT_Get_Glyph(slot, &glyph);
            if (error) {
                return TTF_SetFTError("FT_Get_Glyph() failed", error);
            }

            if (font->outline > 0) {
                FT_Glyph_Stroke(&glyph, font->stroker, 1 /* delete the original glyph */);
            }

            // Render the glyph
            error = FT_Glyph_To_Bitmap(&glyph, ft_render_mode, 0, 1);
            if (error) {
                FT_Done_Glyph(glyph);
                return TTF_SetFTError("FT_Glyph_To_Bitmap() failed", error);
            }

            // Access bitmap content by typecasting
            bitmap_glyph = (FT_BitmapGlyph) glyph;
            src          = &bitmap_glyph->bitmap;

            // Get new metrics, from bitmap
            dst->left   = bitmap_glyph->left;
            dst->top    = bitmap_glyph->top;
        } else {
            // Render the glyph
            error = FT_Render_Glyph(slot, ft_render_mode);
            if (error) {
                return TTF_SetFTError("FT_Render_Glyph() failed", error);
            }

            // Access bitmap from slot
            src         = &slot->bitmap;

            // Get new metrics, from slot
            dst->left   = slot->bitmap_left;
            dst->top    = slot->bitmap_top;
        }

        // Common metrics
        dst->width  = src->width;
        dst->rows   = src->rows;
        dst->buffer = NULL;

        /* FT can make small size glyph of 'width == 0', and 'rows != 0'.
         * Make sure 'rows' is also 0, so it doesn't break USE_DUFFS_LOOP */
        if (dst->width == 0) {
            dst->rows = 0;
        }

        // Adjust for bold text
        if (TTF_HANDLE_STYLE_BOLD(font)) {
            dst->width += font->glyph_overhang;
        }

        // Compute pitch: glyph is padded right to be able to read an 'aligned' size expanding on the right
        dst->pitch = dst->width + alignment;
#if TTF_USE_COLOR
        if (src->pixel_mode == FT_PIXEL_MODE_BGRA) {
            dst->pitch += 3 * dst->width;
        }
#endif
        if (src->pixel_mode == FT_PIXEL_MODE_LCD) {
            dst->pitch += 3 * dst->width;
        }

        if (dst->rows != 0) {
            unsigned int i;

            /* Glyph buffer is NOT aligned,
             * Extra width so it can read an 'aligned' size expanding on the left */
            dst->buffer = (unsigned char *)SDL_malloc(alignment + dst->pitch * dst->rows);
            if (!dst->buffer) {
                return false;
            }

            // Memset
            SDL_memset(dst->buffer, 0, alignment + dst->pitch * dst->rows);

            // Shift, so that the glyph is decoded centered
            dst->buffer += alignment;

            /* FT_Render_Glyph() and .fon fonts always generate a two-color (black and white)
             * glyphslot surface, even when rendered in FT_RENDER_MODE_NORMAL. */
            /* FT_IS_SCALABLE() means that the face contains outline glyphs, but does not imply
             * that outline is rendered as 8-bit grayscale, because embedded bitmap/graymap is
             * preferred (see FT_LOAD_DEFAULT section of FreeType2 API Reference).
             * FT_Render_Glyph() canreturn two-color bitmap or 4/16/256 color graymap
             * according to the format of embedded bitmap/graymap. */
            for (i = 0; i < (unsigned int)src->rows; i++) {
                unsigned char *srcp = src->buffer + i * src->pitch;
                unsigned char *dstp = dst->buffer + i * dst->pitch;
                unsigned int k, quotient, remainder;

                // Decode exactly the needed size from src->width
                if (src->pixel_mode == FT_PIXEL_MODE_MONO) {
                    quotient  = src->width / 8;
                    remainder = src->width & 0x7;
                } else if (src->pixel_mode == FT_PIXEL_MODE_GRAY2) {
                    quotient  = src->width / 4;
                    remainder = src->width & 0x3;
                } else if (src->pixel_mode == FT_PIXEL_MODE_GRAY4) {
                    quotient  = src->width / 2;
                    remainder = src->width & 0x1;
#if TTF_USE_COLOR
                } else if (src->pixel_mode == FT_PIXEL_MODE_BGRA) {
                    quotient  = src->width;
                    remainder = 0;
#endif
                } else if (src->pixel_mode == FT_PIXEL_MODE_LCD) {
                    quotient  = src->width / 3;
                    remainder = 0;
                } else {
                    quotient  = src->width;
                    remainder = 0;
                }

// FT_RENDER_MODE_MONO and src->pixel_mode MONO
#ifdef _MSC_VER
#pragma warning(push, 1)
#pragma warning(disable:4127)
#endif
#define MONO_MONO(K_MAX)                                                    \
                if ((K_MAX)) {                                              \
                    unsigned char c = *srcp++;                              \
                    for (k = 0; k < (K_MAX); ++k) {                         \
                        *dstp++ = (c & 0x80) >> 7;                          \
                        c <<= 1;                                            \
                    }                                                       \
                }

// FT_RENDER_MODE_MONO and src->pixel_mode GRAY2
#define MONO_GRAY2(K_MAX)                                                   \
                if ((K_MAX)) {                                              \
                    unsigned char c = *srcp++;                              \
                    for (k = 0; k < (K_MAX); ++k) {                         \
                        *dstp++ = (((c&0xA0) >> 6) >= 0x2) ? 1 : 0;         \
                        c <<= 2;                                            \
                    }                                                       \
                }

// FT_RENDER_MODE_MONO and src->pixel_mode GRAY4
#define MONO_GRAY4(K_MAX)                                                   \
                if ((K_MAX)) {                                              \
                    unsigned char c = *srcp++;                              \
                    for (k = 0; k < (K_MAX); ++k) {                         \
                        *dstp++ = (((c&0xF0) >> 4) >= 0x8) ? 1 : 0;         \
                        c <<= 4;                                            \
                    }                                                       \
                }

// FT_RENDER_MODE_NORMAL and src->pixel_mode MONO
#define NORMAL_MONO(K_MAX)                                                  \
                if ((K_MAX)) {                                              \
                    unsigned char c = *srcp++;                              \
                    for (k = 0; k < (K_MAX); ++k) {                         \
                        if ((c&0x80) >> 7) {                                \
                            *dstp++ = NUM_GRAYS - 1;                        \
                        } else {                                            \
                            *dstp++ = 0x00;                                 \
                        }                                                   \
                        c <<= 1;                                            \
                    }                                                       \
                }

// FT_RENDER_MODE_NORMAL and src->pixel_mode GRAY2
#define NORMAL_GRAY2(K_MAX)                                                 \
                if ((K_MAX)) {                                              \
                    unsigned char c = *srcp++;                              \
                    for (k = 0; k < (K_MAX); ++k) {                         \
                        if ((c&0xA0) >> 6) {                                \
                            *dstp++ = NUM_GRAYS * ((c&0xA0) >> 6) / 3 - 1;  \
                        } else {                                            \
                            *dstp++ = 0x00;                                 \
                        }                                                   \
                        c <<= 2;                                            \
                    }                                                       \
                }

// FT_RENDER_MODE_NORMAL and src->pixel_mode GRAY4
#define NORMAL_GRAY4(K_MAX)                                                 \
                if ((K_MAX)) {                                              \
                    unsigned char c = *srcp++;                              \
                    for (k = 0; k < (K_MAX); ++k) {                         \
                        if ((c&0xF0) >> 4) {                                \
                            *dstp++ = NUM_GRAYS * ((c&0xF0) >> 4) / 15 - 1; \
                        } else {                                            \
                            *dstp++ = 0x00;                                 \
                        }                                                   \
                        c <<= 4;                                            \
                    }                                                       \
                }

                if (mono) {
                    if (src->pixel_mode == FT_PIXEL_MODE_MONO) {
                        while (quotient--) {
                            MONO_MONO(8);
                        }
                        MONO_MONO(remainder);
                    } else if (src->pixel_mode == FT_PIXEL_MODE_GRAY2) {
                        while (quotient--) {
                            MONO_GRAY2(4);
                        }
                        MONO_GRAY2(remainder);
                    } else if (src->pixel_mode == FT_PIXEL_MODE_GRAY4) {
                        while (quotient--) {
                            MONO_GRAY4(2);
                        }
                        MONO_GRAY4(remainder);
                    } else {
                        while (quotient--) {
                            unsigned char c = *srcp++;
                            *dstp++ = (c >= 0x80) ? 1 : 0;
                        }
                    }
                } else if (src->pixel_mode == FT_PIXEL_MODE_MONO) {
                    /* This special case wouldn't be here if the FT_Render_Glyph()
                     * function wasn't buggy when it tried to render a .fon font with 256
                     * shades of gray.  Instead, it returns a black and white surface
                     * and we have to translate it back to a 256 gray shaded surface. */
                    while (quotient--) {
                        NORMAL_MONO(8);
                    }
                    NORMAL_MONO(remainder);
                } else if (src->pixel_mode == FT_PIXEL_MODE_GRAY2) {
                    while (quotient--) {
                        NORMAL_GRAY2(4);
                    }
                    NORMAL_GRAY2(remainder);
                } else if (src->pixel_mode == FT_PIXEL_MODE_GRAY4) {
                    while (quotient--) {
                        NORMAL_GRAY4(2);
                    }
                    NORMAL_GRAY4(remainder);
#if TTF_USE_COLOR
                } else if (src->pixel_mode == FT_PIXEL_MODE_BGRA) {
                    SDL_memcpy(dstp, srcp, 4 * src->width);
#endif
                } else if (src->pixel_mode == FT_PIXEL_MODE_LCD) {
                    while (quotient--) {
                        Uint8 alpha = 0;
                        Uint8 r, g, b;
                        r = *srcp++;
                        g = *srcp++;
                        b = *srcp++;
                        *dstp++ = b;
                        *dstp++ = g;
                        *dstp++ = r;
                        *dstp++ = alpha;
                    }
                } else {
#if TTF_USE_SDF
                    if (ft_render_mode != FT_RENDER_MODE_SDF) {
                        SDL_memcpy(dstp, srcp, src->width);
                    } else {
                        unsigned int x;
                        for (x = 0; x < src->width; x++) {
                            Uint8 s = srcp[x];
                            Uint8 d;
                            if (s < 128) {
                                d = 256 - (128 - s) * 2;
                            } else {
                                d = 255;
                                /* some glitch ?
                                if (s == 255) {
                                    d = 0;
                                }*/
                            }
                            dstp[x] = d;
                        }
                    }
#else
                    SDL_memcpy(dstp, srcp, src->width);
#endif
                }
            }
        }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

        // Handle the bold style
        if (TTF_HANDLE_STYLE_BOLD(font)) {
            int row;
            // The pixmap is a little hard, we have to add and clamp
            for (row = dst->rows - 1; row >= 0; --row) {
                Uint8 *pixmap = dst->buffer + row * dst->pitch;
                int col, offset;
                // Minimal memset
                // SDL_memset(pixmap + dst->width - font->glyph_overhang, 0, font->glyph_overhang);
                for (offset = 1; offset <= font->glyph_overhang; ++offset) {
                    for (col = dst->width - 1; col > 0; --col) {
                        if (mono) {
                            pixmap[col] |= pixmap[col-1];
                        } else {
                            int pixel = (pixmap[col] + pixmap[col-1]);
                            if (pixel > NUM_GRAYS - 1) {
                                pixel = NUM_GRAYS - 1;
                            }
                            pixmap[col] = (Uint8) pixel;
                        }
                    }
                }
            }
        }

        // Shift back
        if (dst->buffer) {
            dst->buffer -= alignment;
        }

#if TTF_USE_COLOR
        if (src->pixel_mode == FT_PIXEL_MODE_BGRA) {
            dst->is_color = 1;
        } else {
            dst->is_color = 0;
        }
#else
        dst->is_color = 0;
#endif

        // Mark that we rendered this format
        if (mono) {
            cached->stored |= CACHED_BITMAP;
        } else if (src->pixel_mode == FT_PIXEL_MODE_LCD) {
            cached->stored |= CACHED_LCD;
        } else {
#if TTF_USE_COLOR
            if (want & CACHED_COLOR) {
                cached->stored |= CACHED_COLOR;
                /* Most of the time, glyphs loaded with FT_LOAD_COLOR are non colored, so the cache is
                   also suitable for Shaded rendering (eg, loaded without FT_LOAD_COLOR) */
                if (dst->is_color == 0) {
                    cached->stored |= CACHED_PIXMAP;
                }
            } else {
                cached->stored |= CACHED_PIXMAP;
                // If font has no color information, Shaded/Pixmap cache is also suitable for Blend/Color
                if (!FT_HAS_COLOR(font->face)) {
                    cached->stored |= CACHED_COLOR;
                }
            }
#else
            cached->stored |= CACHED_COLOR | CACHED_PIXMAP;
#endif
        }

        // Free outlined glyph
        if (glyph) {
            FT_Done_Glyph(glyph);
        }
    }

    // We're done, this glyph is cached since 'stored' is not 0
    return true;
}

static bool Find_GlyphByIndex(TTF_Font *font, FT_UInt idx,
        int want_bitmap, int want_pixmap, int want_color, int want_lcd, int want_subpixel,
        int translation, c_glyph **out_glyph, TTF_Image **out_image)
{
    // cache size is 256, get key by masking
    c_glyph *glyph = &font->cache[idx & 0xff];

    if (out_glyph) {
        *out_glyph = glyph;
    }

    if (want_pixmap || want_color || want_lcd) {
        *out_image = &glyph->pixmap;
    }

    if (want_bitmap) {
        *out_image = &glyph->bitmap;
    }

    if (want_subpixel) {
        /* Not a real cache, but if it always advances by integer pixels (eg translation 0 or same as previous),
         * this allows to render as fast as normal mode. */
        int want = CACHED_METRICS | want_bitmap | want_pixmap | want_color | want_lcd | want_subpixel;

        if (glyph->stored && glyph->index != idx) {
            Flush_Glyph(glyph);
        }

        if (glyph->subpixel.translation == translation) {
            want &= ~CACHED_SUBPIX;
        }

        if ((glyph->stored & want) == want) {
            return true;
        }

        if (want_color || want_pixmap || want_lcd) {
            if (glyph->stored & (CACHED_COLOR|CACHED_PIXMAP|CACHED_LCD)) {
                Flush_Glyph(glyph);
            }
        }

        glyph->index = idx;
        return Load_Glyph(font, glyph, want, translation);
    } else {
        const int want = CACHED_METRICS | want_bitmap | want_pixmap | want_color | want_lcd;

        // Faster check as it gets inlined
        if (want_pixmap) {
            if ((glyph->stored & CACHED_PIXMAP) && glyph->index == idx) {
                return true;
            }
        } else if (want_bitmap) {
            if ((glyph->stored & CACHED_BITMAP) && glyph->index == idx) {
                return true;
            }
        } else if (want_color) {
            if ((glyph->stored & CACHED_COLOR) && glyph->index == idx) {
                return true;
            }
        } else if (want_lcd) {
            if ((glyph->stored & CACHED_LCD) && glyph->index == idx) {
                return true;
            }
        } else {
            // Get metrics
            if (glyph->stored && glyph->index == idx) {
                return true;
            }
        }

        /* Cache cannot contain both PIXMAP and COLOR (unless COLOR is actually not colored) and LCD
           So, if it's already used, clear it */
        if (want_color || want_pixmap || want_lcd) {
            if (glyph->stored & (CACHED_COLOR|CACHED_PIXMAP|CACHED_LCD)) {
                Flush_Glyph(glyph);
            }
        }

        if (glyph->stored && glyph->index != idx) {
            Flush_Glyph(glyph);
        }

        glyph->index = idx;
        return Load_Glyph(font, glyph, want, 0);
    }
}

static FT_UInt get_char_index(TTF_Font *font, Uint32 ch)
{
    Uint32 cache_index_size = sizeof (font->cache_index) / sizeof (font->cache_index[0]);

    if (ch < cache_index_size) {
        FT_UInt idx = font->cache_index[ch];
        if (idx) {
            return idx;
        }
        idx = FT_Get_Char_Index(font->face, ch);
        font->cache_index[ch] = idx;
        return idx;
    }

    return FT_Get_Char_Index(font->face, ch);
}


static bool Find_GlyphMetrics(TTF_Font *font, Uint32 ch, c_glyph **out_glyph)
{
    TTF_CHECK_FONT(font, false);

    FT_UInt idx = get_char_index(font, ch);
    return Find_GlyphByIndex(font, idx, 0, 0, 0, 0, 0, 0, out_glyph, NULL);
}

bool TTF_FontHasGlyph(TTF_Font *font, Uint32 ch)
{
    TTF_CHECK_FONT(font, false);

    return (get_char_index(font, ch) > 0);
}

SDL_Surface *TTF_GetGlyphImage(TTF_Font *font, Uint32 ch)
{
    FT_UInt idx;

    TTF_CHECK_FONT(font, false);

    idx = get_char_index(font, ch);
    if (idx == 0) {
        SDL_SetError("Codepoint not in font");
        return NULL;
    }

    return TTF_GetGlyphImageForIndex(font, idx);
}

SDL_Surface *TTF_GetGlyphImageForIndex(TTF_Font *font, Uint32 glyph_index)
{
    const int alignment = Get_Alignment() - 1;
    TTF_Image *image;
    SDL_Surface *surface;
    const Uint8 *src;

    TTF_CHECK_FONT(font, NULL);

    if (!Find_GlyphByIndex(font, glyph_index, 0, CACHED_PIXMAP, 0, 0, 0, 0, NULL, &image)) {
        return NULL;
    }

    surface = SDL_CreateSurface(image->width, image->rows, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        return NULL;
    }

    src = image->buffer + alignment;

    if (image->is_color) {
        if (surface->pitch == image->pitch) {
            SDL_memcpy(surface->pixels, src, image->rows * image->pitch);
        } else {
            int row;
            Uint8 *dst = (Uint8 *)surface->pixels;
            size_t length = image->width * 4;
            for (row = 0; row < image->rows; ++row) {
                SDL_memcpy(dst, src, length);
                src += image->pitch;
                dst += surface->pitch;
            }
        }
    } else {
        int row, col;
        Uint32 *dst = (Uint32 *)surface->pixels;
        int skip = (surface->pitch - surface->w * 4) / 4;
        for (row = 0; row < image->rows; ++row) {
            for (col = 0; col < image->width; ++col) {
                Uint32 v = src[col];
                *dst++ = (0x00FFFFFF | v << 24);
            }
            src += image->pitch;
            dst += skip;
        }
    }
    return surface;
}

bool TTF_GetGlyphMetrics(TTF_Font *font, Uint32 ch, int *minx, int *maxx, int *miny, int *maxy, int *advance)
{
    c_glyph *glyph;

    TTF_CHECK_FONT(font, false);

    if (!Find_GlyphMetrics(font, ch, &glyph)) {
        return false;
    }

    if (minx) {
        *minx = glyph->sz_left;
    }
    if (maxx) {
        *maxx = glyph->sz_left + glyph->sz_width;
        *maxx += 2 * font->outline;
    }
    if (miny) {
        *miny = glyph->sz_top - glyph->sz_rows;
    }
    if (maxy) {
        *maxy = glyph->sz_top;
        *maxy += 2 * font->outline;
    }
    if (advance) {
        *advance = FT_CEIL(glyph->advance);
    }
    return true;
}

bool TTF_GetGlyphKerning(TTF_Font *font, Uint32 previous_ch, Uint32 ch, int *kerning)
{
    FT_Error error;
    c_glyph *prev_glyph, *glyph;
    FT_Vector delta;

    if (kerning) {
        *kerning = 0;
    }

    TTF_CHECK_FONT(font, false);

    if (ch == UNICODE_BOM_NATIVE || ch == UNICODE_BOM_SWAPPED) {
        return true;
    }

    if (previous_ch == UNICODE_BOM_NATIVE || previous_ch == UNICODE_BOM_SWAPPED) {
        return true;
    }

    if (!Find_GlyphMetrics(font, ch, &glyph)) {
        return false;
    }

    if (!Find_GlyphMetrics(font, previous_ch, &prev_glyph)) {
        return false;
    }

    error = FT_Get_Kerning(font->face, prev_glyph->index, glyph->index, FT_KERNING_DEFAULT, &delta);
    if (error) {
        return TTF_SetFTError("Couldn't get glyph kerning", error);
    }

    if (kerning) {
        *kerning = (int)(delta.x >> 6);
    }
    return true;
}

static bool TTF_Size_Internal(TTF_Font *font, const char *text, size_t length, int *w, int *h, int *xstart, int *ystart, bool measure_width, int max_width, int *measured_width, size_t *measured_length)
{
    int x = 0;
    int pos_x, pos_y;
    int minx = 0, maxx = 0;
    int miny = 0, maxy = 0;
    c_glyph *glyph;
#if TTF_USE_HARFBUZZ
    hb_direction_t hb_direction;
    hb_script_t hb_script;
    hb_buffer_t *hb_buffer = NULL;
    unsigned int g;
    unsigned int glyph_count;
    hb_glyph_info_t *hb_glyph_info;
    hb_glyph_position_t *hb_glyph_position;
    int y = 0;
    int advance_if_bold = 0;
#else
    int skip_first = 1;
    FT_UInt prev_index = 0;
    FT_Pos  prev_delta = 0;
#endif
    int prev_advance = 0;

    if (w) {
        *w = 0;
    }
    if (h) {
        *h = 0;
    }
    if (measured_width) {
        *measured_width = 0;
    }
    if (measured_length) {
        *measured_length = 0;
    }

    TTF_CHECK_INITIALIZED(false);
    TTF_CHECK_POINTER("font", font, false);
    TTF_CHECK_POINTER("text", text, false);

    if (!length) {
        length = SDL_strlen(text);
    }
    if (measured_length) {
        *measured_length = length;
    }

    maxy = font->height;

    // Reset buffer
    font->pos_len = 0;
    font->num_clusters = 0;

    int last_offset = -1;

#if TTF_USE_HARFBUZZ

    // Adjust for bold text
    if (TTF_HANDLE_STYLE_BOLD(font)) {
        advance_if_bold = F26Dot6(font->glyph_overhang);
    }

    // Create a buffer for harfbuzz to use
    hb_buffer = hb_buffer_create();
    if (hb_buffer == NULL) {
       SDL_SetError("Cannot create harfbuzz buffer");
       goto failure;
    }


    hb_direction = font->hb_direction;
    hb_script = font->hb_script;

    // Set global configuration
    hb_buffer_set_language(hb_buffer, font->hb_language);
    hb_buffer_set_direction(hb_buffer, hb_direction);
    hb_buffer_set_script(hb_buffer, hb_script);
    hb_buffer_guess_segment_properties(hb_buffer);

    // Layout the text
    hb_buffer_add_utf8(hb_buffer, text, (int)length, 0, -1);

    hb_feature_t userfeatures[1];
    userfeatures[0].tag = HB_TAG('k','e','r','n');
    userfeatures[0].value = font->enable_kerning;
    userfeatures[0].start = HB_FEATURE_GLOBAL_START;
    userfeatures[0].end = HB_FEATURE_GLOBAL_END;

    hb_shape(font->hb_font, hb_buffer, userfeatures, 1);

    // Get the result
    hb_glyph_info = hb_buffer_get_glyph_infos(hb_buffer, &glyph_count);
    hb_glyph_position = hb_buffer_get_glyph_positions(hb_buffer, &glyph_count);

    // Load and render each character
    int offset = 0;
    for (g = 0; g < glyph_count; g++) {
        FT_UInt idx   = hb_glyph_info[g].codepoint;
        int x_advance = hb_glyph_position[g].x_advance;
        int y_advance = hb_glyph_position[g].y_advance;
        int x_offset  = hb_glyph_position[g].x_offset;
        int y_offset  = hb_glyph_position[g].y_offset;

        offset = (int)hb_glyph_info[g].cluster;
#else
    // Load each character and sum it's bounding box
    int offset = 0;
    while (length > 0) {
        const char *last = text;
        Uint32 c = SDL_StepUTF8(&text, &length);
        FT_UInt idx = get_char_index(font, c);

        offset += (text - last);

        if (c == UNICODE_BOM_NATIVE || c == UNICODE_BOM_SWAPPED) {
            continue;
        }
#endif
        if (!Find_GlyphByIndex(font, idx, 0, 0, 0, 0, 0, 0, &glyph, NULL)) {
            goto failure;
        }

        // Realloc, if needed
        if (font->pos_len >= font->pos_max) {
            PosBuf_t *saved = font->pos_buf;
            font->pos_max *= 2;
            font->pos_buf = (PosBuf_t *)SDL_realloc(font->pos_buf, font->pos_max * sizeof (font->pos_buf[0]));
            if (font->pos_buf == NULL) {
                font->pos_max /= 2;
                font->pos_buf = saved;
                SDL_SetError("Out of memory");
                goto failure;
            }
        }

#if TTF_USE_HARFBUZZ
        // Compute positions
        pos_x  = x                     + x_offset;
        pos_y  = y + F26Dot6(font->ascent) - y_offset;
        x     += x_advance + advance_if_bold;
        y     += y_advance;
#else
        // Compute positions
        x += prev_advance;
        prev_advance = glyph->advance;
        if (font->use_kerning) {
            if (prev_index && glyph->index) {
                FT_Vector delta;
                FT_Get_Kerning(font->face, prev_index, glyph->index, FT_KERNING_UNFITTED, &delta);
                x += delta.x;
            }
            prev_index = glyph->index;
        }
        // FT SUBPIXEL : LCD_MODE_LIGHT_SUBPIXEL
        if (font->render_subpixel) {
            x += prev_delta;
            // Increment by prev_glyph->lsb_delta - prev_glyph->rsb_delta;
            prev_delta = glyph->subpixel.lsb_minus_rsb;
        } else {
            // FT KERNING_MODE_SMART: Use `lsb_delta' and `rsb_delta' to improve integer positioning of glyphs
            if (skip_first) {
                skip_first = 0;
            } else {
                if (prev_delta - glyph->kerning_smart.lsb_delta >  32 ) {
                    x -= 64;
                } else if (prev_delta - glyph->kerning_smart.lsb_delta < -31 ) {
                    x += 64;
                }
            }
            prev_delta = glyph->kerning_smart.rsb_delta;
            x = ((x + 32) & -64); // ROUND()
        }

        // Compute positions where to copy the glyph bitmap
        pos_x = x;
        pos_y = F26Dot6(font->ascent);
#endif
        // Store things for Render_Line()
        font->pos_buf[font->pos_len].x      = pos_x;
        font->pos_buf[font->pos_len].y      = pos_y;
        font->pos_buf[font->pos_len].index  = idx;
        font->pos_buf[font->pos_len].offset = offset;
        font->pos_len += 1;

        // Save the number of clusters we've seen
        if (offset != last_offset) {
            ++font->num_clusters;
            last_offset = offset;
        }

        // Compute provisional global bounding box
        pos_x = FT_FLOOR(pos_x) + glyph->sz_left;
        pos_y = FT_FLOOR(pos_y) - glyph->sz_top;

        minx = SDL_min(minx, pos_x);
        maxx = SDL_max(maxx, pos_x + glyph->sz_width);
        miny = SDL_min(miny, pos_y);
        maxy = SDL_max(maxy, pos_y + glyph->sz_rows);

        // Measurement mode
        if (measure_width) {
            int cw = SDL_max(maxx, FT_FLOOR(x + prev_advance)) - minx;
            cw += 2 * font->outline;
            if (!max_width || cw <= max_width) {
                if (measured_width) {
                    *measured_width = cw;
                }
            } else {
                if (measured_length) {
                    *measured_length = (size_t)offset;
                }
                break;
            }
        }
    }

    // Allows to render a string with only one space (bug 4344).
    maxx = SDL_max(maxx, FT_FLOOR(x + prev_advance));

    /* Initial x start position: often 0, except when a glyph would be written at
     * a negative position. In this case an offset is needed for the whole line. */
    if (xstart) {
        *xstart = (minx < 0)? -minx : 0;
        *xstart += font->outline;
        if (font->render_sdf) {
            *xstart += 8; // Default 'spread' property
        }
    }

    // Initial y start: compensation for a negative y offset
    if (ystart) {
        *ystart = (miny < 0)? -miny : 0;
        *ystart += font->outline;
        if (font->render_sdf) {
            *ystart += 8; // Default 'spread' property
        }
    }

    // Fill the bounds rectangle
    if (w) {
        *w = (maxx - minx);
        if (*w != 0) {
            *w += 2 * font->outline;
        }
    }
    if (h) {
        *h = (maxy - miny);
        *h += 2 * font->outline;
    }

#if TTF_USE_HARFBUZZ
    if (hb_buffer) {
        hb_buffer_destroy(hb_buffer);
    }
#endif
    return true;

failure:
#if TTF_USE_HARFBUZZ
    if (hb_buffer) {
        hb_buffer_destroy(hb_buffer);
    }
#endif
    return false;
}

bool TTF_GetStringSize(TTF_Font *font, const char *text, size_t length, int *w, int *h)
{
    return TTF_Size_Internal(font, text, length, w, h, NULL, NULL, NO_MEASUREMENT);
}

bool TTF_MeasureString(TTF_Font *font, const char *text, size_t length, int max_width, int *measured_width, size_t *measured_length)
{
    return TTF_Size_Internal(font, text, length, NULL, NULL, NULL, NULL, true, max_width, measured_width, measured_length);
}

static SDL_Surface* TTF_Render_Internal(TTF_Font *font, const char *text, size_t length, SDL_Color fg, SDL_Color bg, const render_mode_t render_mode)
{
    Uint32 color;
    int xstart, ystart, width, height;
    SDL_Surface *textbuf = NULL;

    TTF_CHECK_INITIALIZED(NULL);
    TTF_CHECK_POINTER("font", font, NULL);
    TTF_CHECK_POINTER("text", text, NULL);

    if (!length) {
        length = SDL_strlen(text);
    }

    if (render_mode == RENDER_LCD && !FT_IS_SCALABLE(font->face)) {
        SDL_SetError("LCD rendering is not available for non-scalable font");
        goto failure;
    }

#if TTF_USE_SDF
    // Invalid cache if we were using SDF
    if (render_mode != RENDER_BLENDED) {
        if (font->render_sdf) {
            font->render_sdf = false;
            Flush_Cache(font);
        }
    }
#endif

    // Get the dimensions of the text surface
    if (!TTF_Size_Internal(font, text, length, &width, &height, &xstart, &ystart, NO_MEASUREMENT) || !width) {
        SDL_SetError("Text has zero width");
        goto failure;
    }

    // Create surface for rendering
    if (fg.a == SDL_ALPHA_TRANSPARENT) {
        fg.a = SDL_ALPHA_OPAQUE;
    }
    if (render_mode == RENDER_SOLID) {
        textbuf = Create_Surface_Solid(width, height, fg, &color);
    } else if (render_mode == RENDER_SHADED) {
        textbuf = Create_Surface_Shaded(width, height, fg, bg, &color);
    } else if (render_mode == RENDER_BLENDED) {
        textbuf = Create_Surface_Blended(width, height, fg, &color);
    } else { // render_mode == RENDER_LCD
        textbuf = Create_Surface_LCD(width, height, fg, bg, &color);
    }

    if (textbuf == NULL) {
        goto failure;
    }

    // Render one text line to textbuf at (xstart, ystart)
    if (!Render_Line(render_mode, font->render_subpixel, font, textbuf, xstart, ystart, fg)) {
        goto failure;
    }

    // Apply underline or strikethrough style, if needed
    if (TTF_HANDLE_STYLE_UNDERLINE(font)) {
        Draw_Line(font, textbuf, 0, ystart + font->underline_top_row, width, font->line_thickness, color, render_mode);
    }

    if (TTF_HANDLE_STYLE_STRIKETHROUGH(font)) {
        Draw_Line(font, textbuf, 0, ystart + font->strikethrough_top_row, width, font->line_thickness, color, render_mode);
    }

    return textbuf;
failure:
    if (textbuf) {
        SDL_DestroySurface(textbuf);
    }
    return NULL;
}

SDL_Surface* TTF_RenderText_Solid(TTF_Font *font, const char *text, size_t length, SDL_Color fg)
{
    return TTF_Render_Internal(font, text, length, fg, fg /* unused */, RENDER_SOLID);
}

SDL_Surface* TTF_RenderGlyph_Solid(TTF_Font *font, Uint32 ch, SDL_Color fg)
{
    char utf8[4], *end;

    end = SDL_UCS4ToUTF8(ch, utf8);

    return TTF_RenderText_Solid(font, utf8, (end - utf8), fg);
}

SDL_Surface* TTF_RenderText_Shaded(TTF_Font *font, const char *text, size_t length, SDL_Color fg, SDL_Color bg)
{
    return TTF_Render_Internal(font, text, length, fg, bg, RENDER_SHADED);
}

SDL_Surface* TTF_RenderGlyph_Shaded(TTF_Font *font, Uint32 ch, SDL_Color fg, SDL_Color bg)
{
    char utf8[4], *end;

    end = SDL_UCS4ToUTF8(ch, utf8);

    return TTF_RenderText_Shaded(font, utf8, (end - utf8), fg, bg);
}

SDL_Surface* TTF_RenderText_Blended(TTF_Font *font, const char *text, size_t length, SDL_Color fg)
{
    return TTF_Render_Internal(font, text, length, fg, fg /* unused */, RENDER_BLENDED);
}

SDL_Surface* TTF_RenderGlyph_Blended(TTF_Font *font, Uint32 ch, SDL_Color fg)
{
    char utf8[4], *end;

    end = SDL_UCS4ToUTF8(ch, utf8);

    return TTF_RenderText_Blended(font, utf8, (end - utf8), fg);
}

SDL_Surface* TTF_RenderText_LCD(TTF_Font *font, const char *text, size_t length, SDL_Color fg, SDL_Color bg)
{
    return TTF_Render_Internal(font, text, length, fg, bg, RENDER_LCD);
}


SDL_Surface* TTF_RenderGlyph_LCD(TTF_Font *font, Uint32 ch, SDL_Color fg, SDL_Color bg)
{
    char utf8[4], *end;

    end = SDL_UCS4ToUTF8(ch, utf8);

    return TTF_RenderText_LCD(font, utf8, (end - utf8), fg, bg);
}

static bool CharacterIsDelimiter(Uint32 c)
{
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        return true;
    }
    return false;
}

static bool CharacterIsNewLine(Uint32 c)
{
    if (c == '\n') {
        return true;
    }
    return false;
}

static bool GetWrappedLines(TTF_Font *font, const char *text, size_t length, int xoffset, int wrap_width, bool trim_whitespace, TTF_Line **lines, int *num_lines, int *w, int *h)
{
    int width, height;
    int i, numLines = 0, rowHeight;
    TTF_Line *strLines = NULL;
    bool result = false;

    if (w) {
        *w = 0;
    }
    if (h) {
        *h = 0;
    }

    TTF_CHECK_INITIALIZED(false);
    TTF_CHECK_POINTER("font", font, false);
    TTF_CHECK_POINTER("text", text, false);
    if (wrap_width < 0) {
        return SDL_InvalidParamError("wrap_width");
    }

    if (!length) {
        length = SDL_strlen(text);
    }

    // Get the dimensions of the text surface
    if (!TTF_GetStringSize(font, text, length, &width, &height) || !width) {
        return SDL_SetError("Text has zero width");
    }

    if (*text) {
        int maxNumLines = 0;
        const char *spot = text;
        size_t left = length;

        do {
            const char *save_text = NULL;
            size_t save_length = (size_t)(-1);

            if (numLines >= maxNumLines) {
                TTF_Line *new_lines;
                if (wrap_width == 0) {
                    maxNumLines += 32;
                } else {
                    maxNumLines += (width / wrap_width) + 1;
                }
                new_lines = (TTF_Line *)SDL_realloc(strLines, maxNumLines * sizeof (*strLines));
                if (new_lines == NULL) {
                    goto done;
                }
                strLines = new_lines;
            }

            if (trim_whitespace && spot > text && spot[-1] != '\n') {
                const char *next_spot = spot;
                size_t next_left = left;
                for (;;) {
                    Uint32 c = SDL_StepUTF8(&next_spot, &next_left);
                    if (c == 0 || (c != ' ' && c != '\t')) {
                        break;
                    }
                    spot = next_spot;
                    left = next_left;
                }
            }

            if (numLines > 0) {
                strLines[numLines - 1].length = spot - strLines[numLines - 1].text;
            }
            strLines[numLines].text = spot;
            strLines[numLines].length = left;
            ++numLines;

            int max_width = wrap_width;
            if (max_width > 0) {
                max_width = SDL_max(max_width - xoffset, 1);
            }
            size_t max_length = 0;
            if (!TTF_MeasureString(font, spot, left, max_width, NULL, &max_length)) {
                SDL_SetError("Error measure text");
                goto done;
            }

            if (wrap_width != 0) {
                // The first line can be empty if we have a text position that's
                // at the edge of the wrap length, but subsequent lines should have
                // at least one character per line.
                if (max_length == 0 && numLines > 1) {
                    max_length = 1;
                }
            }

            const char *end = spot + max_length;
            while (spot < end) {
                int is_delim;
                Uint32 c = SDL_StepUTF8(&spot, &left);

                if (c == UNICODE_BOM_NATIVE || c == UNICODE_BOM_SWAPPED) {
                    continue;
                }

                // With wrap_width == 0, normal text rendering but newline aware
                is_delim = (wrap_width > 0) ? CharacterIsDelimiter(c) : CharacterIsNewLine(c);

                // Record last delimiter position
                if (is_delim) {
                    save_text = spot;
                    save_length = left;
                    // Break, if new line
                    if (c == '\n' || (c == '\r' && *spot != '\n')) {
                        break;
                    }
                }
            }

            // Cut at last delimiter/new lines, otherwise in the middle of the word
            if (save_text && left > 0) {
                spot = save_text;
                left = save_length;
            }

            // First line is complete, start the next at offset 0
            xoffset = 0;

        } while (left > 0);

        for (i = 0; i < numLines; ++i) {
            TTF_Line *line = &strLines[i];
            if (line->length == 0) {
                continue;
            }

            // The line doesn't include any delimiter that caused it to be wrapped.
            if (CharacterIsNewLine(line->text[line->length - 1])) {
                --line->length;
                if (line->text[line->length - 1] == '\r') {
                    --line->length;
                }
            } else if (i < (numLines - 1) &&
                       CharacterIsDelimiter(line->text[line->length - 1])) {
                --line->length;
            }

            if (trim_whitespace) {
                while (line->length > 0 &&
                       CharacterIsDelimiter(line->text[line->length - 1])) {
                    --line->length;
                }
            }
        }
    }

    rowHeight = SDL_max(height, font->lineskip);

    if (wrap_width == 0) {
        // Find the max of all line lengths
        if (numLines > 1) {
            width = 0;
            for (i = 0; i < numLines; i++) {
                int w_tmp, h_tmp;

                if (TTF_GetStringSize(font, strLines[i].text, strLines[i].length, &w_tmp, &h_tmp)) {
                    width = SDL_max(w_tmp, width);
                }
            }
            // In case there are all newlines
            width = SDL_max(width, 1);
        }
    } else {
        if (numLines <= 1 && font->horizontal_align == TTF_HORIZONTAL_ALIGN_LEFT) {
            // Don't go above wrap_width if you have only 1 line which hasn't been cut
            width = SDL_min((int)wrap_width, width);
        } else {
            width = wrap_width;
        }
    }
    height = rowHeight + font->lineskip * (numLines - 1);

    result = true;

done:
    if (result) {
        if (lines) {
            *lines = strLines;
        } else {
            SDL_free(strLines);
        }
        if (num_lines) {
            *num_lines = numLines;
        }
        if (w) {
            *w = width;
        }
        if (h) {
            *h = height;
        }
    } else {
        SDL_free(strLines);
    }
    return result;
}

bool TTF_GetStringSizeWrapped(TTF_Font *font, const char *text, size_t length, int wrap_width, int *w, int *h)
{
    return GetWrappedLines(font, text, length, 0, wrap_width, true, NULL, NULL, w, h);
}

static SDL_Surface* TTF_Render_Wrapped_Internal(TTF_Font *font, const char *text, size_t length, SDL_Color fg, SDL_Color bg, int wrap_width, const render_mode_t render_mode)
{
    Uint32 color;
    int width, height;
    SDL_Surface *textbuf = NULL;
    int i, numLines = 0;
    TTF_Line *strLines = NULL;

    if (!GetWrappedLines(font, text, length, 0, wrap_width, true, &strLines, &numLines, &width, &height)) {
        return NULL;
    }

    if (render_mode == RENDER_LCD && !FT_IS_SCALABLE(font->face)) {
        SDL_SetError("LCD rendering is not available for non-scalable font");
        goto failure;
    }

#if TTF_USE_SDF
    // Invalid cache if we were using SDF
    if (render_mode != RENDER_BLENDED) {
        if (font->render_sdf) {
            font->render_sdf = false;
            Flush_Cache(font);
        }
    }
#endif

    // Create surface for rendering
    if (fg.a == SDL_ALPHA_TRANSPARENT) {
        fg.a = SDL_ALPHA_OPAQUE;
    }
    if (render_mode == RENDER_SOLID) {
        textbuf = Create_Surface_Solid(width, height, fg, &color);
    } else if (render_mode == RENDER_SHADED) {
        textbuf = Create_Surface_Shaded(width, height, fg, bg, &color);
    } else if (render_mode == RENDER_BLENDED) {
        textbuf = Create_Surface_Blended(width, height, fg, &color);
    } else { // render_mode == RENDER_LCD
        textbuf = Create_Surface_LCD(width, height, fg, bg, &color);
    }

    if (textbuf == NULL) {
        goto failure;
    }

    // Render each line
    for (i = 0; i < numLines; i++) {
        int xstart, ystart, line_width, xoffset;

        // Initialize xstart, ystart and compute positions
        if (!TTF_Size_Internal(font, strLines[i].text, strLines[i].length, &line_width, NULL, &xstart, &ystart, NO_MEASUREMENT)) {
            goto failure;
        }

        // Move to i-th line
        ystart += i * font->lineskip;

        // Control left/right/center align of each bit of text
        if (font->horizontal_align == TTF_HORIZONTAL_ALIGN_RIGHT) {
            xoffset = (width - line_width);
        } else if (font->horizontal_align == TTF_HORIZONTAL_ALIGN_CENTER) {
            xoffset = (width - line_width) / 2;
        } else {
            xoffset = 0;
        }
        xoffset = SDL_max(0, xoffset);

        // Render one text line to textbuf at (xstart, ystart)
        if (!Render_Line(render_mode, font->render_subpixel, font, textbuf, xstart + xoffset, ystart, fg)) {
            goto failure;
        }

        // Apply underline or strikethrough style, if needed
        if (TTF_HANDLE_STYLE_UNDERLINE(font)) {
            Draw_Line(font, textbuf, xoffset, ystart + font->underline_top_row, line_width, font->line_thickness, color, render_mode);
        }

        if (TTF_HANDLE_STYLE_STRIKETHROUGH(font)) {
            Draw_Line(font, textbuf, xoffset, ystart + font->strikethrough_top_row, line_width, font->line_thickness, color, render_mode);
        }
    }

    if (strLines) {
        SDL_free(strLines);
    }
    return textbuf;

failure:
    if (textbuf) {
        SDL_DestroySurface(textbuf);
    }
    if (strLines) {
        SDL_free(strLines);
    }
    return NULL;
}

SDL_Surface* TTF_RenderText_Solid_Wrapped(TTF_Font *font, const char *text, size_t length, SDL_Color fg, int wrap_width)
{
    return TTF_Render_Wrapped_Internal(font, text, length, fg, fg /* unused */, wrap_width, RENDER_SOLID);
}

SDL_Surface* TTF_RenderText_Shaded_Wrapped(TTF_Font *font, const char *text, size_t length, SDL_Color fg, SDL_Color bg, int wrap_width)
{
    return TTF_Render_Wrapped_Internal(font, text, length, fg, bg, wrap_width, RENDER_SHADED);
}

SDL_Surface* TTF_RenderText_Blended_Wrapped(TTF_Font *font, const char *text, size_t length, SDL_Color fg, int wrap_width)
{
    return TTF_Render_Wrapped_Internal(font, text, length, fg, fg /* unused */, wrap_width, RENDER_BLENDED);
}

SDL_Surface* TTF_RenderText_LCD_Wrapped(TTF_Font *font, const char *text, size_t length, SDL_Color fg, SDL_Color bg, int wrap_width)
{
    return TTF_Render_Wrapped_Internal(font, text, length, fg, bg, wrap_width, RENDER_LCD);
}

struct TTF_TextLayout
{
    int wrap_length;
    bool wrap_whitespace_visible;
    int *lines;
};

typedef struct TTF_InternalText
{
    TTF_Text text;
    TTF_TextData internal;
    TTF_TextLayout layout;
} TTF_InternalText;

TTF_Text *TTF_CreateText(TTF_TextEngine *engine, TTF_Font *font, const char *text, size_t length)
{
    if (engine && engine->version < sizeof(*engine)) {
        // Update this to handle older versions of this interface
        SDL_SetError("Invalid engine, should be initialized with SDL_INIT_INTERFACE()");
        return NULL;
    }

    TTF_InternalText *mem = (TTF_InternalText *)SDL_calloc(1, sizeof(*mem));
    if (!mem) {
        return NULL;
    }

    TTF_Text *result = &mem->text;
    result->internal = &mem->internal;
    result->internal->layout = &mem->layout;
    result->internal->font = font;
    result->internal->color.r = 1.0f;
    result->internal->color.g = 1.0f;
    result->internal->color.b = 1.0f;
    result->internal->color.a = 1.0f;
    result->internal->needs_layout_update = true;
    result->internal->engine = engine;
    if (text && *text) {
        if (length == 0) {
            length = SDL_strlen(text);
        }

        result->text = (char *)SDL_malloc(length + 1);
        if (!result->text) {
            SDL_free(mem);
            return NULL;
        }
        SDL_memcpy(result->text, text, length);
        result->text[length] = '\0';
    }

    if (font) {
        AddFontTextReference(font, result);
    }
    return result;
}

static int SDLCALL SortClusters(const void *a, const void *b)
{
    TTF_SubString *A = (TTF_SubString *)a;
    TTF_SubString *B = (TTF_SubString *)b;

    if (A->offset == B->offset) {
        if (A->flags & TTF_SUBSTRING_LINE_END) {
            return -1;
        }
        if (B->flags & TTF_SUBSTRING_LINE_END) {
            return 1;
        }
        return 0;
    }
    return (A->offset - B->offset);
}

static int CalculateClusterLengths(TTF_Text *text, TTF_SubString *clusters, int num_clusters, size_t length, int *lines)
{
    SDL_qsort(clusters, num_clusters, sizeof(*clusters), SortClusters);

    TTF_Font *font = text->internal->font;
    int i;
    const TTF_SubString *src = clusters;
    TTF_SubString *last = NULL;
    int cluster_index = 0;
    for (i = 0; i < num_clusters; ++i, ++src) {
        // Merge zero width clusters
        if (last && src->offset == last->offset) {
            if (!(last->flags & TTF_SUBSTRING_LINE_END)) {
                last->flags |= src->flags;
                SDL_GetRectUnion(&last->rect, &src->rect, &last->rect);
                continue;
            }
        }

        TTF_SubString *cluster = &clusters[cluster_index];
        if (src != cluster) {
            SDL_copyp(cluster, src);
        }

        if (!last || cluster->line_index != last->line_index) {
            if (!last) {
                cluster->flags |= TTF_SUBSTRING_TEXT_START;
            }
            cluster->flags |= TTF_SUBSTRING_LINE_START;

            if (lines && cluster->line_index > 0) {
                lines[cluster->line_index - 1] = cluster_index;
            }
        }

        cluster->cluster_index = cluster_index++;

        if (cluster->flags & TTF_SUBSTRING_LINE_END) {
            if (cluster->flags & TTF_SUBSTRING_LINE_START) {
                cluster->rect.y = cluster->line_index * font->lineskip;
                cluster->rect.h = font->height;
            } else {
                SDL_copyp(&cluster->rect, &clusters[cluster->cluster_index - 1].rect);
                cluster->rect.x += cluster->rect.w;
                cluster->rect.w = 0;
            }
        } else if (cluster->flags & TTF_SUBSTRING_TEXT_END) {
            if (last) {
                if (last->length > 0 && text->text[last->offset + last->length - 1] == '\n') {
                    cluster->line_index = last->line_index + 1;
                    cluster->rect.y = (cluster->line_index * font->lineskip);
                    cluster->rect.h = font->height;
                } else {
                    cluster->line_index = last->line_index;
                    SDL_copyp(&cluster->rect, &last->rect);
                    cluster->rect.x += cluster->rect.w;
                    cluster->rect.w = 0;
                }
            } else {
                cluster->rect.h = font->height;
            }
        }

        if (i < (num_clusters - 1)) {
            cluster->length = clusters[i + 1].offset - cluster->offset;
        } else {
            SDL_assert(cluster->flags & TTF_SUBSTRING_TEXT_END);
            SDL_assert(cluster->offset == length);
        }
        last = cluster;
    }
    return cluster_index;
}

static bool LayoutText(TTF_Text *text)
{
    TTF_Font *font = text->internal->font;
    int wrap_width = text->internal->layout->wrap_length;
    bool trim_whitespace = !text->internal->layout->wrap_whitespace_visible;
    size_t length = SDL_strlen(text->text);
    int i, width = 0, height = 0, numLines = 0;
    TTF_Line *strLines = NULL;
    TTF_DrawOperation *ops = NULL, *new_ops;
    int num_ops = 0, max_ops = 0, extra_ops = 0, additional_ops;
    TTF_SubString *clusters = NULL, *new_clusters, *cluster;
    int num_clusters = 0, max_clusters = 0, cluster_offset;
    int *lines = NULL;
    bool result = false;

    if (!GetWrappedLines(font, text->text, length, text->internal->x, wrap_width, trim_whitespace, &strLines, &numLines, &width, &height)) {
        return true;
    }
    height += text->internal->y;

    if (TTF_HANDLE_STYLE_UNDERLINE(font)) {
        ++extra_ops;
    }
    if (TTF_HANDLE_STYLE_STRIKETHROUGH(font)) {
        ++extra_ops;
    }

    if (numLines > 1) {
        lines = (int *)SDL_malloc((numLines - 1) * sizeof(*lines));
        if (!lines) {
            goto done;
        }
        for (i = 0; i < (numLines - 1); ++i) {
            lines[i] = -1;
        }
    }

    max_clusters = numLines + 1;
    clusters = (TTF_SubString *)SDL_calloc(max_clusters, sizeof(*clusters));
    if (!clusters) {
        goto done;
    }

    // Render each line
    for (i = 0; i < numLines; i++) {
        int xstart, ystart, line_width, xoffset;

        if (strLines[i].length == 0) {
            cluster = &clusters[num_clusters++];
            cluster->flags = TTF_SUBSTRING_LINE_END;
            cluster->offset = (int)(uintptr_t)(strLines[i].text - text->text);
            cluster->line_index = i;
            continue;
        }

        // Initialize xstart, ystart and compute positions
        if (!TTF_Size_Internal(font, strLines[i].text, strLines[i].length, &line_width, NULL, &xstart, &ystart, NO_MEASUREMENT)) {
            goto done;
        }

        // Move to i-th line
        ystart += i * font->lineskip;

        ystart += text->internal->y;

        // Control left/right/center align of each bit of text
        if (font->horizontal_align == TTF_HORIZONTAL_ALIGN_RIGHT) {
            xoffset = (width - line_width);
        } else if (font->horizontal_align == TTF_HORIZONTAL_ALIGN_CENTER) {
            xoffset = (width - line_width) / 2;
        } else {
            xoffset = 0;
        }
        xoffset = SDL_max(0, xoffset);

        if (i == 0) {
            xoffset += text->internal->x;
        }

        // Allocate space for the operations on this line
        additional_ops = (font->pos_len + extra_ops);
        new_ops = (TTF_DrawOperation *)SDL_realloc(ops, (max_ops + additional_ops) * sizeof(*new_ops));
        if (!new_ops) {
            goto done;
        }
        SDL_memset(new_ops + max_ops, 0, additional_ops * sizeof(*new_ops));
        ops = new_ops;
        max_ops += additional_ops;

        // Allocate space for the clusters on this line
        new_clusters = (TTF_SubString *)SDL_realloc(clusters, (max_clusters + font->num_clusters) * sizeof(*new_clusters));
        if (!new_clusters) {
            goto done;
        }
        SDL_memset(new_clusters + max_clusters, 0, font->num_clusters * sizeof(*new_clusters));
        clusters = new_clusters;
        max_clusters += font->num_clusters;
        cluster_offset = (int)(uintptr_t)(strLines[i].text - text->text);

        // Create the text drawing operations
        if (!Render_Line_TextEngine(font, xstart + xoffset, ystart, width, height, ops, &num_ops, clusters, &num_clusters, cluster_offset, i)) {
            goto done;
        }
        cluster = &clusters[num_clusters++];
        cluster->flags = TTF_SUBSTRING_LINE_END;
        cluster->offset = (int)(uintptr_t)(strLines[i].text - text->text + strLines[i].length);
        cluster->line_index = i;

        // Apply underline or strikethrough style, if needed
        if (TTF_HANDLE_STYLE_UNDERLINE(font)) {
            Draw_Line_TextEngine(font, width, height, xoffset, ystart + font->underline_top_row, line_width, font->line_thickness, ops, &num_ops);
        }

        if (TTF_HANDLE_STYLE_STRIKETHROUGH(font)) {
            Draw_Line_TextEngine(font, width, height, xoffset, ystart + font->strikethrough_top_row, line_width, font->line_thickness, ops, &num_ops);
        }
    }
    cluster = &clusters[num_clusters++];
    cluster->flags = TTF_SUBSTRING_TEXT_END;
    cluster->offset = (int)length;

    num_clusters = CalculateClusterLengths(text, clusters, num_clusters, length, lines);

    result = true;

done:
    if (result) {
        text->num_lines = numLines;
        text->internal->w = width;
        text->internal->h = height;
        text->internal->num_ops = num_ops;
        text->internal->ops = ops;
        text->internal->num_clusters = num_clusters;
        text->internal->clusters = clusters;
        text->internal->layout->lines = lines;
    } else {
        SDL_free(ops);
        SDL_free(clusters);
        SDL_free(lines);
    }
    SDL_free(strLines);
    return result;
}

static void DestroyEngineText(TTF_Text *text)
{
    TTF_TextEngine *engine = text->internal->engine;
    if (engine && engine->DestroyText &&
        text->internal->engine_text) {
        engine->DestroyText(engine->userdata, text);
        text->internal->engine_text = NULL;
    }
}

static bool CreateEngineText(TTF_Text *text)
{
    TTF_TextEngine *engine = text->internal->engine;
    if (engine && engine->CreateText && text->internal->num_ops > 0) {
        if (!engine->CreateText(engine->userdata, text)) {
            return false;
        }
    }
    return true;
}

SDL_PropertiesID TTF_GetTextProperties(TTF_Text *text)
{
    TTF_CHECK_POINTER("text", text, 0);

    if (!text->internal->props) {
        text->internal->props = SDL_CreateProperties();
    }
    return text->internal->props;
}

bool TTF_SetTextEngine(TTF_Text *text, TTF_TextEngine *engine)
{
    TTF_CHECK_POINTER("text", text, false);

    if (engine == text->internal->engine) {
        return true;
    }

    if (engine && engine->version < sizeof(*engine)) {
        // Update this to handle older versions of this interface
        return SDL_SetError("Invalid engine, should be initialized with SDL_INIT_INTERFACE()");
    }

    DestroyEngineText(text);

    text->internal->engine = engine;
    text->internal->needs_engine_update = true;

    return true;
}

TTF_TextEngine *TTF_GetTextEngine(TTF_Text *text)
{
    TTF_CHECK_POINTER("text", text, NULL);

    return text->internal->engine;
}

bool TTF_SetTextFont(TTF_Text *text, TTF_Font *font)
{
    TTF_CHECK_POINTER("text", text, false);

    if (font == text->internal->font) {
        return true;
    }

    if (text->internal->font) {
        RemoveFontTextReference(text->internal->font, text);
    }

    text->internal->font = font;

    if (text->internal->font) {
        AddFontTextReference(text->internal->font, text);

        // Only update the text if we have a font available
        text->internal->needs_layout_update = true;
    }

    return true;
}

TTF_Font *TTF_GetTextFont(TTF_Text *text)
{
    TTF_CHECK_POINTER("text", text, NULL);

    return text->internal->font;
}

bool TTF_SetTextColor(TTF_Text *text, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    const float fR = (float)r / 255.0f;
    const float fG = (float)g / 255.0f;
    const float fB = (float)b / 255.0f;
    const float fA = (float)a / 255.0f;

    return TTF_SetTextColorFloat(text, fR, fG, fB, fA);
}

bool TTF_SetTextColorFloat(TTF_Text *text, float r, float g, float b, float a)
{
    TTF_CHECK_POINTER("text", text, false);

    text->internal->color.r = r;
    text->internal->color.g = g;
    text->internal->color.b = b;
    text->internal->color.a = a;
    return true;
}

bool TTF_GetTextColor(TTF_Text *text, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
    float fR = 1.0f, fG = 1.0f, fB = 1.0f, fA = 1.0f;

    if (!TTF_GetTextColorFloat(text, &fR, &fG, &fB, &fA)) {
        if (r) {
            *r = 255;
        }
        if (g) {
            *g = 255;
        }
        if (b) {
            *b = 255;
        }
        if (b) {
            *b = 255;
        }
        return false;
    }

    if (r) {
        *r = (Uint8)SDL_roundf(SDL_clamp(fR, 0.0f, 1.0f) * 255.0f);
    }
    if (g) {
        *g = (Uint8)SDL_roundf(SDL_clamp(fG, 0.0f, 1.0f) * 255.0f);
    }
    if (b) {
        *b = (Uint8)SDL_roundf(SDL_clamp(fB, 0.0f, 1.0f) * 255.0f);
    }
    if (a) {
        *a = (Uint8)SDL_roundf(SDL_clamp(fA, 0.0f, 1.0f) * 255.0f);
    }
    return true;
}

bool TTF_GetTextColorFloat(TTF_Text *text, float *r, float *g, float *b, float *a)
{
    SDL_FColor color;

    if (r) {
        *r = 1.0f;
    }
    if (g) {
        *g = 1.0f;
    }
    if (b) {
        *b = 1.0f;
    }
    if (a) {
        *a = 1.0f;
    }

    TTF_CHECK_POINTER("text", text, false);

    color = text->internal->color;

    if (r) {
        *r = color.r;
    }
    if (g) {
        *g = color.g;
    }
    if (b) {
        *b = color.b;
    }
    if (a) {
        *a = color.a;
    }
    return true;
}

bool TTF_SetTextPosition(TTF_Text *text, int x, int y)
{
    TTF_CHECK_POINTER("text", text, false);

    if (x != text->internal->x || y != text->internal->y) {
        text->internal->x = x;
        text->internal->y = y;
        text->internal->needs_layout_update = true;
    }
    return true;
}

bool TTF_GetTextPosition(TTF_Text *text, int *x, int *y)
{
    if (x) {
        *x = 0;
    }
    if (y) {
        *y = 0;
    }

    TTF_CHECK_POINTER("text", text, false);

    if (x) {
        *x = text->internal->x;
    }
    if (y) {
        *y = text->internal->y;
    }
    return true;
}

bool TTF_SetTextWrapWidth(TTF_Text *text, int wrap_width)
{
    TTF_CHECK_POINTER("text", text, false);

    if (wrap_width == text->internal->layout->wrap_length) {
        return true;
    }

    text->internal->layout->wrap_length = SDL_max(wrap_width, 0);
    text->internal->needs_layout_update = true;
    return true;
}

bool TTF_GetTextWrapWidth(TTF_Text *text, int *wrap_width)
{
    if (wrap_width) {
        *wrap_width = 0;
    }

    TTF_CHECK_POINTER("text", text, false);

    if (wrap_width) {
        *wrap_width = text->internal->layout->wrap_length;
    }
    return true;
}

bool TTF_SetTextWrapWhitespaceVisible(TTF_Text *text, bool visible)
{
    TTF_CHECK_POINTER("text", text, false);

    if (visible == text->internal->layout->wrap_whitespace_visible) {
        return true;
    }

    text->internal->layout->wrap_whitespace_visible = visible;
    text->internal->needs_layout_update = true;
    return true;
}

bool TTF_TextWrapWhitespaceVisible(TTF_Text *text)
{
    TTF_CHECK_POINTER("text", text, false);

    return text->internal->layout->wrap_whitespace_visible;
}

bool TTF_SetTextString(TTF_Text *text, const char *string, size_t length)
{
    TTF_CHECK_POINTER("text", text, false);

    if (!string || !*string) {
        if (!text->text) {
            return true;
        }

        SDL_free(text->text);
        text->text = NULL;
    } else {
        if (!length) {
            length = SDL_strlen(string);
        }

        if (text->text && length == SDL_strlen(text->text) && SDL_memcmp(text, text->text, length) == 0) {
            return true;
        }

        char *new_string = SDL_malloc(length + 1);
        if (!new_string) {
            return false;
        }
        SDL_memcpy(new_string, string, length);
        new_string[length] = '\0';

        SDL_free(text->text);
        text->text = new_string;
    }

    text->internal->needs_layout_update = true;
    return true;
}

bool TTF_InsertTextString(TTF_Text *text, int offset, const char *string, size_t length)
{
    TTF_CHECK_POINTER("text", text, false);

    if (!string || !*string) {
        return true;
    }

    if (!length) {
        length = SDL_strlen(string);
    }

    if (!text->text) {
        return TTF_SetTextString(text, string, length);
    }

    int old_length = (int)SDL_strlen(text->text);
    size_t new_length = old_length + length;
    char *new_string = (char *)SDL_realloc(text->text, new_length + 1);
    if (!new_string) {
        return false;
    }

    if (offset < 0) {
        offset = old_length + 1 + offset;
        if (offset < 0) {
            offset = 0;
        }
    } else {
        if (offset > old_length) {
            offset = old_length;
        }
    }

    int shift = (old_length - offset);
    if (shift > 0) {
        SDL_memmove(new_string + offset + length, new_string + offset, shift);
    }
    SDL_memcpy(new_string + offset, string, length);
    new_string[new_length] = '\0';

    text->text = new_string;

    text->internal->needs_layout_update = true;
    return true;
}

bool TTF_AppendTextString(TTF_Text *text, const char *string, size_t length)
{
    return TTF_InsertTextString(text, -1, string, length);
}

bool TTF_DeleteTextString(TTF_Text *text, int offset, int length)
{
    TTF_CHECK_POINTER("text", text, false);

    if (!length || !text->text) {
        return true;
    }

    int old_length = (int)SDL_strlen(text->text);
    if (offset < 0) {
        offset = old_length + 1 + offset;
        if (offset < 0) {
            offset = 0;
        }
    } else {
        if (offset >= old_length) {
            return true;
        }
    }

    if (length < 0 || length >= (old_length - offset)) {
        if (offset == 0) {
            return TTF_SetTextString(text, NULL, 0);
        }
        text->text[offset] = '\0';
    } else {
        int shift = (old_length - length - offset);
        SDL_memcpy(&text->text[offset], &text->text[offset + length], shift);
        text->text[offset + shift] = '\0';
    }

    text->internal->needs_layout_update = true;
    return true;
}

bool TTF_GetTextSize(TTF_Text *text, int *w, int *h)
{
    if (w) {
        *w = 0;
    }
    if (h) {
        *h = 0;
    }

    TTF_CHECK_POINTER("text", text, false);

    if (!TTF_UpdateText(text)) {
        return false;
    }

    if (w) {
        *w = text->internal->w;
    }
    if (h) {
        *h = text->internal->h;
    }
    return true;
}

bool TTF_GetTextSubString(TTF_Text *text, int offset, TTF_SubString *substring)
{
    if (substring) {
        SDL_zerop(substring);
    }

    TTF_CHECK_POINTER("text", text, false);
    TTF_CHECK_POINTER("substring", substring, false);

    if (!TTF_UpdateText(text)) {
        return false;
    }

    if (text->internal->num_clusters == 0) {
        substring->rect.h = text->internal->font->height;
        return true;
    }

    int num_clusters = text->internal->num_clusters;
    TTF_SubString *clusters = text->internal->clusters;

    if (offset < 0) {
        SDL_copyp(substring, &clusters[0]);
        substring->length = 0;
        substring->rect.w = 0;
        return true;
    }

    int length = (int)SDL_strlen(text->text);
    if (offset >= length) {
        SDL_copyp(substring, &clusters[num_clusters - 1]);
        return true;
    }

    // Make a quick guess that works for ASCII text
    const TTF_SubString *cluster = &clusters[offset];
    if (offset < num_clusters && cluster->offset == offset) {
        if ((cluster->flags & TTF_SUBSTRING_LINE_END) && cluster->length == 0 &&
            offset < (num_clusters - 1)) {
            ++cluster;
        }
        SDL_copyp(substring, cluster);
        return true;
    }

    // Do a binary search to find the cluster
    int low = 0;
    int high = num_clusters - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        cluster = &clusters[mid];

        // If we're a zero length line ending, check the next cluster
        if ((cluster->flags & TTF_SUBSTRING_LINE_END) && cluster->length == 0 &&
            mid < (num_clusters - 1)) {
            ++cluster;
        }
        if (offset >= cluster->offset && offset < (cluster->offset + cluster->length)) {
            SDL_copyp(substring, &clusters[mid]);
            break;
        }

        if (cluster->offset < offset) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return true;
}

bool TTF_GetTextSubStringForLine(TTF_Text *text, int line, TTF_SubString *substring)
{
    if (substring) {
        SDL_zerop(substring);
    }

    TTF_CHECK_POINTER("text", text, false);
    TTF_CHECK_POINTER("substring", substring, false);

    if (!TTF_UpdateText(text)) {
        return false;
    }

    if (text->internal->num_clusters == 0) {
        substring->rect.h = text->internal->font->height;
        return true;
    }

    int num_clusters = text->internal->num_clusters;
    TTF_SubString *clusters = text->internal->clusters;

    if (line < 0) {
        SDL_copyp(substring, &clusters[0]);
        substring->length = 0;
        substring->rect.w = 0;
        return true;
    }

    if (line >= text->num_lines) {
        SDL_copyp(substring, &clusters[num_clusters - 1]);
        return true;
    }

    int *lines = text->internal->layout->lines;
    if (line == 0) {
        SDL_copyp(substring, &clusters[0]);
    } else {
        SDL_copyp(substring, &clusters[lines[line - 1]]);
    }
    if (line == text->num_lines - 1) {
        substring->length = (int)SDL_strlen(text->text) - substring->offset;
    } else {
        substring->length = clusters[lines[line]].offset - substring->offset;
    }
    for (int i = substring->cluster_index + 1; i < num_clusters; ++i) {
        TTF_SubString *cluster = &clusters[i];
        if (cluster->line_index != line) {
            break;
        }
        substring->flags |= cluster->flags;
        SDL_GetRectUnion(&substring->rect, &cluster->rect, &substring->rect);
    }
    return true;
}

TTF_SubString **TTF_GetTextSubStringsForRange(TTF_Text *text, int offset, int length, int *count)
{
    if (count) {
        *count = 0;
    }

    TTF_CHECK_POINTER("text", text, NULL);

    if (!TTF_UpdateText(text)) {
        return NULL;
    }

    if (text->internal->num_clusters == 0) {
        TTF_SubString **result = (TTF_SubString **)SDL_malloc(2 * sizeof(*result) + 1 * sizeof(**result));
        if (!result) {
            return NULL;
        }

        TTF_SubString *substring = (TTF_SubString *)(result + 2);
        result[0] = substring;
        result[1] = NULL;
        SDL_zerop(substring);
        substring->rect.h = text->internal->font->height;

        if (count) {
            *count = 1;
        }
        return result;
    }

    if (length < 0) {
        length = (int)SDL_strlen(text->text);
    }

    TTF_SubString substring1, substring2;
    int offset1 = offset;
    int offset2 = offset + length;
    if (!TTF_GetTextSubString(text, offset1, &substring1) ||
        !TTF_GetTextSubString(text, offset2, &substring2) ||
        !TTF_GetPreviousTextSubString(text, &substring2, &substring2)) {
        return NULL;
    }

    if (substring1.cluster_index == substring2.cluster_index) {
        TTF_SubString **result = (TTF_SubString **)SDL_malloc(2 * sizeof(*result) + 1 * sizeof(**result));
        if (!result) {
            return NULL;
        }

        TTF_SubString *substring = (TTF_SubString *)(result + 2);
        result[0] = substring;
        result[1] = NULL;
        SDL_copyp(substring, &substring1);
        if (length == 0) {
            substring->length = 0;
            if (TTF_GetFontDirection(text->internal->font) != TTF_DIRECTION_RTL) {
                substring->rect.x += substring->rect.w;
            }
            substring->rect.w = 0;
        }

        if (count) {
            *count = 1;
        }
        return result;
    }

    // Build a list of contiguous substrings
    TTF_SubString *clusters = text->internal->clusters;
    int num_results = 1;
    TTF_SubString *last = &clusters[substring1.cluster_index];
    for (int i = substring1.cluster_index + 1; i <= substring2.cluster_index; ++i) {
        TTF_SubString *cluster = &clusters[i];
        if (cluster->line_index != last->line_index) {
            ++num_results;
            last = cluster;
        }
    }

    TTF_SubString **result = (TTF_SubString **)SDL_malloc((num_results + 1) * sizeof(*result) + num_results * sizeof(**result));
    if (!result) {
        return NULL;
    }

    TTF_SubString *substrings = (TTF_SubString *)(result + num_results + 1);
    for (int i = 0; i < num_results; ++i) {
        result[i] = &substrings[i];
    }
    result[num_results] = NULL;

    TTF_SubString *substring = substrings;
    SDL_copyp(substring, &substring1);
    for (int i = substring1.cluster_index + 1; i <= substring2.cluster_index; ++i) {
        TTF_SubString *cluster = &clusters[i];
        if (cluster->line_index == substring->line_index) {
            substring->flags |= cluster->flags;
            SDL_GetRectUnion(&substring->rect, &cluster->rect, &substring->rect);
        } else {
            substring->length = (cluster->offset - substring->offset);
            ++substring;
            SDL_copyp(substring, cluster);
        }
    }
    substring->length = (substring2.offset - substring->offset) + substring2.length;

    if (count) {
        *count = num_results;
    }
    return result;
}

bool TTF_GetTextSubStringForPoint(TTF_Text *text, int x, int y, TTF_SubString *substring)
{
    if (substring) {
        SDL_zerop(substring);
    }

    TTF_CHECK_POINTER("text", text, false);
    TTF_CHECK_POINTER("substring", substring, false);

    if (!TTF_UpdateText(text)) {
        return false;
    }

    if (text->internal->num_clusters == 0) {
        substring->rect.h = text->internal->font->height;
        return true;
    }

#if TTF_USE_HARFBUZZ
    hb_direction_t hb_direction = text->internal->font->hb_direction;
    bool prefer_row = (hb_direction == HB_DIRECTION_LTR || hb_direction == HB_DIRECTION_RTL);
    bool line_ends_right = (hb_direction == HB_DIRECTION_LTR);
#else
    bool prefer_row = true;
    bool line_ends_right = true;
#endif
    const TTF_SubString *closest = NULL;
    int closest_dist = INT_MAX;
    int wrap_cost = 100;
    SDL_Point point = { x, y };
    for (int i = 0; i < text->internal->num_clusters; ++i) {
        const TTF_SubString *cluster = &text->internal->clusters[i];
        int center_x = (cluster->rect.x + cluster->rect.w / 2);
        int center_y = (cluster->rect.y + cluster->rect.h / 2);
        int dist;

        if (cluster->flags & TTF_SUBSTRING_LINE_END) {
            if (prefer_row && (cluster->flags & TTF_SUBSTRING_LINE_END)) {
                if ((y >= cluster->rect.y && y < (cluster->rect.y + cluster->rect.h)) &&
                    ((line_ends_right && x >= cluster->rect.x) ||
                     (!line_ends_right && x <= cluster->rect.x))) {
                    closest = cluster;
                    break;
                }
            }
        } else {
            if (prefer_row && (cluster->flags & TTF_SUBSTRING_LINE_START)) {
                if ((y >= cluster->rect.y && y < (cluster->rect.y + cluster->rect.h)) &&
                    ((line_ends_right && x < cluster->rect.x) ||
                     (!line_ends_right && x > cluster->rect.x))) {
                    closest = cluster;
                    break;
                }
            }

            if (SDL_PointInRect(&point, &cluster->rect)) {
                closest = cluster;
                break;
            }
        }
        if (prefer_row) {
            dist = SDL_abs(center_y - y) * wrap_cost + SDL_abs(center_x - x);
        } else {
            dist = SDL_abs(center_x - x) * wrap_cost + SDL_abs(center_y - y);
        }
        if (dist < closest_dist) {
            closest = cluster;
            closest_dist = dist;
        }
    }

    if (closest) {
        SDL_copyp(substring, closest);
    }
    return true;
}

bool TTF_GetPreviousTextSubString(TTF_Text *text, const TTF_SubString *substring, TTF_SubString *previous)
{
    if (previous && previous != substring) {
        SDL_zerop(previous);
    }

    TTF_CHECK_POINTER("text", text, false);
    TTF_CHECK_POINTER("substring", substring, false);
    TTF_CHECK_POINTER("previous", previous, false);

    int num_clusters = text->internal->num_clusters;
    const TTF_SubString *clusters = text->internal->clusters;
    if (substring->cluster_index < 0 || substring->cluster_index >= num_clusters) {
        return SDL_SetError("Cluster index out of range");
    }
    if (substring->offset != clusters[substring->cluster_index].offset) {
        return SDL_SetError("Stale substring");
    }
    if (substring->cluster_index == 0) {
        SDL_copyp(previous, &clusters[0]);
        previous->length = 0;
        previous->rect.w = 0;
    } else {
        SDL_copyp(previous, &clusters[substring->cluster_index - 1]);
    }
    return true;
}

bool TTF_GetNextTextSubString(TTF_Text *text, const TTF_SubString *substring, TTF_SubString *next)
{
    if (next && next != substring) {
        SDL_zerop(next);
    }

    TTF_CHECK_POINTER("text", text, false);
    TTF_CHECK_POINTER("substring", substring, false);
    TTF_CHECK_POINTER("next", next, false);

    int num_clusters = text->internal->num_clusters;
    const TTF_SubString *clusters = text->internal->clusters;
    if (substring->cluster_index < 0 || substring->cluster_index >= num_clusters) {
        return SDL_SetError("Cluster index out of range");
    }
    if (substring->offset != clusters[substring->cluster_index].offset) {
        return SDL_SetError("Stale substring");
    }
    if (substring->cluster_index == (num_clusters - 1)) {
        SDL_copyp(next, &clusters[num_clusters - 1]);
    } else {
        SDL_copyp(next, &clusters[substring->cluster_index + 1]);
    }
    return true;
}

bool TTF_UpdateText(TTF_Text *text)
{
    if (text->internal->needs_layout_update) {
        DestroyEngineText(text);
        text->internal->needs_engine_update = true;

        if (text->internal->ops) {
            SDL_free(text->internal->ops);
            text->internal->ops = NULL;
            text->internal->num_ops = 0;
        }
        if (text->internal->clusters) {
            SDL_free(text->internal->clusters);
            text->internal->clusters = NULL;
            text->internal->num_clusters = 0;
        }
        if (text->internal->layout->lines) {
            SDL_free(text->internal->layout->lines);
            text->internal->layout->lines = NULL;
        }
        text->num_lines = 0;
        text->internal->w = 0;
        text->internal->h = 0;

        if (text->internal->font && text->text) {
            if (!LayoutText(text)) {
                return false;
            }
        }

        text->internal->needs_layout_update = false;
    }

    if (text->internal->needs_engine_update) {
        if (!CreateEngineText(text)) {
            return false;
        }

        text->internal->needs_engine_update = false;
    }

    return true;
}

void TTF_DestroyText(TTF_Text *text)
{
    if (!text) {
        return;
    }

    DestroyEngineText(text);

    if (text->internal->ops) {
        SDL_free(text->internal->ops);
    }
    if (text->internal->clusters) {
        SDL_free(text->internal->clusters);
    }
    if (text->internal->layout->lines) {
        SDL_free(text->internal->layout->lines);
    }

    TTF_SetTextFont(text, NULL);
    SDL_DestroyProperties(text->internal->props);
    SDL_free(text->text);
    SDL_free(text);
}

bool TTF_SetFontSize(TTF_Font *font, float ptsize)
{
    return TTF_SetFontSizeDPI(font, ptsize, 0, 0);
}

bool TTF_SetFontSizeDPI(TTF_Font *font, float ptsize, int hdpi, int vdpi)
{
    TTF_CHECK_FONT(font, false);

    if (ptsize <= 0.0f) {
        return SDL_InvalidParamError("ptsize");
    }

    if (hdpi <= 0 && vdpi <= 0) {
        hdpi = font->hdpi;
        vdpi = font->vdpi;
    } else if (hdpi <= 0) {
        hdpi = vdpi;
    } else if (vdpi <= 0) {
        vdpi = hdpi;
    }

    if (ptsize == font->ptsize && hdpi == font->hdpi && vdpi == font->vdpi) {
        return true;
    }

    FT_Face face = font->face;
    FT_Error error;

    // Make sure that our font face is scalable (global metrics)
    if (FT_IS_SCALABLE(face)) {
        /* Set the character size using the provided DPI.  If a zero DPI
         * is provided, then the other DPI setting will be used.  If both
         * are zero, then Freetype's default 72 DPI will be used.  */
        error = FT_Set_Char_Size(face, 0, (int)SDL_roundf(ptsize * 64), (FT_UInt)hdpi, (FT_UInt)vdpi);
        if (error) {
            return TTF_SetFTError("Couldn't set font size", error);
        }
    } else {
        /* Non-scalable font case.  ptsize determines which family
         * or series of fonts to grab from the non-scalable format.
         * It is not the point size of the font.  */
        if (face->num_fixed_sizes <= 0) {
            return SDL_SetError("Couldn't select size : no num_fixed_sizes");
        }

        // within [0; num_fixed_sizes - 1]
        int index = (int)ptsize;
        index = SDL_max(index, 0);
        index = SDL_min(index, face->num_fixed_sizes - 1);

        error = FT_Select_Size(face, index);
        if (error) {
            return TTF_SetFTError("Couldn't select size", error);
        }
    }

    TTF_InitFontMetrics(font);

    font->ptsize = ptsize;
    font->hdpi = hdpi;
    font->vdpi = vdpi;

    Flush_Cache(font);
    UpdateFontText(font);

#if TTF_USE_HARFBUZZ
    // Call when size or variations settings on underlying FT_Face change.
    hb_ft_font_changed(font->hb_font);
#endif

    return true;
}

float TTF_GetFontSize(TTF_Font *font)
{
    TTF_CHECK_FONT(font, 0.0f);

    return font->ptsize;
}

bool TTF_GetFontDPI(TTF_Font *font, int *hdpi, int *vdpi)
{
    if (hdpi) {
        *hdpi = 0;
    }
    if (vdpi) {
        *vdpi = 0;
    }

    TTF_CHECK_FONT(font, false);

    if (hdpi) {
        *hdpi = font->hdpi;
    }
    if (vdpi) {
        *vdpi = font->vdpi;
    }
    return true;
}

void TTF_SetFontStyle(TTF_Font *font, int style)
{
    int prev_style;
    long face_style;

    TTF_CHECK_FONT(font,);

    prev_style = font->style;
    face_style = font->face->style_flags;

    // Don't add a style if already in the font, SDL_ttf doesn't need to handle them
    if (face_style & FT_STYLE_FLAG_BOLD) {
        style &= ~TTF_STYLE_BOLD;
    }
    if (face_style & FT_STYLE_FLAG_ITALIC) {
        style &= ~TTF_STYLE_ITALIC;
    }

    if (font->style == style) {
        return;
    }

    font->style = style;

    TTF_InitFontMetrics(font);

    /* Flush the cache if styles that impact glyph drawing have changed */
    if ((font->style | TTF_STYLE_NO_GLYPH_CHANGE) != (prev_style | TTF_STYLE_NO_GLYPH_CHANGE)) {
        Flush_Cache(font);
    }
    UpdateFontText(font);
}

int TTF_GetFontStyle(const TTF_Font *font)
{
    int style;
    long face_style;

    TTF_CHECK_FONT(font, -1);

    style = font->style;
    face_style = font->face->style_flags;

    // Add the style already in the font
    if (face_style & FT_STYLE_FLAG_BOLD) {
        style |= TTF_STYLE_BOLD;
    }
    if (face_style & FT_STYLE_FLAG_ITALIC) {
        style |= TTF_STYLE_ITALIC;
    }

    return style;
}

bool TTF_SetFontOutline(TTF_Font *font, int outline)
{
    TTF_CHECK_FONT(font, false);

    outline = SDL_max(0, outline);

    if (outline == font->outline) {
        return true;
    }

    if (outline > 0) {
        if (!font->stroker) {
            FT_Error error;

            SDL_LockMutex(TTF_state.lock);
            error = FT_Stroker_New(TTF_state.library, &font->stroker);
            SDL_UnlockMutex(TTF_state.lock);
            if (error) {
                return TTF_SetFTError("Couldn't create font stroker", error);
            }
        }

        FT_Stroker_Set(font->stroker, outline * 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
    } else {
        if (font->stroker) {
            FT_Stroker_Done(font->stroker);
            font->stroker = NULL;
        }
    }

    font->outline = outline;

    TTF_InitFontMetrics(font);
    Flush_Cache(font);
    UpdateFontText(font);

    return true;
}

int TTF_GetFontOutline(const TTF_Font *font)
{
    TTF_CHECK_FONT(font, -1);

    return font->outline;
}

void TTF_SetFontHinting(TTF_Font *font, int hinting)
{
    TTF_CHECK_FONT(font,);

    int ft_load_target;
    if (hinting == TTF_HINTING_LIGHT || hinting == TTF_HINTING_LIGHT_SUBPIXEL) {
        ft_load_target = FT_LOAD_TARGET_LIGHT;
    } else if (hinting == TTF_HINTING_MONO) {
        ft_load_target = FT_LOAD_TARGET_MONO;
    } else if (hinting == TTF_HINTING_NONE) {
        ft_load_target = FT_LOAD_NO_HINTING;
    } else {
        ft_load_target = FT_LOAD_TARGET_NORMAL;
    }

    int render_subpixel = (hinting == TTF_HINTING_LIGHT_SUBPIXEL) ? 1 : 0;

    if (ft_load_target == font->ft_load_target && render_subpixel == font->render_subpixel) {
        return;
    }

    font->ft_load_target = ft_load_target;

#if TTF_USE_HARFBUZZ
    // update flag for HB
    hb_ft_font_set_load_flags(font->hb_font, FT_LOAD_DEFAULT | font->ft_load_target);
#endif

    Flush_Cache(font);
    UpdateFontText(font);
}

int TTF_GetFontHinting(const TTF_Font *font)
{
    TTF_CHECK_FONT(font, -1);

    if (font->ft_load_target == FT_LOAD_TARGET_LIGHT) {
        if (font->render_subpixel == 0) {
            return TTF_HINTING_LIGHT;
        } else {
            return TTF_HINTING_LIGHT_SUBPIXEL;
        }
    } else if (font->ft_load_target == FT_LOAD_TARGET_MONO) {
        return TTF_HINTING_MONO;
    } else if (font->ft_load_target == FT_LOAD_NO_HINTING) {
        return TTF_HINTING_NONE;
    }
    return TTF_HINTING_NORMAL;
}

bool TTF_SetFontSDF(TTF_Font *font, bool enabled)
{
    TTF_CHECK_FONT(font, false);
#if TTF_USE_SDF
    font->render_sdf = enabled;
    Flush_Cache(font);
    UpdateFontText(font);
    return true;
#else
    (void)enabled;
    return SDL_SetError("SDL_ttf compiled without SDF support");
#endif
}

bool TTF_GetFontSDF(const TTF_Font *font)
{
    TTF_CHECK_FONT(font, false);

    return font->render_sdf;
}

void TTF_SetFontWrapAlignment(TTF_Font *font, TTF_HorizontalAlignment align)
{
    TTF_CHECK_FONT(font,);

    if (align == font->horizontal_align) {
        return;
    }

    switch (align) {
    case TTF_HORIZONTAL_ALIGN_LEFT:
    case TTF_HORIZONTAL_ALIGN_CENTER:
    case TTF_HORIZONTAL_ALIGN_RIGHT:
        font->horizontal_align = align;
        break;
    default:
        // Ignore invalid values
        break;
    }
    UpdateFontText(font);
}

TTF_HorizontalAlignment TTF_GetFontWrapAlignment(const TTF_Font *font)
{
    TTF_CHECK_FONT(font, TTF_HORIZONTAL_ALIGN_INVALID);

    return font->horizontal_align;
}

int TTF_GetFontHeight(const TTF_Font *font)
{
    TTF_CHECK_FONT(font, 0);

    return font->height;
}

int TTF_GetFontAscent(const TTF_Font *font)
{
    TTF_CHECK_FONT(font, 0);

    return font->ascent + 2 * font->outline;
}

int TTF_GetFontDescent(const TTF_Font *font)
{
    TTF_CHECK_FONT(font, 0);

    return font->descent;
}

void TTF_SetFontLineSkip(TTF_Font *font, int lineskip)
{
    TTF_CHECK_FONT(font,);

    if (lineskip == font->lineskip) {
        return;
    }

    font->lineskip = lineskip;
    UpdateFontText(font);
}

int TTF_GetFontLineSkip(const TTF_Font *font)
{
    TTF_CHECK_FONT(font, 0);

    return font->lineskip;
}

void TTF_SetFontKerning(TTF_Font *font, bool enabled)
{
    TTF_CHECK_FONT(font,);

    if (enabled == font->enable_kerning) {
        return;
    }

    font->enable_kerning = enabled;
#if TTF_USE_HARFBUZZ
    // Harfbuzz can do kerning positioning even if the font hasn't the data
#else
    font->use_kerning   = enabled && FT_HAS_KERNING(font->face);
#endif
    UpdateFontText(font);
}

bool TTF_GetFontKerning(const TTF_Font *font)
{
    TTF_CHECK_FONT(font, false);

    return font->enable_kerning;
}

int TTF_GetNumFontFaces(const TTF_Font *font)
{
    TTF_CHECK_FONT(font, 0);

    return (int)font->face->num_faces;
}

bool TTF_FontIsFixedWidth(const TTF_Font *font)
{
    TTF_CHECK_FONT(font, false);

    return FT_IS_FIXED_WIDTH(font->face);
}

bool TTF_FontIsScalable(const TTF_Font *font)
{
    TTF_CHECK_FONT(font, false);

    return FT_IS_SCALABLE(font->face);
}

const char *TTF_GetFontFamilyName(const TTF_Font *font)
{
    TTF_CHECK_FONT(font, NULL);

    return font->face->family_name;
}

const char *TTF_GetFontStyleName(const TTF_Font *font)
{
    TTF_CHECK_FONT(font, NULL);

    return font->face->style_name;
}

bool TTF_SetFontDirection(TTF_Font *font, TTF_Direction direction)
{
    TTF_CHECK_FONT(font, false);

#if TTF_USE_HARFBUZZ
    hb_direction_t hb_direction;
    if (direction == TTF_DIRECTION_LTR) {
        hb_direction = HB_DIRECTION_LTR;
    } else if (direction == TTF_DIRECTION_RTL) {
        hb_direction = HB_DIRECTION_RTL;
    } else if (direction == TTF_DIRECTION_TTB) {
        hb_direction = HB_DIRECTION_BTT;
    } else if (direction == TTF_DIRECTION_BTT) {
        hb_direction = HB_DIRECTION_TTB;
    } else {
        return SDL_InvalidParamError("direction");
    }

    if (hb_direction == font->hb_direction) {
        return true;
    }

    font->hb_direction = hb_direction;
    UpdateFontText(font);
    return true;
#else
    (void) direction;
    return SDL_Unsupported();
#endif
}

TTF_Direction TTF_GetFontDirection(TTF_Font *font)
{
    TTF_CHECK_FONT(font, TTF_DIRECTION_LTR);

#if TTF_USE_HARFBUZZ
    switch (font->hb_direction) {
    case HB_DIRECTION_LTR:
        return TTF_DIRECTION_LTR;
    case HB_DIRECTION_RTL:
        return TTF_DIRECTION_RTL;
    case HB_DIRECTION_BTT:
        return TTF_DIRECTION_TTB;
    case HB_DIRECTION_TTB:
        return TTF_DIRECTION_BTT;
    default:
        return TTF_DIRECTION_LTR;
    }
#else
    return TTF_DIRECTION_LTR;
#endif
}

bool TTF_SetFontScript(TTF_Font *font, const char *script)
{
    TTF_CHECK_FONT(font, false);

#if TTF_USE_HARFBUZZ
    Uint8 a, b, c, d;
    hb_script_t hb_script;

    if (script == NULL || SDL_strlen(script) != 4) {
        return SDL_InvalidParamError("script");
    }

    a = script[0];
    b = script[1];
    c = script[2];
    d = script[3];

    hb_script = HB_TAG(a, b, c, d);

    if (hb_script == font->hb_script) {
        return true;
    }

    font->hb_script = hb_script;
    UpdateFontText(font);
    return true;
#else
    (void) script;
    return SDL_Unsupported();
#endif
}

bool TTF_GetGlyphScript(Uint32 ch, char *script, size_t script_size)
{
#if TTF_USE_HARFBUZZ
    TTF_CHECK_POINTER("script", script, false);

    if (script_size < 5) {
        return SDL_SetError("Insufficient script buffer size");
    }

    hb_buffer_t *hb_buffer = hb_buffer_create();

    if (hb_buffer == NULL) {
        return SDL_SetError("Cannot create harfbuzz buffer");
    }

    hb_unicode_funcs_t* hb_unicode_functions = hb_buffer_get_unicode_funcs(hb_buffer);

    if (hb_unicode_functions == NULL) {
        hb_buffer_destroy(hb_buffer);
        return SDL_SetError("Cannot get harfbuzz unicode functions");
    }

    hb_buffer_clear_contents(hb_buffer);
    hb_buffer_set_content_type(hb_buffer, HB_BUFFER_CONTENT_TYPE_UNICODE);

    uint8_t untagged_script[4] = { HB_UNTAG(hb_unicode_script(hb_unicode_functions, ch)) };
    script[0] = (char)untagged_script[0];
    script[1] = (char)untagged_script[1];
    script[2] = (char)untagged_script[2];
    script[3] = (char)untagged_script[3];
    script[4] = '\0';

    hb_buffer_destroy(hb_buffer);
    return true;

#else
    (void) script;
    (void) script_size;
    (void) ch;
    return SDL_Unsupported();
#endif
}

bool TTF_SetFontLanguage(TTF_Font *font, const char *language_bcp47)
{
    TTF_CHECK_FONT(font, false);

#if TTF_USE_HARFBUZZ
    hb_language_t hb_language;
    if (language_bcp47 == NULL) {
        hb_language = hb_language_from_string("", -1);
    } else {
        hb_language = hb_language_from_string(language_bcp47, -1);
    }

    if (hb_language == font->hb_language) {
        return true;
    }

    font->hb_language = hb_language;
    UpdateFontText(font);
    return true;
#else
    (void) language_bcp47;
    return SDL_Unsupported();
#endif
}

void TTF_CloseFont(TTF_Font *font)
{
    if (!font) {
        return;
    }

    if (font->text) {
        while (!SDL_HashTableEmpty(font->text)) {
            TTF_Text *text = NULL;
            void *iter = NULL;
            if (!SDL_IterateHashTable(font->text, (const void **)&text, NULL, &iter)) {
                break;
            }
            if (text && text->internal->font == font) {
                TTF_SetTextFont(text, NULL);
            }
            SDL_RemoveFromHashTable(font->text, text);
        }
        SDL_DestroyHashTable(font->text);
        font->text = NULL;
    }
    Flush_Cache(font);

#if TTF_USE_HARFBUZZ
    hb_font_destroy(font->hb_font);
#endif
    if (font->props) {
        SDL_DestroyProperties(font->props);
    }
    if (font->face) {
        FT_Done_Face(font->face);
    }
    if (font->stroker) {
        FT_Stroker_Done(font->stroker);
    }
    if (font->args.stream) {
        SDL_free(font->args.stream);
    }
    if (font->closeio) {
        SDL_CloseIO(font->src);
    }
    if (font->pos_buf) {
        SDL_free(font->pos_buf);
    }
    SDL_free(font);
}

void TTF_Quit(void)
{
    if (!SDL_ShouldQuit(&TTF_state.init)) {
        return;
    }

    if (!SDL_AtomicDecRef(&TTF_state.refcount)) {
        SDL_SetInitialized(&TTF_state.init, true);
        return;
    }

    if (TTF_state.library) {
        FT_Done_FreeType(TTF_state.library);
        TTF_state.library = NULL;
    }

    if (TTF_state.lock) {
        SDL_DestroyMutex(TTF_state.lock);
        TTF_state.lock = NULL;
    }

    SDL_SetInitialized(&TTF_state.init, false);
}

int TTF_WasInit(void)
{
    return SDL_GetAtomicInt(&TTF_state.refcount);
}

