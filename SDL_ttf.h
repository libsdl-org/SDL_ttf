/*
    SDL_ttf:  A companion library to SDL for working with TrueType (tm) fonts
    Copyright (C) 1997  Sam Lantinga

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    5635-34 Springhouse Dr.
    Pleasanton, CA 94588 (USA)
    slouken@devolution.com
*/

/* This library is a wrapper around the excellent FreeType 1.0 library,
   available at:
	http://www.physiol.med.tu-muenchen.de/~robert/freetype.html
*/

#ifndef _SDLttf_h
#define _SDLttf_h

#include "SDL.h"
#include "begin_code.h"

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* The internal structure containing font information */
typedef struct _TTF_Font TTF_Font;

/* Initialize the TTF engine - returns 0 if successful, -1 on error */
extern DECLSPEC int TTF_Init(void);

/* Open a font file and create a font of the specified point size */
extern DECLSPEC TTF_Font *TTF_OpenFont(const char *file, int ptsize);

/* Set and retrieve the font style
   This font style is implemented by modifying the font glyphs, and
   doesn't reflect any inherent properties of the truetype font file.
*/
#define TTF_STYLE_NORMAL	0x00
#define TTF_STYLE_BOLD		0x01
#define TTF_STYLE_ITALIC	0x02
#define TTF_STYLE_UNDERLINE	0x04
extern DECLSPEC int TTF_GetFontStyle(TTF_Font *font);
extern DECLSPEC void TTF_SetFontStyle(TTF_Font *font, int style);

/* Get the total height of the font - usually equal to point size */
extern DECLSPEC int TTF_FontHeight(TTF_Font *font);

/* Get the offset from the baseline to the top of the font
   This is a positive value, relative to the baseline.
 */
extern DECLSPEC int TTF_FontAscent(TTF_Font *font);

/* Get the offset from the baseline to the bottom of the font
   This is a negative value, relative to the baseline.
 */
extern DECLSPEC int TTF_FontDescent(TTF_Font *font);

/* Get the recommended spacing between lines of text for this font */
extern DECLSPEC int TTF_FontLineSkip(TTF_Font *font);

/* Get the metrics (dimensions) of a glyph */
extern DECLSPEC int TTF_GlyphMetrics(TTF_Font *font, Uint16 ch,
				     int *minx, int *maxx,
                                     int *miny, int *maxy, int *advance);

/* Get the dimensions of a rendered string of text */
extern DECLSPEC int TTF_SizeText(TTF_Font *font, const char *text, int *w, int *h);
extern DECLSPEC int TTF_SizeUTF8(TTF_Font *font, const char *text, int *w, int *h);
extern DECLSPEC int TTF_SizeUNICODE(TTF_Font *font, const Uint16 *text, int *w, int *h);

/* Create an 8-bit palettized surface and render the given text at
   fast quality with the given font and color.  The 0 pixel is the
   colorkey, giving a transparent background, and the 1 pixel is set
   to the text color.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface *TTF_RenderText_Solid(TTF_Font *font,
				const char *text, SDL_Color fg);
extern DECLSPEC SDL_Surface *TTF_RenderUTF8_Solid(TTF_Font *font,
				const char *text, SDL_Color fg);
extern DECLSPEC SDL_Surface *TTF_RenderUNICODE_Solid(TTF_Font *font,
				const Uint16 *text, SDL_Color fg);

/* Create an 8-bit palettized surface and render the given glyph at
   fast quality with the given font and color.  The 0 pixel is the
   colorkey, giving a transparent background, and the 1 pixel is set
   to the text color.  The glyph is rendered without any padding or
   centering in the X direction, and aligned normally in the Y direction.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface *TTF_RenderGlyph_Solid(TTF_Font *font,
					Uint16 ch, SDL_Color fg);

/* Create an 8-bit palettized surface and render the given text at
   high quality with the given font and colors.  The 0 pixel is background,
   while other pixels have varying degrees of the foreground color.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface *TTF_RenderText_Shaded(TTF_Font *font,
				const char *text, SDL_Color fg, SDL_Color bg);
extern DECLSPEC SDL_Surface *TTF_RenderUTF8_Shaded(TTF_Font *font,
				const char *text, SDL_Color fg, SDL_Color bg);
extern DECLSPEC SDL_Surface *TTF_RenderUNICODE_Shaded(TTF_Font *font,
				const Uint16 *text, SDL_Color fg, SDL_Color bg);

/* Create an 8-bit palettized surface and render the given glyph at
   high quality with the given font and colors.  The 0 pixel is background,
   while other pixels have varying degrees of the foreground color.
   The glyph is rendered without any padding or centering in the X
   direction, and aligned normally in the Y direction.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface *TTF_RenderGlyph_Shaded(TTF_Font *font,
				Uint16 ch, SDL_Color fg, SDL_Color bg);

/* Create a 32-bit ARGB surface and render the given text at high quality,
   using alpha blending to dither the font with the given color.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface *TTF_RenderText_Blended(TTF_Font *font,
				const char *text, SDL_Color fg);
extern DECLSPEC SDL_Surface *TTF_RenderUTF8_Blended(TTF_Font *font,
				const char *text, SDL_Color fg);
extern DECLSPEC SDL_Surface *TTF_RenderUNICODE_Blended(TTF_Font *font,
				const Uint16 *text, SDL_Color fg);

/* Create a 32-bit ARGB surface and render the given glyph at high quality,
   using alpha blending to dither the font with the given color.
   The glyph is rendered without any padding or centering in the X
   direction, and aligned normally in the Y direction.
   This function returns the new surface, or NULL if there was an error.
*/
extern DECLSPEC SDL_Surface *TTF_RenderGlyph_Blended(TTF_Font *font,
						Uint16 ch, SDL_Color fg);

/* For compatibility with previous versions, here are the old functions */
#define TTF_RenderText(font, text, fg, bg)	\
	TTF_RenderText_Shaded(font, text, fg, bg)
#define TTF_RenderUTF8(font, text, fg, bg)	\
	TTF_RenderUTF8_Shaded(font, text, fg, bg)
#define TTF_RenderUNICODE(font, text, fg, bg)	\
	TTF_RenderUNICODE_Shaded(font, text, fg, bg)

/* Close an opened font file */
extern DECLSPEC void TTF_CloseFont(TTF_Font *font);

/* De-initialize the TTF engine */
extern DECLSPEC void TTF_Quit(void);

/* We'll use SDL for reporting errors */
#define TTF_SetError	SDL_SetError
#define TTF_GetError	SDL_GetError

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
};
#endif
#include "close_code.h"

#endif /* _SDLttf_h */
