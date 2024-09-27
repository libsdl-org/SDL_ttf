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


/**
 *  \file SDL_ttf.h
 *
 *  Header file for SDL_ttf library
 *
 *  This library is a wrapper around the excellent FreeType 2.0 library,
 *  available at: https://www.freetype.org/
 *
 *  Note: In many places, SDL_ttf will say "glyph" when it means "code point."
 *  Unicode is hard, we learn as we go, and we apologize for adding to the
 *  confusion.
 *
 */
#ifndef SDL_TTF_H_
#define SDL_TTF_H_

#include <SDL3/SDL.h>
#include <SDL3/SDL_begin_code.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Printable format: "%d.%d.%d", MAJOR, MINOR, MICRO
 */
#define SDL_TTF_MAJOR_VERSION   3
#define SDL_TTF_MINOR_VERSION   0
#define SDL_TTF_MICRO_VERSION   0

/**
 * This is the version number macro for the current SDL_ttf version.
 */
#define SDL_TTF_VERSION \
    SDL_VERSIONNUM(SDL_TTF_MAJOR_VERSION, SDL_TTF_MINOR_VERSION, SDL_TTF_MICRO_VERSION)

/**
 * This macro will evaluate to true if compiled with SDL_ttf at least X.Y.Z.
 */
#define SDL_TTF_VERSION_ATLEAST(X, Y, Z) \
    ((SDL_TTF_MAJOR_VERSION >= X) && \
     (SDL_TTF_MAJOR_VERSION > X || SDL_TTF_MINOR_VERSION >= Y) && \
     (SDL_TTF_MAJOR_VERSION > X || SDL_TTF_MINOR_VERSION > Y || SDL_TTF_MICRO_VERSION >= Z))

/**
 * This function gets the version of the dynamically linked SDL_ttf library.
 *
 * \returns SDL_ttf version.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC int SDLCALL TTF_Version(void);

/**
 * Query the version of the FreeType library in use.
 *
 * TTF_Init() should be called before calling this function.
 *
 * \param major to be filled in with the major version number. Can be NULL.
 * \param minor to be filled in with the minor version number. Can be NULL.
 * \param patch to be filled in with the param version number. Can be NULL.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_Init
 */
extern SDL_DECLSPEC void SDLCALL TTF_GetFreeTypeVersion(int *major, int *minor, int *patch);

/**
 * Query the version of the HarfBuzz library in use.
 *
 * If HarfBuzz is not available, the version reported is 0.0.0.
 *
 * \param major to be filled in with the major version number. Can be NULL.
 * \param minor to be filled in with the minor version number. Can be NULL.
 * \param patch to be filled in with the param version number. Can be NULL.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL TTF_GetHarfBuzzVersion(int *major, int *minor, int *patch);

/**
 * The internal structure containing font information.
 *
 * Opaque data!
 */
typedef struct TTF_Font TTF_Font;

/**
 * Initialize SDL_ttf.
 *
 * You must successfully call this function before it is safe to call any
 * other function in this library, with one exception: a human-readable error
 * message can be retrieved from SDL_GetError() if this function fails.
 *
 * SDL must be initialized before calls to functions in this library, because
 * this library uses utility functions from the SDL library.
 *
 * It is safe to call this more than once; the library keeps a counter of init
 * calls, and decrements it on each call to TTF_Quit, so you must pair your
 * init and quit calls.
 *
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_Quit
 */
extern SDL_DECLSPEC bool SDLCALL TTF_Init(void);

/**
 * Create a font from a file, using a specified point size.
 *
 * Some .fon fonts will have several sizes embedded in the file, so the point
 * size becomes the index of choosing which size. If the value is too high,
 * the last indexed size will be the default.
 *
 * When done with the returned TTF_Font, use TTF_CloseFont() to dispose of it.
 *
 * \param file path to font file.
 * \param ptsize point size to use for the newly-opened font.
 * \returns a valid TTF_Font, or NULL on failure; call SDL_GetError() for more
 *          information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_CloseFont
 */
extern SDL_DECLSPEC TTF_Font * SDLCALL TTF_OpenFont(const char *file, float ptsize);

/**
 * Create a font from an SDL_IOStream, using a specified point size.
 *
 * Some .fon fonts will have several sizes embedded in the file, so the point
 * size becomes the index of choosing which size. If the value is too high,
 * the last indexed size will be the default.
 *
 * If `closeio` is true, `src` will be automatically closed once the font is
 * closed. Otherwise you should close `src` yourself after closing the font.
 *
 * When done with the returned TTF_Font, use TTF_CloseFont() to dispose of it.
 *
 * \param src an SDL_IOStream to provide a font file's data.
 * \param closeio true to close `src` when the font is closed, false to leave
 *                it open.
 * \param ptsize point size to use for the newly-opened font.
 * \returns a valid TTF_Font, or NULL on failure; call SDL_GetError() for more
 *          information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_CloseFont
 */
extern SDL_DECLSPEC TTF_Font * SDLCALL TTF_OpenFontIO(SDL_IOStream *src, bool closeio, float ptsize);

/**
 * Create a font with the specified properties.
 *
 * These are the supported properties:
 *
 * - `TTF_PROP_FONT_CREATE_FILENAME_STRING`: the font file to open, if an
 *   SDL_IOStream isn't being used. This is required if
 *   `TTF_PROP_FONT_CREATE_IOSTREAM_POINTER` isn't set.
 * - `TTF_PROP_FONT_CREATE_IOSTREAM_POINTER`: an SDL_IOStream containing the
 *   font to be opened. This should not be closed until the font is closed.
 *   This is required if `TTF_PROP_FONT_CREATE_FILENAME_STRING` isn't set.
 * - `TTF_PROP_FONT_CREATE_IOSTREAM_OFFSET_NUMBER`: the offset in the iostream
 *   for the beginning of the font, defaults to 0.
 * - `TTF_PROP_FONT_CREATE_IOSTREAM_AUTOCLOSE_BOOLEAN`: true if closing the
 *   font should also close the associated SDL_IOStream.
 * - `TTF_PROP_FONT_CREATE_SIZE_NUMBER`: the point size of the font. Some .fon
 *   fonts will have several sizes embedded in the file, so the point size
 *   becomes the index of choosing which size. If the value is too high, the
 *   last indexed size will be the default.
 * - `TTF_PROP_FONT_CREATE_FACE_NUMBER`: the face index of the font, if the
 *   font contains multiple font faces.
 * - `TTF_PROP_FONT_CREATE_HORIZONTAL_DPI_NUMBER`: the horizontal DPI to use
 *   for font rendering, defaults to
 *   `TTF_PROP_FONT_CREATE_VERTICAL_DPI_NUMBER` if set, or 72 otherwise.
 * - `TTF_PROP_FONT_CREATE_VERTICAL_DPI_NUMBER`: the vertical DPI to use for
 *   font rendering, defaults to `TTF_PROP_FONT_CREATE_HORIZONTAL_DPI_NUMBER`
 *   if set, or 72 otherwise.
 *
 * \param props the properties to use.
 * \returns a valid TTF_Font, or NULL on failure; call SDL_GetError() for more
 *          information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_CloseFont
 */
extern SDL_DECLSPEC TTF_Font * SDLCALL TTF_OpenFontWithProperties(SDL_PropertiesID props);

#define TTF_PROP_FONT_CREATE_FILENAME_STRING            "SDL_ttf.font.create.filename"
#define TTF_PROP_FONT_CREATE_IOSTREAM_POINTER           "SDL_ttf.font.create.iostream"
#define TTF_PROP_FONT_CREATE_IOSTREAM_OFFSET_NUMBER     "SDL_ttf.font.create.iostream.offset"
#define TTF_PROP_FONT_CREATE_IOSTREAM_AUTOCLOSE_BOOLEAN "SDL_ttf.font.create.iostream.autoclose"
#define TTF_PROP_FONT_CREATE_SIZE_FLOAT                 "SDL_ttf.font.create.size"
#define TTF_PROP_FONT_CREATE_FACE_NUMBER                "SDL_ttf.font.create.face"
#define TTF_PROP_FONT_CREATE_HORIZONTAL_DPI_NUMBER      "SDL_ttf.font.create.hdpi"
#define TTF_PROP_FONT_CREATE_VERTICAL_DPI_NUMBER        "SDL_ttf.font.create.vdpi"

/**
 * Get the properties associated with a font.
 *
 * The following read-only properties are provided by SDL:
 *
 * - `TTF_PROP_FONT_FACE_POINTER`: the FT_Face associated with the font.
 *
 * \param font the font to query.
 * \returns a valid property ID on success or 0 on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC SDL_PropertiesID SDLCALL TTF_GetFontProperties(TTF_Font *font);

#define TTF_PROP_FONT_FACE_POINTER                      "SDL_ttf.font.face"

/**
 * Set a font's size dynamically.
 *
 * This clears already-generated glyphs, if any, from the cache.
 *
 * \param font the font to resize.
 * \param ptsize the new point size.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC bool SDLCALL TTF_SetFontSize(TTF_Font *font, float ptsize);

/**
 * Set font size dynamically with target resolutions (in DPI).
 *
 * This clears already-generated glyphs, if any, from the cache.
 *
 * \param font the font to resize.
 * \param ptsize the new point size.
 * \param hdpi the target horizontal DPI.
 * \param vdpi the target vertical DPI.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC bool SDLCALL TTF_SetFontSizeDPI(TTF_Font *font, float ptsize, unsigned int hdpi, unsigned int vdpi);

/**
 * Font style flags
 */
#define TTF_STYLE_NORMAL        0x00
#define TTF_STYLE_BOLD          0x01
#define TTF_STYLE_ITALIC        0x02
#define TTF_STYLE_UNDERLINE     0x04
#define TTF_STYLE_STRIKETHROUGH 0x08

/**
 * Query a font's current style.
 *
 * The font styles are a set of bit flags, OR'd together:
 *
 * - `TTF_STYLE_NORMAL` (is zero)
 * - `TTF_STYLE_BOLD`
 * - `TTF_STYLE_ITALIC`
 * - `TTF_STYLE_UNDERLINE`
 * - `TTF_STYLE_STRIKETHROUGH`
 *
 * \param font the font to query.
 * \returns the current font style, as a set of bit flags.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_SetFontStyle
 */
extern SDL_DECLSPEC int SDLCALL TTF_GetFontStyle(const TTF_Font *font);

/**
 * Set a font's current style.
 *
 * Setting the style clears already-generated glyphs, if any, from the cache.
 *
 * The font styles are a set of bit flags, OR'd together:
 *
 * - `TTF_STYLE_NORMAL` (is zero)
 * - `TTF_STYLE_BOLD`
 * - `TTF_STYLE_ITALIC`
 * - `TTF_STYLE_UNDERLINE`
 * - `TTF_STYLE_STRIKETHROUGH`
 *
 * \param font the font to set a new style on.
 * \param style the new style values to set, OR'd together.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_GetFontStyle
 */
extern SDL_DECLSPEC void SDLCALL TTF_SetFontStyle(TTF_Font *font, int style);

/**
 * Query a font's current outline.
 *
 * \param font the font to query.
 * \returns the font's current outline value.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_SetFontOutline
 */
extern SDL_DECLSPEC int SDLCALL TTF_GetFontOutline(const TTF_Font *font);

/**
 * Set a font's current outline.
 *
 * \param font the font to set a new outline on.
 * \param outline positive outline value, 0 to default.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_GetFontOutline
 */
extern SDL_DECLSPEC bool SDLCALL TTF_SetFontOutline(TTF_Font *font, int outline);


/**
 * Hinting flags
 */
#define TTF_HINTING_NORMAL          0
#define TTF_HINTING_LIGHT           1
#define TTF_HINTING_MONO            2
#define TTF_HINTING_NONE            3
#define TTF_HINTING_LIGHT_SUBPIXEL  4

/**
 * Query a font's current FreeType hinter setting.
 *
 * The hinter setting is a single value:
 *
 * - `TTF_HINTING_NORMAL`
 * - `TTF_HINTING_LIGHT`
 * - `TTF_HINTING_MONO`
 * - `TTF_HINTING_NONE`
 * - `TTF_HINTING_LIGHT_SUBPIXEL` (available in SDL_ttf 3.0.0 and later)
 *
 * \param font the font to query.
 * \returns the font's current hinter value.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_SetFontHinting
 */
extern SDL_DECLSPEC int SDLCALL TTF_GetFontHinting(const TTF_Font *font);

/**
 * Set a font's current hinter setting.
 *
 * Setting it clears already-generated glyphs, if any, from the cache.
 *
 * The hinter setting is a single value:
 *
 * - `TTF_HINTING_NORMAL`
 * - `TTF_HINTING_LIGHT`
 * - `TTF_HINTING_MONO`
 * - `TTF_HINTING_NONE`
 * - `TTF_HINTING_LIGHT_SUBPIXEL` (available in SDL_ttf 3.0.0 and later)
 *
 * \param font the font to set a new hinter setting on.
 * \param hinting the new hinter setting.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_GetFontHinting
 */
extern SDL_DECLSPEC void SDLCALL TTF_SetFontHinting(TTF_Font *font, int hinting);

/**
 * The horizontal alignment used when rendering wrapped text.
 *
 * \since This enum is available since SDL_ttf 3.0.0.
 */
typedef enum TTF_HorizontalAlignment
{
    TTF_HORIZONTAL_ALIGN_INVALID = -1,
    TTF_HORIZONTAL_ALIGN_LEFT,
    TTF_HORIZONTAL_ALIGN_CENTER,
    TTF_HORIZONTAL_ALIGN_RIGHT
} TTF_HorizontalAlignment;

/**
 * Query a font's current wrap alignment option.
 *
 * \param font the font to query.
 * \returns the font's current wrap alignment option.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_SetFontWrapAlignment
 */
extern SDL_DECLSPEC TTF_HorizontalAlignment SDLCALL TTF_GetFontWrapAlignment(const TTF_Font *font);

/**
 * Set a font's current wrap alignment option.
 *
 * \param font the font to set a new wrap alignment option on.
 * \param align the new wrap alignment option.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_GetFontWrapAlignment
 */
extern SDL_DECLSPEC void SDLCALL TTF_SetFontWrapAlignment(TTF_Font *font, TTF_HorizontalAlignment align);

/**
 * Query the total height of a font.
 *
 * This is usually equal to point size.
 *
 * \param font the font to query.
 * \returns the font's height.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC int SDLCALL TTF_GetFontHeight(const TTF_Font *font);

/**
 * Query the offset from the baseline to the top of a font.
 *
 * This is a positive value, relative to the baseline.
 *
 * \param font the font to query.
 * \returns the font's ascent.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC int SDLCALL TTF_GetFontAscent(const TTF_Font *font);

/**
 * Query the offset from the baseline to the bottom of a font.
 *
 * This is a negative value, relative to the baseline.
 *
 * \param font the font to query.
 * \returns the font's descent.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC int SDLCALL TTF_GetFontDescent(const TTF_Font *font);

/**
 * Query the spacing between lines of text for a font.
 *
 * \param font the font to query.
 * \returns the font's recommended spacing.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC int SDLCALL TTF_GetFontLineSkip(const TTF_Font *font);

/**
 * Set the spacing between lines of text for a font.
 *
 * \param font the font to modify.
 * \param lineskip the new line spacing for the font.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL TTF_SetFontLineSkip(TTF_Font *font, int lineskip);

/**
 * Query whether or not kerning is enabled for a font.
 *
 * \param font the font to query.
 * \returns true if kerning is enabled, false otherwise.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC bool SDLCALL TTF_GetFontKerning(const TTF_Font *font);

/**
 * Set if kerning is enabled for a font.
 *
 * Newly-opened fonts default to allowing kerning. This is generally a good
 * policy unless you have a strong reason to disable it, as it tends to
 * produce better rendering (with kerning disabled, some fonts might render
 * the word `kerning` as something that looks like `keming` for example).
 *
 * \param font the font to set kerning on.
 * \param enabled true to enable kerning, false to disable.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL TTF_SetFontKerning(TTF_Font *font, bool enabled);

/**
 * Query whether a font is fixed-width.
 *
 * A "fixed-width" font means all glyphs are the same width across; a
 * lowercase 'i' will be the same size across as a capital 'W', for example.
 * This is common for terminals and text editors, and other apps that treat
 * text as a grid. Most other things (WYSIWYG word processors, web pages, etc)
 * are more likely to not be fixed-width in most cases.
 *
 * \param font the font to query.
 * \returns true if the font is fixed-width, false otherwise.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC bool SDLCALL TTF_FontIsFixedWidth(const TTF_Font *font);

/**
 * Query whether a font is scalable or not.
 *
 * Scalability lets us distinguish between outline and bitmap fonts.
 *
 * \param font the font to query
 *
 * \returns true if the font is scalable, false otherwise.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_SetFontSDF
 */
extern SDL_DECLSPEC bool TTF_FontIsScalable(const TTF_Font *font);

/**
 * Query a font's family name.
 *
 * This string is dictated by the contents of the font file.
 *
 * Note that the returned string is to internal storage, and should not be
 * modified or free'd by the caller. The string becomes invalid, with the rest
 * of the font, when `font` is handed to TTF_CloseFont().
 *
 * \param font the font to query.
 * \returns the font's family name.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC const char * SDLCALL TTF_GetFontFamilyName(const TTF_Font *font);

/**
 * Query a font's style name.
 *
 * This string is dictated by the contents of the font file.
 *
 * Note that the returned string is to internal storage, and should not be
 * modified or free'd by the caller. The string becomes invalid, with the rest
 * of the font, when `font` is handed to TTF_CloseFont().
 *
 * \param font the font to query.
 * \returns the font's style name.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC const char * SDLCALL TTF_GetFontStyleName(const TTF_Font *font);

/**
 * Check whether a glyph is provided by the font for a 32-bit codepoint.
 *
 * \param font the font to query.
 * \param ch the character code to check.
 * \returns true if font provides a glyph for this character, false if not.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC bool SDLCALL TTF_FontHasGlyph(TTF_Font *font, Uint32 ch);

/**
 * Query the metrics (dimensions) of a font's 32-bit glyph.
 *
 * To understand what these metrics mean, here is a useful link:
 *
 * https://freetype.sourceforge.net/freetype2/docs/tutorial/step2.html
 *
 * \param font the font to query.
 * \param ch the character code to check.
 * \param minx a pointer filled in with the minimum x coordinate of the glyph
 *             from the left edge of its bounding box. This value may be
 *             negative.
 * \param maxx a pointer filled in with the maximum x coordinate of the glyph
 *             from the left edge of its bounding box.
 * \param miny a pointer filled in with the minimum y coordinate of the glyph
 *             from the bottom edge of its bounding box. This value may be
 *             negative.
 * \param maxy a pointer filled in with the maximum y coordinate of the glyph
 *             from the bottom edge of its bounding box.
 * \param advance a pointer filled in with the distance to the next glyph from
 *                the left edge of this glyph's bounding box.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC bool SDLCALL TTF_GetGlyphMetrics(TTF_Font *font, Uint32 ch, int *minx, int *maxx, int *miny, int *maxy, int *advance);

/**
 * Calculate the dimensions of a rendered string of UTF-8 text.
 *
 * This will report the width and height, in pixels, of the space that the
 * specified string will take to fully render.
 *
 * \param font the font to query.
 * \param text text to calculate, in UTF-8 encoding.
 * \param length the length of the text, in bytes, or 0 for null terminated
 *               text.
 * \param w will be filled with width, in pixels, on return.
 * \param h will be filled with height, in pixels, on return.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC bool SDLCALL TTF_GetTextSize(TTF_Font *font, const char *text, size_t length, int *w, int *h);

/**
 * Calculate the dimensions of a rendered string of UTF-8 text.
 *
 * This will report the width and height, in pixels, of the space that the
 * specified string will take to fully render.
 *
 * Text is wrapped to multiple lines on line endings and on word boundaries if
 * it extends beyond `wrapLength` in pixels.
 *
 * If wrapLength is 0, this function will only wrap on newline characters.
 *
 * \param font the font to query.
 * \param text text to calculate, in UTF-8 encoding.
 * \param length the length of the text, in bytes, or 0 for null terminated
 *               text.
 * \param wrapLength the maximum width or 0 to wrap on newline characters.
 * \param w will be filled with width, in pixels, on return.
 * \param h will be filled with height, in pixels, on return.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC bool SDLCALL TTF_GetTextSizeWrapped(TTF_Font *font, const char *text, size_t length, int wrapLength, int *w, int *h);

/**
 * Calculate how much of a UTF-8 string will fit in a given width.
 *
 * This reports the number of characters that can be rendered before reaching
 * `measure_width`.
 *
 * This does not need to render the string to do this calculation.
 *
 * \param font the font to query.
 * \param text text to calculate, in UTF-8 encoding.
 * \param length the length of the text, in bytes, or 0 for null terminated
 *               text.
 * \param measure_width maximum width, in pixels, available for the string.
 * \param extent on return, filled with latest calculated width.
 * \param count on return, filled with number of characters that can be
 *              rendered.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC bool SDLCALL TTF_MeasureText(TTF_Font *font, const char *text, size_t length, int measure_width, int *extent, int *count);

/**
 * Render UTF-8 text at high quality to a new 8-bit surface.
 *
 * This function will allocate a new 8-bit, palettized surface. The surface's
 * 0 pixel will be the specified background color, while other pixels have
 * varying degrees of the foreground color. This function returns the new
 * surface, or NULL if there was an error.
 *
 * This will not word-wrap the string; you'll get a surface with a single line
 * of text, as long as the string requires. You can use
 * TTF_RenderText_Shaded_Wrapped() instead if you need to wrap the output to
 * multiple lines.
 *
 * This will not wrap on newline characters.
 *
 * \param font the font to render with.
 * \param text text to render, in UTF-8 encoding.
 * \param length the length of the text, in bytes, or 0 for null terminated
 *               text.
 * \param fg the foreground color for the text.
 * \param bg the background color for the text.
 * \returns a new 8-bit, palettized surface, or NULL if there was an error.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_RenderText_Blended
 * \sa TTF_RenderText_LCD
 * \sa TTF_RenderText_Shaded_Wrapped
 */
extern SDL_DECLSPEC SDL_Surface * SDLCALL TTF_RenderText_Shaded(TTF_Font *font, const char *text, size_t length, SDL_Color fg, SDL_Color bg);

/**
 * Render word-wrapped UTF-8 text at high quality to a new 8-bit surface.
 *
 * This function will allocate a new 8-bit, palettized surface. The surface's
 * 0 pixel will be the specified background color, while other pixels have
 * varying degrees of the foreground color. This function returns the new
 * surface, or NULL if there was an error.
 *
 * Text is wrapped to multiple lines on line endings and on word boundaries if
 * it extends beyond `wrapLength` in pixels.
 *
 * If wrapLength is 0, this function will only wrap on newline characters.
 *
 * \param font the font to render with.
 * \param text text to render, in UTF-8 encoding.
 * \param length the length of the text, in bytes, or 0 for null terminated
 *               text.
 * \param fg the foreground color for the text.
 * \param bg the background color for the text.
 * \param wrapLength the maximum width of the text surface or 0 to wrap on
 *                   newline characters.
 * \returns a new 8-bit, palettized surface, or NULL if there was an error.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_RenderText_Blended_Wrapped
 * \sa TTF_RenderText_LCD_Wrapped
 * \sa TTF_RenderText_Shaded
 */
extern SDL_DECLSPEC SDL_Surface * SDLCALL TTF_RenderText_Shaded_Wrapped(TTF_Font *font, const char *text, size_t length, SDL_Color fg, SDL_Color bg, int wrapLength);

/**
 * Render a single 32-bit glyph at high quality to a new 8-bit surface.
 *
 * This function will allocate a new 8-bit, palettized surface. The surface's
 * 0 pixel will be the specified background color, while other pixels have
 * varying degrees of the foreground color. This function returns the new
 * surface, or NULL if there was an error.
 *
 * The glyph is rendered without any padding or centering in the X direction,
 * and aligned normally in the Y direction.
 *
 * \param font the font to render with.
 * \param ch the character to render.
 * \param fg the foreground color for the text.
 * \param bg the background color for the text.
 * \returns a new 8-bit, palettized surface, or NULL if there was an error.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_RenderGlyph_Blended
 * \sa TTF_RenderGlyph_LCD
 */
extern SDL_DECLSPEC SDL_Surface * SDLCALL TTF_RenderGlyph_Shaded(TTF_Font *font, Uint32 ch, SDL_Color fg, SDL_Color bg);

/**
 * Render UTF-8 text at high quality to a new ARGB surface.
 *
 * This function will allocate a new 32-bit, ARGB surface, using alpha
 * blending to dither the font with the given color. This function returns the
 * new surface, or NULL if there was an error.
 *
 * This will not word-wrap the string; you'll get a surface with a single line
 * of text, as long as the string requires. You can use
 * TTF_RenderText_Blended_Wrapped() instead if you need to wrap the output to
 * multiple lines.
 *
 * This will not wrap on newline characters.
 *
 * \param font the font to render with.
 * \param text text to render, in UTF-8 encoding.
 * \param length the length of the text, in bytes, or 0 for null terminated
 *               text.
 * \param fg the foreground color for the text.
 * \returns a new 32-bit, ARGB surface, or NULL if there was an error.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_RenderText_Blended_Wrapped
 * \sa TTF_RenderText_LCD
 * \sa TTF_RenderText_Shaded
 */
extern SDL_DECLSPEC SDL_Surface * SDLCALL TTF_RenderText_Blended(TTF_Font *font, const char *text, size_t length, SDL_Color fg);

/**
 * Render word-wrapped UTF-8 text at high quality to a new ARGB surface.
 *
 * This function will allocate a new 32-bit, ARGB surface, using alpha
 * blending to dither the font with the given color. This function returns the
 * new surface, or NULL if there was an error.
 *
 * Text is wrapped to multiple lines on line endings and on word boundaries if
 * it extends beyond `wrapLength` in pixels.
 *
 * If wrapLength is 0, this function will only wrap on newline characters.
 *
 * \param font the font to render with.
 * \param text text to render, in UTF-8 encoding.
 * \param length the length of the text, in bytes, or 0 for null terminated
 *               text.
 * \param fg the foreground color for the text.
 * \param wrapLength the maximum width of the text surface or 0 to wrap on
 *                   newline characters.
 * \returns a new 32-bit, ARGB surface, or NULL if there was an error.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_RenderText_Blended
 * \sa TTF_RenderText_LCD_Wrapped
 * \sa TTF_RenderText_Shaded_Wrapped
 */
extern SDL_DECLSPEC SDL_Surface * SDLCALL TTF_RenderText_Blended_Wrapped(TTF_Font *font, const char *text, size_t length, SDL_Color fg, int wrapLength);

/**
 * Render a single 32-bit glyph at high quality to a new ARGB surface.
 *
 * This function will allocate a new 32-bit, ARGB surface, using alpha
 * blending to dither the font with the given color. This function returns the
 * new surface, or NULL if there was an error.
 *
 * The glyph is rendered without any padding or centering in the X direction,
 * and aligned normally in the Y direction.
 *
 * \param font the font to render with.
 * \param ch the character to render.
 * \param fg the foreground color for the text.
 * \returns a new 32-bit, ARGB surface, or NULL if there was an error.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_RenderGlyph_LCD
 * \sa TTF_RenderGlyph_Shaded
 */
extern SDL_DECLSPEC SDL_Surface * SDLCALL TTF_RenderGlyph_Blended(TTF_Font *font, Uint32 ch, SDL_Color fg);

/**
 * Render UTF-8 text at LCD subpixel quality to a new ARGB surface.
 *
 * This function will allocate a new 32-bit, ARGB surface, and render
 * alpha-blended text using FreeType's LCD subpixel rendering. This function
 * returns the new surface, or NULL if there was an error.
 *
 * This will not word-wrap the string; you'll get a surface with a single line
 * of text, as long as the string requires. You can use
 * TTF_RenderText_LCD_Wrapped() instead if you need to wrap the output to
 * multiple lines.
 *
 * This will not wrap on newline characters.
 *
 * \param font the font to render with.
 * \param text text to render, in UTF-8 encoding.
 * \param length the length of the text, in bytes, or 0 for null terminated
 *               text.
 * \param fg the foreground color for the text.
 * \param bg the background color for the text.
 * \returns a new 32-bit, ARGB surface, or NULL if there was an error.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_RenderText_Blended
 * \sa TTF_RenderText_LCD_Wrapped
 * \sa TTF_RenderText_Shaded
 */
extern SDL_DECLSPEC SDL_Surface * SDLCALL TTF_RenderText_LCD(TTF_Font *font, const char *text, size_t length, SDL_Color fg, SDL_Color bg);

/**
 * Render word-wrapped UTF-8 text at LCD subpixel quality to a new ARGB
 * surface.
 *
 * This function will allocate a new 32-bit, ARGB surface, and render
 * alpha-blended text using FreeType's LCD subpixel rendering. This function
 * returns the new surface, or NULL if there was an error.
 *
 * Text is wrapped to multiple lines on line endings and on word boundaries if
 * it extends beyond `wrapLength` in pixels.
 *
 * If wrapLength is 0, this function will only wrap on newline characters.
 *
 * \param font the font to render with.
 * \param text text to render, in UTF-8 encoding.
 * \param length the length of the text, in bytes, or 0 for null terminated
 *               text.
 * \param fg the foreground color for the text.
 * \param bg the background color for the text.
 * \param wrapLength the maximum width of the text surface or 0 to wrap on
 *                   newline characters.
 * \returns a new 32-bit, ARGB surface, or NULL if there was an error.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_RenderText_Blended_Wrapped
 * \sa TTF_RenderText_LCD
 * \sa TTF_RenderText_Shaded_Wrapped
 */
extern SDL_DECLSPEC SDL_Surface * SDLCALL TTF_RenderText_LCD_Wrapped(TTF_Font *font, const char *text, size_t length, SDL_Color fg, SDL_Color bg, int wrapLength);

/**
 * Render a single 32-bit glyph at LCD subpixel quality to a new ARGB surface.
 *
 * This function will allocate a new 32-bit, ARGB surface, and render
 * alpha-blended text using FreeType's LCD subpixel rendering. This function
 * returns the new surface, or NULL if there was an error.
 *
 * The glyph is rendered without any padding or centering in the X direction,
 * and aligned normally in the Y direction.
 *
 * \param font the font to render with.
 * \param ch the character to render.
 * \param fg the foreground color for the text.
 * \param bg the background color for the text.
 * \returns a new 32-bit, ARGB surface, or NULL if there was an error.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_RenderGlyph_Blended
 * \sa TTF_RenderGlyph_Shaded
 */
extern SDL_DECLSPEC SDL_Surface * SDLCALL TTF_RenderGlyph_LCD(TTF_Font *font, Uint32 ch, SDL_Color fg, SDL_Color bg);


/**
 * Dispose of a previously-created font.
 *
 * Call this when done with a font. This function will free any resources
 * associated with it. It is safe to call this function on NULL, for example
 * on the result of a failed call to TTF_OpenFont().
 *
 * The font is not valid after being passed to this function. String pointers
 * from functions that return information on this font, such as
 * TTF_GetFontFamilyName() and TTF_GetFontStyleName(), are no longer valid
 * after this call, as well.
 *
 * \param font the font to dispose of.
 *
 * \threadsafety This function should not be called while any other thread is
 *               using the font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_OpenFont
 * \sa TTF_OpenFontIndexDPIIO
 * \sa TTF_OpenFontIO
 * \sa TTF_OpenFontDPI
 * \sa TTF_OpenFontDPIIO
 * \sa TTF_OpenFontIndex
 * \sa TTF_OpenFontIndexDPI
 * \sa TTF_OpenFontIndexDPIIO
 * \sa TTF_OpenFontIndexIO
 */
extern SDL_DECLSPEC void SDLCALL TTF_CloseFont(TTF_Font *font);

/**
 * Deinitialize SDL_ttf.
 *
 * You must call this when done with the library, to free internal resources.
 * It is safe to call this when the library isn't initialized, as it will just
 * return immediately.
 *
 * Once you have as many quit calls as you have had successful calls to
 * TTF_Init, the library will actually deinitialize.
 *
 * Please note that this does not automatically close any fonts that are still
 * open at the time of deinitialization, and it is possibly not safe to close
 * them afterwards, as parts of the library will no longer be initialized to
 * deal with it. A well-written program should call TTF_CloseFont() on any
 * open fonts before calling this function!
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL TTF_Quit(void);

/**
 * Check if SDL_ttf is initialized.
 *
 * This reports the number of times the library has been initialized by a call
 * to TTF_Init(), without a paired deinitialization request from TTF_Quit().
 *
 * In short: if it's greater than zero, the library is currently initialized
 * and ready to work. If zero, it is not initialized.
 *
 * Despite the return value being a signed integer, this function should not
 * return a negative number.
 *
 * \returns the current number of initialization calls, that need to
 *          eventually be paired with this many calls to TTF_Quit().
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_Init
 * \sa TTF_Quit
 */
extern SDL_DECLSPEC int SDLCALL TTF_WasInit(void);

/**
 * Query the kerning size of two 32-bit glyphs.
 *
 * \param font the font to query.
 * \param previous_ch the previous character's code, 32 bits.
 * \param ch the current character's code, 32 bits.
 * \param kerning a pointer filled in with the kerning size between the two specified characters, in pixels, may be NULL.
 * \returns true on success or false on failure; call SDL_GetError()
 *          for more information.
 *
 * \threadsafety This function should be called on the thread that created the font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC bool TTF_GetGlyphKerning(TTF_Font *font, Uint32 previous_ch, Uint32 ch, int *kerning);

/**
 * Enable Signed Distance Field rendering for a font.
 *
 * This works with the Blended APIs. SDF is a technique that
 * helps fonts look sharp even when scaling and rotating.
 *
 * This clears already-generated glyphs, if any, from the cache.
 *
 * \param font the font to set SDF support on.
 * \param enabled true to enable SDF, false to disable.
 * \returns true on success or false on failure; call SDL_GetError()
 *          for more information.
 *
 * \threadsafety This function should be called on the thread that created the font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_GetFontSDF
 */
extern SDL_DECLSPEC bool TTF_SetFontSDF(TTF_Font *font, bool enabled);

/**
 * Query whether Signed Distance Field rendering is enabled for a font.
 *
 * \param font the font to query
 *
 * \returns true if enabled, false otherwise.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_SetFontSDF
 */
extern SDL_DECLSPEC bool TTF_GetFontSDF(const TTF_Font *font);

/**
 * Direction flags
 *
 * \since This enum is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_SetFontDirection
 */
typedef enum TTF_Direction
{
  TTF_DIRECTION_LTR = 0,    /* Left to Right */
  TTF_DIRECTION_RTL,        /* Right to Left */
  TTF_DIRECTION_TTB,        /* Top to Bottom */
  TTF_DIRECTION_BTT         /* Bottom to Top */
} TTF_Direction;

/**
 * Set direction to be used for text shaping by a font.
 *
 * Possible direction values are:
 *
 * - `TTF_DIRECTION_LTR` (Left to Right)
 * - `TTF_DIRECTION_RTL` (Right to Left)
 * - `TTF_DIRECTION_TTB` (Top to Bottom)
 * - `TTF_DIRECTION_BTT` (Bottom to Top)
 *
 * If SDL_ttf was not built with HarfBuzz support, this function returns
 * false.
 *
 * \param font the font to specify a direction for.
 * \param direction the new direction for text to flow.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC bool SDLCALL TTF_SetFontDirection(TTF_Font *font, TTF_Direction direction);

/**
 * Set script to be used for text shaping by a font.
 *
 * The supplied script value must be a null-terminated string of exactly four
 * characters.
 *
 * If SDL_ttf was not built with HarfBuzz support, this function returns
 * false.
 *
 * \param font the font to specify a script name for.
 * \param script null-terminated string of exactly 4 characters.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 *
 * \threadsafety This function is not thread-safe.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC bool SDLCALL TTF_SetFontScript(TTF_Font *font, const char *script);

/**
 * Get the script used by a 32-bit codepoint.
 *
 * The supplied script value will be a null-terminated string of exactly four
 * characters.
 *
 * If SDL_ttf was not built with HarfBuzz support, this function returns
 * false.
 *
 * \param ch the character code to check.
 * \param script a pointer filled in with the script used by `ch`.
 * \param script_size the size of the script buffer, which must be at least 5
 *                    characters.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 *
 * \threadsafety This function should be called on the thread that created the
 *               font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC bool SDLCALL TTF_GetGlyphScript(Uint32 ch, char *script, size_t script_size);

/**
 * Set language to be used for text shaping by a font.
 *
 * If SDL_ttf was not built with HarfBuzz support, this function returns false.
 *
 * \param font the font to specify a language for.
 * \param language_bcp47 a null-terminated string containing the desired language's BCP47 code. Or null to reset the value.
 * \returns true on success or false on failure; call SDL_GetError()
 *          for more information.
 *
 * \threadsafety This function should be called on the thread that created the font.
 *
 * \since This function is available since SDL_ttf 3.0.0.
 */
extern SDL_DECLSPEC bool TTF_SetFontLanguage(TTF_Font *font, const char *language_bcp47);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_TTF_H_ */

/* vi: set ts=4 sw=4 expandtab: */
