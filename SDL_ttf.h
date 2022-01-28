/*
  SDL_ttf:  A companion library to SDL for working with TrueType (tm) fonts
  Copyright (C) 2001-2022 Sam Lantinga <slouken@libsdl.org>

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

/* This library is a wrapper around the excellent FreeType 2.0 library,
   available at:
    http://www.freetype.org/
*/

/* Note: In many places, SDL_ttf will say "glyph" when it means "code point."
   Unicode is hard, we learn as we go, and we apologize for adding to the
   confusion. */

#ifndef SDL_TTF_H_
#define SDL_TTF_H_

#include "SDL.h"
#include "begin_code.h"

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* Printable format: "%d.%d.%d", MAJOR, MINOR, PATCHLEVEL
*/
#define SDL_TTF_MAJOR_VERSION   2
#define SDL_TTF_MINOR_VERSION   0
#define SDL_TTF_PATCHLEVEL      18

/* This macro can be used to fill a version structure with the compile-time
 * version of the SDL_ttf library.
 */
#define SDL_TTF_VERSION(X)                          \
{                                                   \
    (X)->major = SDL_TTF_MAJOR_VERSION;             \
    (X)->minor = SDL_TTF_MINOR_VERSION;             \
    (X)->patch = SDL_TTF_PATCHLEVEL;                \
}

/* Backwards compatibility */
#define TTF_MAJOR_VERSION   SDL_TTF_MAJOR_VERSION
#define TTF_MINOR_VERSION   SDL_TTF_MINOR_VERSION
#define TTF_PATCHLEVEL      SDL_TTF_PATCHLEVEL
#define TTF_VERSION(X)      SDL_TTF_VERSION(X)

/**
 *  This is the version number macro for the current SDL_ttf version.
 */
#define SDL_TTF_COMPILEDVERSION \
    SDL_VERSIONNUM(SDL_TTF_MAJOR_VERSION, SDL_TTF_MINOR_VERSION, SDL_TTF_PATCHLEVEL)

/**
 *  This macro will evaluate to true if compiled with SDL_ttf at least X.Y.Z.
 */
#define SDL_TTF_VERSION_ATLEAST(X, Y, Z) \
    (SDL_TTF_COMPILEDVERSION >= SDL_VERSIONNUM(X, Y, Z))

/* Make sure this is defined (only available in newer SDL versions) */
#ifndef SDL_DEPRECATED
#define SDL_DEPRECATED
#endif

/* This function gets the version of the dynamically linked SDL_ttf library.
   it should NOT be used to fill a version structure, instead you should
   use the SDL_TTF_VERSION() macro.
 */
extern DECLSPEC const SDL_version * SDLCALL TTF_Linked_Version(void);

/* This function stores the version of the FreeType2 library in use.
   TTF_Init() should be called before calling this function.
 */
extern DECLSPEC void SDLCALL TTF_GetFreeTypeVersion(int *major, int *minor, int *patch);

/* This function stores the version of the HarfBuzz library in use,
   or 0 if HarfBuzz is not available.
 */
extern DECLSPEC void SDLCALL TTF_GetHarfBuzzVersion(int *major, int *minor, int *patch);

/* ZERO WIDTH NO-BREAKSPACE (Unicode byte order mark) */
#define UNICODE_BOM_NATIVE  0xFEFF
#define UNICODE_BOM_SWAPPED 0xFFFE

/* This function tells the library whether UNICODE text is generally
   byteswapped.  A UNICODE BOM character in a string will override
   this setting for the remainder of that string.
*/
extern DECLSPEC void SDLCALL TTF_ByteSwappedUNICODE(SDL_bool swapped);

/* The internal structure containing font information */
typedef struct _TTF_Font TTF_Font;

/* Initialize the TTF engine - returns 0 if successful, -1 on error */
extern DECLSPEC int SDLCALL TTF_Init(void);

/* Open a font file and create a font of the specified point size.
 * Some .fon fonts will have several sizes embedded in the file, so the
 * point size becomes the index of choosing which size.  If the value
 * is too high, the last indexed size will be the default. */
extern DECLSPEC TTF_Font * SDLCALL TTF_OpenFont(const char *file, int ptsize);
extern DECLSPEC TTF_Font * SDLCALL TTF_OpenFontIndex(const char *file, int ptsize, long index);
/* Open a font file from a SDL_RWops: 'src' must be kept alive for the lifetime of the TTF_Font.
 * 'freesrc' can be set so that TTF_CloseFont closes the RWops */
extern DECLSPEC TTF_Font * SDLCALL TTF_OpenFontRW(SDL_RWops *src, int freesrc, int ptsize);
extern DECLSPEC TTF_Font * SDLCALL TTF_OpenFontIndexRW(SDL_RWops *src, int freesrc, int ptsize, long index);

/* Opens a font using the given horizontal and vertical target resolutions (in DPI).
 * DPI scaling only applies to scalable fonts (e.g. TrueType). */
extern DECLSPEC TTF_Font * SDLCALL TTF_OpenFontDPI(const char *file, int ptsize, unsigned int hdpi, unsigned int vdpi);
extern DECLSPEC TTF_Font * SDLCALL TTF_OpenFontIndexDPI(const char *file, int ptsize, long index, unsigned int hdpi, unsigned int vdpi);
extern DECLSPEC TTF_Font * SDLCALL TTF_OpenFontDPIRW(SDL_RWops *src, int freesrc, int ptsize, unsigned int hdpi, unsigned int vdpi);
extern DECLSPEC TTF_Font * SDLCALL TTF_OpenFontIndexDPIRW(SDL_RWops *src, int freesrc, int ptsize, long index, unsigned int hdpi, unsigned int vdpi);

/* Set font size dynamically. This clears already generated glyphs, if any, from the cache. */
extern DECLSPEC int SDLCALL TTF_SetFontSize(TTF_Font *font, int ptsize);
extern DECLSPEC int SDLCALL TTF_SetFontSizeDPI(TTF_Font *font, int ptsize, unsigned int hdpi, unsigned int vdpi);

/* Set and retrieve the font style. Setting the style clears already generated glyphs, if any, from the cache. */
#define TTF_STYLE_NORMAL        0x00
#define TTF_STYLE_BOLD          0x01
#define TTF_STYLE_ITALIC        0x02
#define TTF_STYLE_UNDERLINE     0x04
#define TTF_STYLE_STRIKETHROUGH 0x08
extern DECLSPEC int SDLCALL TTF_GetFontStyle(const TTF_Font *font);
extern DECLSPEC void SDLCALL TTF_SetFontStyle(TTF_Font *font, int style);
extern DECLSPEC int SDLCALL TTF_GetFontOutline(const TTF_Font *font);
extern DECLSPEC void SDLCALL TTF_SetFontOutline(TTF_Font *font, int outline);

/* Set and retrieve FreeType hinter settings. Setting it clears already generated glyphs, if any, from the cache. */
#define TTF_HINTING_NORMAL          0
#define TTF_HINTING_LIGHT           1
#define TTF_HINTING_MONO            2
#define TTF_HINTING_NONE            3
#define TTF_HINTING_LIGHT_SUBPIXEL  4
extern DECLSPEC int SDLCALL TTF_GetFontHinting(const TTF_Font *font);
extern DECLSPEC void SDLCALL TTF_SetFontHinting(TTF_Font *font, int hinting);

/* Get the total height of the font - usually equal to point size */
extern DECLSPEC int SDLCALL TTF_FontHeight(const TTF_Font *font);

/* Get the offset from the baseline to the top of the font
   This is a positive value, relative to the baseline.
 */
extern DECLSPEC int SDLCALL TTF_FontAscent(const TTF_Font *font);

/* Get the offset from the baseline to the bottom of the font
   This is a negative value, relative to the baseline.
 */
extern DECLSPEC int SDLCALL TTF_FontDescent(const TTF_Font *font);

/* Get the recommended spacing between lines of text for this font */
extern DECLSPEC int SDLCALL TTF_FontLineSkip(const TTF_Font *font);

/* Get/Set whether or not kerning is allowed for this font */
extern DECLSPEC int SDLCALL TTF_GetFontKerning(const TTF_Font *font);
extern DECLSPEC void SDLCALL TTF_SetFontKerning(TTF_Font *font, int allowed);

/* Get the number of faces of the font */
extern DECLSPEC long SDLCALL TTF_FontFaces(const TTF_Font *font);

/* Get the font face attributes, if any */
extern DECLSPEC int SDLCALL TTF_FontFaceIsFixedWidth(const TTF_Font *font);
extern DECLSPEC char * SDLCALL TTF_FontFaceFamilyName(const TTF_Font *font);
extern DECLSPEC char * SDLCALL TTF_FontFaceStyleName(const TTF_Font *font);

/* Check wether a glyph is provided by the font or not */
extern DECLSPEC int SDLCALL TTF_GlyphIsProvided(TTF_Font *font, Uint16 ch);
extern DECLSPEC int SDLCALL TTF_GlyphIsProvided32(TTF_Font *font, Uint32 ch);

/* Get the metrics (dimensions) of a glyph
   To understand what these metrics mean, here is a useful link:
    http://freetype.sourceforge.net/freetype2/docs/tutorial/step2.html
 */
extern DECLSPEC int SDLCALL TTF_GlyphMetrics(TTF_Font *font, Uint16 ch,
                     int *minx, int *maxx,
                     int *miny, int *maxy, int *advance);
extern DECLSPEC int SDLCALL TTF_GlyphMetrics32(TTF_Font *font, Uint32 ch,
                     int *minx, int *maxx,
                     int *miny, int *maxy, int *advance);

/* Get the dimensions of a rendered string of text */
extern DECLSPEC int SDLCALL TTF_SizeText(TTF_Font *font, const char *text, int *w, int *h);
extern DECLSPEC int SDLCALL TTF_SizeUTF8(TTF_Font *font, const char *text, int *w, int *h);
extern DECLSPEC int SDLCALL TTF_SizeUNICODE(TTF_Font *font, const Uint16 *text, int *w, int *h);

/* Get the measurement string of text without rendering
   e.g. the number of characters that can be rendered before reaching 'measure_width'

   in:
   measure_width - in pixels to measure this text
   out:
   count  - number of characters that can be rendered
   extent - latest calculated width
*/
extern DECLSPEC int SDLCALL TTF_MeasureText(TTF_Font *font, const char *text, int measure_width, int *extent, int *count);
extern DECLSPEC int SDLCALL TTF_MeasureUTF8(TTF_Font *font, const char *text, int measure_width, int *extent, int *count);
extern DECLSPEC int SDLCALL TTF_MeasureUNICODE(TTF_Font *font, const Uint16 *text, int measure_width, int *extent, int *count);

/* Create an 8-bit palettized surface and render the given text at
   fast quality with the given font and color.  The 0 pixel is the
   colorkey, giving a transparent background, and the 1 pixel is set
   to the text color.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderText_Solid(TTF_Font *font,
                const char *text, SDL_Color fg);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderUTF8_Solid(TTF_Font *font,
                const char *text, SDL_Color fg);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderUNICODE_Solid(TTF_Font *font,
                const Uint16 *text, SDL_Color fg);

/* Create an 8-bit palettized surface and render the given text at
   fast quality with the given font and color.  The 0 pixel is the
   colorkey, giving a transparent background, and the 1 pixel is set
   to the text color.
   Text is wrapped to multiple lines on line endings and on word boundaries
   if it extends beyond wrapLength in pixels.
   If wrapLength is 0, only wrap on new lines.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderText_Solid_Wrapped(TTF_Font *font,
                const char *text, SDL_Color fg, Uint32 wrapLength);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderUTF8_Solid_Wrapped(TTF_Font *font,
                const char *text, SDL_Color fg, Uint32 wrapLength);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderUNICODE_Solid_Wrapped(TTF_Font *font,
                const Uint16 *text, SDL_Color fg, Uint32 wrapLength);

/* Create an 8-bit palettized surface and render the given glyph at
   fast quality with the given font and color.  The 0 pixel is the
   colorkey, giving a transparent background, and the 1 pixel is set
   to the text color.  The glyph is rendered without any padding or
   centering in the X direction, and aligned normally in the Y direction.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderGlyph_Solid(TTF_Font *font,
                    Uint16 ch, SDL_Color fg);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderGlyph32_Solid(TTF_Font *font,
                    Uint32 ch, SDL_Color fg);

/* Create an 8-bit palettized surface and render the given text at
   high quality with the given font and colors.  The 0 pixel is background,
   while other pixels have varying degrees of the foreground color.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderText_Shaded(TTF_Font *font,
                const char *text, SDL_Color fg, SDL_Color bg);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderUTF8_Shaded(TTF_Font *font,
                const char *text, SDL_Color fg, SDL_Color bg);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderUNICODE_Shaded(TTF_Font *font,
                const Uint16 *text, SDL_Color fg, SDL_Color bg);

/* Create an 8-bit palettized surface and render the given text at
   high quality with the given font and colors.  The 0 pixel is background,
   while other pixels have varying degrees of the foreground color.
   Text is wrapped to multiple lines on line endings and on word boundaries
   if it extends beyond wrapLength in pixels.
   If wrapLength is 0, only wrap on new lines.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderText_Shaded_Wrapped(TTF_Font *font,
                const char *text, SDL_Color fg, SDL_Color bg, Uint32 wrapLength);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderUTF8_Shaded_Wrapped(TTF_Font *font,
                const char *text, SDL_Color fg, SDL_Color bg, Uint32 wrapLength);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderUNICODE_Shaded_Wrapped(TTF_Font *font,
                const Uint16 *text, SDL_Color fg, SDL_Color bg, Uint32 wrapLength);

/* Create an 8-bit palettized surface and render the given glyph at
   high quality with the given font and colors.  The 0 pixel is background,
   while other pixels have varying degrees of the foreground color.
   The glyph is rendered without any padding or centering in the X
   direction, and aligned normally in the Y direction.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderGlyph_Shaded(TTF_Font *font,
                Uint16 ch, SDL_Color fg, SDL_Color bg);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderGlyph32_Shaded(TTF_Font *font,
                Uint32 ch, SDL_Color fg, SDL_Color bg);

/* Create a 32-bit ARGB surface and render the given text at high quality,
   using alpha blending to dither the font with the given color.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderText_Blended(TTF_Font *font,
                const char *text, SDL_Color fg);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderUTF8_Blended(TTF_Font *font,
                const char *text, SDL_Color fg);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderUNICODE_Blended(TTF_Font *font,
                const Uint16 *text, SDL_Color fg);


/* Create a 32-bit ARGB surface and render the given text at high quality,
   using alpha blending to dither the font with the given color.
   Text is wrapped to multiple lines on line endings and on word boundaries
   if it extends beyond wrapLength in pixels.
   If wrapLength is 0, only wrap on new lines.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderText_Blended_Wrapped(TTF_Font *font,
                const char *text, SDL_Color fg, Uint32 wrapLength);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderUTF8_Blended_Wrapped(TTF_Font *font,
                const char *text, SDL_Color fg, Uint32 wrapLength);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderUNICODE_Blended_Wrapped(TTF_Font *font,
                const Uint16 *text, SDL_Color fg, Uint32 wrapLength);

/* Create a 32-bit ARGB surface and render the given glyph at high quality,
   using alpha blending to dither the font with the given color.
   The glyph is rendered without any padding or centering in the X
   direction, and aligned normally in the Y direction.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderGlyph_Blended(TTF_Font *font,
                        Uint16 ch, SDL_Color fg);
extern DECLSPEC SDL_Surface * SDLCALL TTF_RenderGlyph32_Blended(TTF_Font *font,
                        Uint32 ch, SDL_Color fg);

/* For compatibility with previous versions, here are the old functions */
#define TTF_RenderText(font, text, fg, bg)  \
    TTF_RenderText_Shaded(font, text, fg, bg)
#define TTF_RenderUTF8(font, text, fg, bg)  \
    TTF_RenderUTF8_Shaded(font, text, fg, bg)
#define TTF_RenderUNICODE(font, text, fg, bg)   \
    TTF_RenderUNICODE_Shaded(font, text, fg, bg)

/* Set Direction and Script to be used for text shaping.
   - direction is of type hb_direction_t or ttf_direction_t
   - script is of type hb_script_t or ttf_script_t

   This functions returns always 0, or -1 if SDL_ttf is not compiled with HarfBuzz
*/
extern DECLSPEC int SDLCALL TTF_SetDirection(int direction); /* hb_direction_t or ttf_direction_t */
extern DECLSPEC int SDLCALL TTF_SetScript(int script); /* hb_script_t or ttf_script_t */

/* Close an opened font file */
extern DECLSPEC void SDLCALL TTF_CloseFont(TTF_Font *font);

/* De-initialize the TTF engine */
extern DECLSPEC void SDLCALL TTF_Quit(void);

/* Check if the TTF engine is initialized */
extern DECLSPEC int SDLCALL TTF_WasInit(void);

/* Get the kerning size of two glyphs indices */
/* DEPRECATED: this function requires FreeType font indexes, not glyphs,
   by accident, which we don't expose through this API, so it could give
   wildly incorrect results, especially with non-ASCII values.
   Going forward, please use TTF_GetFontKerningSizeGlyphs() instead, which
   does what you probably expected this function to do. */
extern DECLSPEC int TTF_GetFontKerningSize(TTF_Font *font, int prev_index, int index) SDL_DEPRECATED;

/* Get the kerning size of two glyphs */
extern DECLSPEC int TTF_GetFontKerningSizeGlyphs(TTF_Font *font, Uint16 previous_ch, Uint16 ch);
extern DECLSPEC int TTF_GetFontKerningSizeGlyphs32(TTF_Font *font, Uint32 previous_ch, Uint32 ch);

/* Enable Signed Distance Field rendering (with the Blended APIs) */
extern DECLSPEC int TTF_SetFontSDF(TTF_Font *font, SDL_bool on_off);
extern DECLSPEC SDL_bool TTF_GetFontSDF(const TTF_Font *font);

/* We'll use SDL for reporting errors */
#define TTF_SetError    SDL_SetError
#define TTF_GetError    SDL_GetError

/* Exact mapping with HarfBuzz types */
typedef enum {
  TTF_DIRECTION_INVALID = 0,
  TTF_DIRECTION_LTR = 4,
  TTF_DIRECTION_RTL
  /* TTF_DIRECTION_TTB, */
  /* TTF_DIRECTION_BTT  */
} ttf_direction_t;

#define TTF_TAG(c1,c2,c3,c4) ((Uint32)((((Uint32)(c1)&0xFF)<<24)|(((Uint32)(c2)&0xFF)<<16)|(((Uint32)(c3)&0xFF)<<8)|((Uint32)(c4)&0xFF)))
#define TTF_TAG_NONE TTF_TAG(0,0,0,0)
#define TTF_TAG_MAX TTF_TAG(0xff,0xff,0xff,0xff)
#define TTF_TAG_MAX_SIGNED TTF_TAG(0x7f,0xff,0xff,0xff)

typedef enum
{
  TTF_SCRIPT_COMMON			= TTF_TAG ('Z','y','y','y'), /*1.1*/
  TTF_SCRIPT_INHERITED			= TTF_TAG ('Z','i','n','h'), /*1.1*/
  TTF_SCRIPT_UNKNOWN			= TTF_TAG ('Z','z','z','z'), /*5.0*/

  TTF_SCRIPT_ARABIC			= TTF_TAG ('A','r','a','b'), /*1.1*/
  TTF_SCRIPT_ARMENIAN			= TTF_TAG ('A','r','m','n'), /*1.1*/
  TTF_SCRIPT_BENGALI			= TTF_TAG ('B','e','n','g'), /*1.1*/
  TTF_SCRIPT_CYRILLIC			= TTF_TAG ('C','y','r','l'), /*1.1*/
  TTF_SCRIPT_DEVANAGARI			= TTF_TAG ('D','e','v','a'), /*1.1*/
  TTF_SCRIPT_GEORGIAN			= TTF_TAG ('G','e','o','r'), /*1.1*/
  TTF_SCRIPT_GREEK			= TTF_TAG ('G','r','e','k'), /*1.1*/
  TTF_SCRIPT_GUJARATI			= TTF_TAG ('G','u','j','r'), /*1.1*/
  TTF_SCRIPT_GURMUKHI			= TTF_TAG ('G','u','r','u'), /*1.1*/
  TTF_SCRIPT_HANGUL			= TTF_TAG ('H','a','n','g'), /*1.1*/
  TTF_SCRIPT_HAN				= TTF_TAG ('H','a','n','i'), /*1.1*/
  TTF_SCRIPT_HEBREW			= TTF_TAG ('H','e','b','r'), /*1.1*/
  TTF_SCRIPT_HIRAGANA			= TTF_TAG ('H','i','r','a'), /*1.1*/
  TTF_SCRIPT_KANNADA			= TTF_TAG ('K','n','d','a'), /*1.1*/
  TTF_SCRIPT_KATAKANA			= TTF_TAG ('K','a','n','a'), /*1.1*/
  TTF_SCRIPT_LAO				= TTF_TAG ('L','a','o','o'), /*1.1*/
  TTF_SCRIPT_LATIN			= TTF_TAG ('L','a','t','n'), /*1.1*/
  TTF_SCRIPT_MALAYALAM			= TTF_TAG ('M','l','y','m'), /*1.1*/
  TTF_SCRIPT_ORIYA			= TTF_TAG ('O','r','y','a'), /*1.1*/
  TTF_SCRIPT_TAMIL			= TTF_TAG ('T','a','m','l'), /*1.1*/
  TTF_SCRIPT_TELUGU			= TTF_TAG ('T','e','l','u'), /*1.1*/
  TTF_SCRIPT_THAI			= TTF_TAG ('T','h','a','i'), /*1.1*/

  TTF_SCRIPT_TIBETAN			= TTF_TAG ('T','i','b','t'), /*2.0*/

  TTF_SCRIPT_BOPOMOFO			= TTF_TAG ('B','o','p','o'), /*3.0*/
  TTF_SCRIPT_BRAILLE			= TTF_TAG ('B','r','a','i'), /*3.0*/
  TTF_SCRIPT_CANADIAN_SYLLABICS		= TTF_TAG ('C','a','n','s'), /*3.0*/
  TTF_SCRIPT_CHEROKEE			= TTF_TAG ('C','h','e','r'), /*3.0*/
  TTF_SCRIPT_ETHIOPIC			= TTF_TAG ('E','t','h','i'), /*3.0*/
  TTF_SCRIPT_KHMER			= TTF_TAG ('K','h','m','r'), /*3.0*/
  TTF_SCRIPT_MONGOLIAN			= TTF_TAG ('M','o','n','g'), /*3.0*/
  TTF_SCRIPT_MYANMAR			= TTF_TAG ('M','y','m','r'), /*3.0*/
  TTF_SCRIPT_OGHAM			= TTF_TAG ('O','g','a','m'), /*3.0*/
  TTF_SCRIPT_RUNIC			= TTF_TAG ('R','u','n','r'), /*3.0*/
  TTF_SCRIPT_SINHALA			= TTF_TAG ('S','i','n','h'), /*3.0*/
  TTF_SCRIPT_SYRIAC			= TTF_TAG ('S','y','r','c'), /*3.0*/
  TTF_SCRIPT_THAANA			= TTF_TAG ('T','h','a','a'), /*3.0*/
  TTF_SCRIPT_YI				= TTF_TAG ('Y','i','i','i'), /*3.0*/

  TTF_SCRIPT_DESERET			= TTF_TAG ('D','s','r','t'), /*3.1*/
  TTF_SCRIPT_GOTHIC			= TTF_TAG ('G','o','t','h'), /*3.1*/
  TTF_SCRIPT_OLD_ITALIC			= TTF_TAG ('I','t','a','l'), /*3.1*/

  TTF_SCRIPT_BUHID			= TTF_TAG ('B','u','h','d'), /*3.2*/
  TTF_SCRIPT_HANUNOO			= TTF_TAG ('H','a','n','o'), /*3.2*/
  TTF_SCRIPT_TAGALOG			= TTF_TAG ('T','g','l','g'), /*3.2*/
  TTF_SCRIPT_TAGBANWA			= TTF_TAG ('T','a','g','b'), /*3.2*/

  TTF_SCRIPT_CYPRIOT			= TTF_TAG ('C','p','r','t'), /*4.0*/
  TTF_SCRIPT_LIMBU			= TTF_TAG ('L','i','m','b'), /*4.0*/
  TTF_SCRIPT_LINEAR_B			= TTF_TAG ('L','i','n','b'), /*4.0*/
  TTF_SCRIPT_OSMANYA			= TTF_TAG ('O','s','m','a'), /*4.0*/
  TTF_SCRIPT_SHAVIAN			= TTF_TAG ('S','h','a','w'), /*4.0*/
  TTF_SCRIPT_TAI_LE			= TTF_TAG ('T','a','l','e'), /*4.0*/
  TTF_SCRIPT_UGARITIC			= TTF_TAG ('U','g','a','r'), /*4.0*/

  TTF_SCRIPT_BUGINESE			= TTF_TAG ('B','u','g','i'), /*4.1*/
  TTF_SCRIPT_COPTIC			= TTF_TAG ('C','o','p','t'), /*4.1*/
  TTF_SCRIPT_GLAGOLITIC			= TTF_TAG ('G','l','a','g'), /*4.1*/
  TTF_SCRIPT_KHAROSHTHI			= TTF_TAG ('K','h','a','r'), /*4.1*/
  TTF_SCRIPT_NEW_TAI_LUE			= TTF_TAG ('T','a','l','u'), /*4.1*/
  TTF_SCRIPT_OLD_PERSIAN			= TTF_TAG ('X','p','e','o'), /*4.1*/
  TTF_SCRIPT_SYLOTI_NAGRI		= TTF_TAG ('S','y','l','o'), /*4.1*/
  TTF_SCRIPT_TIFINAGH			= TTF_TAG ('T','f','n','g'), /*4.1*/

  TTF_SCRIPT_BALINESE			= TTF_TAG ('B','a','l','i'), /*5.0*/
  TTF_SCRIPT_CUNEIFORM			= TTF_TAG ('X','s','u','x'), /*5.0*/
  TTF_SCRIPT_NKO				= TTF_TAG ('N','k','o','o'), /*5.0*/
  TTF_SCRIPT_PHAGS_PA			= TTF_TAG ('P','h','a','g'), /*5.0*/
  TTF_SCRIPT_PHOENICIAN			= TTF_TAG ('P','h','n','x'), /*5.0*/

  TTF_SCRIPT_CARIAN			= TTF_TAG ('C','a','r','i'), /*5.1*/
  TTF_SCRIPT_CHAM			= TTF_TAG ('C','h','a','m'), /*5.1*/
  TTF_SCRIPT_KAYAH_LI			= TTF_TAG ('K','a','l','i'), /*5.1*/
  TTF_SCRIPT_LEPCHA			= TTF_TAG ('L','e','p','c'), /*5.1*/
  TTF_SCRIPT_LYCIAN			= TTF_TAG ('L','y','c','i'), /*5.1*/
  TTF_SCRIPT_LYDIAN			= TTF_TAG ('L','y','d','i'), /*5.1*/
  TTF_SCRIPT_OL_CHIKI			= TTF_TAG ('O','l','c','k'), /*5.1*/
  TTF_SCRIPT_REJANG			= TTF_TAG ('R','j','n','g'), /*5.1*/
  TTF_SCRIPT_SAURASHTRA			= TTF_TAG ('S','a','u','r'), /*5.1*/
  TTF_SCRIPT_SUNDANESE			= TTF_TAG ('S','u','n','d'), /*5.1*/
  TTF_SCRIPT_VAI				= TTF_TAG ('V','a','i','i'), /*5.1*/

  TTF_SCRIPT_AVESTAN			= TTF_TAG ('A','v','s','t'), /*5.2*/
  TTF_SCRIPT_BAMUM			= TTF_TAG ('B','a','m','u'), /*5.2*/
  TTF_SCRIPT_EGYPTIAN_HIEROGLYPHS	= TTF_TAG ('E','g','y','p'), /*5.2*/
  TTF_SCRIPT_IMPERIAL_ARAMAIC		= TTF_TAG ('A','r','m','i'), /*5.2*/
  TTF_SCRIPT_INSCRIPTIONAL_PAHLAVI	= TTF_TAG ('P','h','l','i'), /*5.2*/
  TTF_SCRIPT_INSCRIPTIONAL_PARTHIAN	= TTF_TAG ('P','r','t','i'), /*5.2*/
  TTF_SCRIPT_JAVANESE			= TTF_TAG ('J','a','v','a'), /*5.2*/
  TTF_SCRIPT_KAITHI			= TTF_TAG ('K','t','h','i'), /*5.2*/
  TTF_SCRIPT_LISU			= TTF_TAG ('L','i','s','u'), /*5.2*/
  TTF_SCRIPT_MEETEI_MAYEK		= TTF_TAG ('M','t','e','i'), /*5.2*/
  TTF_SCRIPT_OLD_SOUTH_ARABIAN		= TTF_TAG ('S','a','r','b'), /*5.2*/
  TTF_SCRIPT_OLD_TURKIC			= TTF_TAG ('O','r','k','h'), /*5.2*/
  TTF_SCRIPT_SAMARITAN			= TTF_TAG ('S','a','m','r'), /*5.2*/
  TTF_SCRIPT_TAI_THAM			= TTF_TAG ('L','a','n','a'), /*5.2*/
  TTF_SCRIPT_TAI_VIET			= TTF_TAG ('T','a','v','t'), /*5.2*/

  TTF_SCRIPT_BATAK			= TTF_TAG ('B','a','t','k'), /*6.0*/
  TTF_SCRIPT_BRAHMI			= TTF_TAG ('B','r','a','h'), /*6.0*/
  TTF_SCRIPT_MANDAIC			= TTF_TAG ('M','a','n','d'), /*6.0*/

  TTF_SCRIPT_CHAKMA			= TTF_TAG ('C','a','k','m'), /*6.1*/
  TTF_SCRIPT_MEROITIC_CURSIVE		= TTF_TAG ('M','e','r','c'), /*6.1*/
  TTF_SCRIPT_MEROITIC_HIEROGLYPHS	= TTF_TAG ('M','e','r','o'), /*6.1*/
  TTF_SCRIPT_MIAO			= TTF_TAG ('P','l','r','d'), /*6.1*/
  TTF_SCRIPT_SHARADA			= TTF_TAG ('S','h','r','d'), /*6.1*/
  TTF_SCRIPT_SORA_SOMPENG		= TTF_TAG ('S','o','r','a'), /*6.1*/
  TTF_SCRIPT_TAKRI			= TTF_TAG ('T','a','k','r'), /*6.1*/

  /*
   * Since: 0.9.30
   */
  TTF_SCRIPT_BASSA_VAH			= TTF_TAG ('B','a','s','s'), /*7.0*/
  TTF_SCRIPT_CAUCASIAN_ALBANIAN		= TTF_TAG ('A','g','h','b'), /*7.0*/
  TTF_SCRIPT_DUPLOYAN			= TTF_TAG ('D','u','p','l'), /*7.0*/
  TTF_SCRIPT_ELBASAN			= TTF_TAG ('E','l','b','a'), /*7.0*/
  TTF_SCRIPT_GRANTHA			= TTF_TAG ('G','r','a','n'), /*7.0*/
  TTF_SCRIPT_KHOJKI			= TTF_TAG ('K','h','o','j'), /*7.0*/
  TTF_SCRIPT_KHUDAWADI			= TTF_TAG ('S','i','n','d'), /*7.0*/
  TTF_SCRIPT_LINEAR_A			= TTF_TAG ('L','i','n','a'), /*7.0*/
  TTF_SCRIPT_MAHAJANI			= TTF_TAG ('M','a','h','j'), /*7.0*/
  TTF_SCRIPT_MANICHAEAN			= TTF_TAG ('M','a','n','i'), /*7.0*/
  TTF_SCRIPT_MENDE_KIKAKUI		= TTF_TAG ('M','e','n','d'), /*7.0*/
  TTF_SCRIPT_MODI			= TTF_TAG ('M','o','d','i'), /*7.0*/
  TTF_SCRIPT_MRO				= TTF_TAG ('M','r','o','o'), /*7.0*/
  TTF_SCRIPT_NABATAEAN			= TTF_TAG ('N','b','a','t'), /*7.0*/
  TTF_SCRIPT_OLD_NORTH_ARABIAN		= TTF_TAG ('N','a','r','b'), /*7.0*/
  TTF_SCRIPT_OLD_PERMIC			= TTF_TAG ('P','e','r','m'), /*7.0*/
  TTF_SCRIPT_PAHAWH_HMONG		= TTF_TAG ('H','m','n','g'), /*7.0*/
  TTF_SCRIPT_PALMYRENE			= TTF_TAG ('P','a','l','m'), /*7.0*/
  TTF_SCRIPT_PAU_CIN_HAU			= TTF_TAG ('P','a','u','c'), /*7.0*/
  TTF_SCRIPT_PSALTER_PAHLAVI		= TTF_TAG ('P','h','l','p'), /*7.0*/
  TTF_SCRIPT_SIDDHAM			= TTF_TAG ('S','i','d','d'), /*7.0*/
  TTF_SCRIPT_TIRHUTA			= TTF_TAG ('T','i','r','h'), /*7.0*/
  TTF_SCRIPT_WARANG_CITI			= TTF_TAG ('W','a','r','a'), /*7.0*/

  TTF_SCRIPT_AHOM			= TTF_TAG ('A','h','o','m'), /*8.0*/
  TTF_SCRIPT_ANATOLIAN_HIEROGLYPHS	= TTF_TAG ('H','l','u','w'), /*8.0*/
  TTF_SCRIPT_HATRAN			= TTF_TAG ('H','a','t','r'), /*8.0*/
  TTF_SCRIPT_MULTANI			= TTF_TAG ('M','u','l','t'), /*8.0*/
  TTF_SCRIPT_OLD_HUNGARIAN		= TTF_TAG ('H','u','n','g'), /*8.0*/
  TTF_SCRIPT_SIGNWRITING			= TTF_TAG ('S','g','n','w'), /*8.0*/

  /*
   * Since 1.3.0
   */
  TTF_SCRIPT_ADLAM			= TTF_TAG ('A','d','l','m'), /*9.0*/
  TTF_SCRIPT_BHAIKSUKI			= TTF_TAG ('B','h','k','s'), /*9.0*/
  TTF_SCRIPT_MARCHEN			= TTF_TAG ('M','a','r','c'), /*9.0*/
  TTF_SCRIPT_OSAGE			= TTF_TAG ('O','s','g','e'), /*9.0*/
  TTF_SCRIPT_TANGUT			= TTF_TAG ('T','a','n','g'), /*9.0*/
  TTF_SCRIPT_NEWA			= TTF_TAG ('N','e','w','a'), /*9.0*/

  /*
   * Since 1.6.0
   */
  TTF_SCRIPT_MASARAM_GONDI		= TTF_TAG ('G','o','n','m'), /*10.0*/
  TTF_SCRIPT_NUSHU			= TTF_TAG ('N','s','h','u'), /*10.0*/
  TTF_SCRIPT_SOYOMBO			= TTF_TAG ('S','o','y','o'), /*10.0*/
  TTF_SCRIPT_ZANABAZAR_SQUARE		= TTF_TAG ('Z','a','n','b'), /*10.0*/

  /*
   * Since 1.8.0
   */
  TTF_SCRIPT_DOGRA			= TTF_TAG ('D','o','g','r'), /*11.0*/
  TTF_SCRIPT_GUNJALA_GONDI		= TTF_TAG ('G','o','n','g'), /*11.0*/
  TTF_SCRIPT_HANIFI_ROHINGYA		= TTF_TAG ('R','o','h','g'), /*11.0*/
  TTF_SCRIPT_MAKASAR			= TTF_TAG ('M','a','k','a'), /*11.0*/
  TTF_SCRIPT_MEDEFAIDRIN			= TTF_TAG ('M','e','d','f'), /*11.0*/
  TTF_SCRIPT_OLD_SOGDIAN			= TTF_TAG ('S','o','g','o'), /*11.0*/
  TTF_SCRIPT_SOGDIAN			= TTF_TAG ('S','o','g','d'), /*11.0*/

  /*
   * Since 2.4.0
   */
  TTF_SCRIPT_ELYMAIC			= TTF_TAG ('E','l','y','m'), /*12.0*/
  TTF_SCRIPT_NANDINAGARI			= TTF_TAG ('N','a','n','d'), /*12.0*/
  TTF_SCRIPT_NYIAKENG_PUACHUE_HMONG	= TTF_TAG ('H','m','n','p'), /*12.0*/
  TTF_SCRIPT_WANCHO			= TTF_TAG ('W','c','h','o'), /*12.0*/

  /*
   * Since 2.6.7
   */
  TTF_SCRIPT_CHORASMIAN			= TTF_TAG ('C','h','r','s'), /*13.0*/
  TTF_SCRIPT_DIVES_AKURU			= TTF_TAG ('D','i','a','k'), /*13.0*/
  TTF_SCRIPT_KHITAN_SMALL_SCRIPT		= TTF_TAG ('K','i','t','s'), /*13.0*/
  TTF_SCRIPT_YEZIDI			= TTF_TAG ('Y','e','z','i'), /*13.0*/

  /* No script set. */
  TTF_SCRIPT_INVALID			= TTF_TAG_NONE,

  /*< private >*/

  _TTF_SCRIPT_MAX_VALUE				= TTF_TAG_MAX_SIGNED, /*< skip >*/
  _TTF_SCRIPT_MAX_VALUE_SIGNED			= TTF_TAG_MAX_SIGNED /*< skip >*/

} ttf_script_t;


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* SDL_TTF_H_ */

/* vi: set ts=4 sw=4 expandtab: */
