#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <freetype/freetype.h>
#include <freetype/ftoutln.h>
#include <freetype/ttnameid.h>

#include "SDL.h"
#include "SDL_ttf.h"

/* Macro to convert a character to a Unicode value -- assume already Unicode */
#define UNICODE(c)	c

/* FIXME: Right now we assume the gray-scale renderer Freetype is using
   supports 256 shades of gray, but we should instead key off of num_grays
   in the result FT_Bitmap after the FT_Render_Glyph() call. */
#define NUM_GRAYS       256

/* Handy routines for converting from fixed point */
#define FT_FLOOR(X)	((X & -64) / 64)
#define FT_CEIL(X)	(((X + 63) & -64) / 64)

#define CACHED_METRICS	0x10
#define CACHED_BITMAP	0x01
#define CACHED_PIXMAP	0x02

/* Cached glyph information */
typedef struct cached_glyph {
	int stored;
	FT_UInt index;
	FT_Bitmap bitmap;
	FT_Bitmap pixmap;
	int minx;
	int maxx;
	int miny;
	int maxy;
	int yoffset;
	int advance;
	Uint16 cached;
} c_glyph;

/* The structure used to hold internal font information */
struct _TTF_Font {
	/* Freetype2 maintains all sorts of useful info itself */
	FT_Face face;

	/* We'll cache these ourselves */
	int height;
	int ascent;
	int descent;
	int lineskip;

	/* The font style */
	int style;

	/* Extra width in glyph bounds for text styles */
	int glyph_overhang;
	float glyph_italics;

	/* Information in the font for underlining */
	int underline_offset;
	int underline_height;

	/* Cache for style-transformed glyphs */
	c_glyph *current;
	c_glyph cache[256];
	c_glyph scratch;
};

/* The FreeType font engine/library */
static FT_Library library;
static int TTF_initialized = 0;

static void TTF_SetFTError(const char *msg, FT_Error error)
{
#ifdef USE_FREETYPE_ERRORS
#undef FTERRORS_H
#define FT_ERRORDEF( e, v, s )  { e, s },
	static const struct
	{
	  int          err_code;
	  const char*  err_msg;
	} ft_errors[] = {
#include <freetype/fterrors.h>
	};
	int i;
	const char *err_msg;
	char buffer[1024];

	err_msg = NULL;
	for ( i=0; i<((sizeof ft_errors)/(sizeof ft_errors[0])); ++i ) {
		if ( error == ft_errors[i].err_code ) {
			err_msg = ft_errors[i].err_msg;
			break;
		}
	}
	if ( ! err_msg ) {
		err_msg = "unknown FreeType error";
	}
	sprintf(buffer, "%s: %s", msg, err_msg);
	TTF_SetError(buffer);
#else
	TTF_SetError(msg);
#endif /* USE_FREETYPE_ERRORS */
}

int TTF_Init( void )
{
	int status;
	FT_Error error;

	status = 0;
	error = FT_Init_FreeType( &library );
	if ( error ) {
		TTF_SetFTError( "Couldn't init FreeType engine", error );
		status = -1;
	} else {
		TTF_initialized = 1;
	}
	return status;
}

TTF_Font* TTF_OpenFontIndex( const char *file, int ptsize, long index )
{
	TTF_Font* font;
	FT_Error error;
	FT_Face face;
	FT_Fixed scale;

	font = (TTF_Font*) malloc(sizeof *font);
	if ( font == NULL ) {
		TTF_SetError( "Out of memory" );
		return NULL;
	}
	memset( font, 0, sizeof( *font ) );

	/* Open the font and create ancillary data */
	error = FT_New_Face( library, file, 0, &font->face );
	if( error ) {
		TTF_SetFTError( "Couldn't load font file", error);
		free( font );
		return NULL;
	}
	if ( index != 0 ) {
		if ( font->face->num_faces > index ) {
		  	FT_Done_Face( font->face );
			error = FT_New_Face( library, file, index, &font->face );
			if( error ) {
				TTF_SetFTError( "Couldn't get font face", error);
				free( font );
				return NULL;
			}
		} else {
			TTF_SetFTError( "No such font face", error);
			free( font );
			return NULL;
		}
	}
	face = font->face;

	/* Make sure that our font face is scalable (global metrics) */
	if ( ! FT_IS_SCALABLE(face) ) {
		TTF_SetError("Font face is not scalable");
		TTF_CloseFont( font );
		return NULL;
	}

	/* Set the character size and use default DPI (72) */
	error = FT_Set_Char_Size( font->face, 0, ptsize * 64, 0, 0 );
	if( error ) {
		TTF_SetFTError( "Couldn't set font size", error );
		TTF_CloseFont( font );
		return NULL;
	}

	/* Get the scalable font metrics for this font */
	scale = face->size->metrics.y_scale;
	font->ascent  = FT_CEIL(FT_MulFix(face->bbox.yMax, scale));
	font->descent = FT_CEIL(FT_MulFix(face->bbox.yMin, scale));
	font->height  = font->ascent - font->descent + /* baseline */ 1;
	font->lineskip = FT_CEIL(FT_MulFix(face->height, scale));
	font->underline_offset = FT_FLOOR(FT_MulFix(face->underline_position, scale));
	font->underline_height = FT_FLOOR(FT_MulFix(face->underline_thickness, scale));
	if ( font->underline_height < 1 ) {
		font->underline_height = 1;
	}
#ifdef DEBUG_FONTS
	printf("Font metrics:\n");
	printf("\tascent = %d, descent = %d\n",
		font->ascent, font->descent);
	printf("\theight = %d, lineskip = %d\n",
		font->height, font->lineskip);
	printf("\tunderline_offset = %d, underline_height = %d\n",
		font->underline_offset, font->underline_height);
#endif

	/* Set the default font style */
	font->style = TTF_STYLE_NORMAL;
	font->glyph_overhang = face->size->metrics.y_ppem / 10;
	/* x offset = cos(((90.0-12)/360)*2*M_PI), or 12 degree angle */
	font->glyph_italics = 0.207f;
	font->glyph_italics *= font->height;

	return font;
}

TTF_Font* TTF_OpenFont( const char *file, int ptsize )
{
	TTF_OpenFontIndex(file, ptsize, 0);
}

static void Flush_Glyph( c_glyph* glyph )
{
	glyph->stored = 0;
	glyph->index = 0;
	if( glyph->bitmap.buffer ) {
		free( glyph->bitmap.buffer );
		glyph->bitmap.buffer = 0;
	}
	if( glyph->pixmap.buffer ) {
		free( glyph->pixmap.buffer );
		glyph->pixmap.buffer = 0;
	}
	glyph->cached = 0;
}
	
static void Flush_Cache( TTF_Font* font )
{
	int i;
	int size = sizeof( font->cache ) / sizeof( font->cache[0] );

	for( i = 0; i < size; ++i ) {
		if( font->cache[i].cached ) {
			Flush_Glyph( &font->cache[i] );
		}

	}
	if( font->scratch.cached ) {
		Flush_Glyph( &font->scratch );
	}

}

static FT_Error Load_Glyph( TTF_Font* font, Uint16 ch, c_glyph* cached, int want )
{
	FT_Face face;
	FT_Error error;
	FT_GlyphSlot glyph;
	FT_Glyph_Metrics* metrics;
	FT_Outline* outline;

	assert( font );
	assert( font->face );

	face = font->face;

	/* Load the glyph */
	if ( ! cached->index ) {
		cached->index = FT_Get_Char_Index( face, ch );
	}
	error = FT_Load_Glyph( face, cached->index, FT_LOAD_DEFAULT );
	if( error ) {
		return error;
	}

	/* Get our glyph shortcuts */
	glyph = face->glyph;
	metrics = &glyph->metrics;
	outline = &glyph->outline;

	/* Get the glyph metrics if desired */
	if ( (want & CACHED_METRICS) && !(cached->stored & CACHED_METRICS) ) {
		/* Get the bounding box */
		cached->minx = FT_FLOOR(metrics->horiBearingX);
		cached->maxx = cached->minx + FT_CEIL(metrics->width);
		cached->maxy = FT_FLOOR(metrics->horiBearingY);
		cached->miny = cached->maxy - FT_CEIL(metrics->height);
		cached->yoffset = font->ascent - cached->maxy;
		cached->advance = FT_CEIL(metrics->horiAdvance);

		/* Adjust for bold and italic text */
		if( font->style & TTF_STYLE_BOLD ) {
			cached->maxx += font->glyph_overhang;
		}
		if( font->style & TTF_STYLE_ITALIC ) {
			cached->maxx += (int)ceil(font->glyph_italics);
		}
		cached->stored |= CACHED_METRICS;
	}

	if ( ((want & CACHED_BITMAP) && !(cached->stored & CACHED_BITMAP)) ||
	     ((want & CACHED_PIXMAP) && !(cached->stored & CACHED_PIXMAP)) ) {
		int mono = (want & CACHED_BITMAP);
		int i;
		FT_Bitmap* src;
		FT_Bitmap* dst;

		/* Handle the italic style */
		if( font->style & TTF_STYLE_ITALIC ) {
			FT_Matrix shear;

			shear.xx = 1 << 16;
			shear.xy = (int) ( font->glyph_italics * ( 1 << 16 ) ) / font->height;
			shear.yx = 0;
			shear.yy = 1 << 16;

			FT_Outline_Transform( outline, &shear );
		}

		/* Render the glyph */
		if ( mono ) {
			error = FT_Render_Glyph( glyph, ft_render_mode_mono );
		} else {
			error = FT_Render_Glyph( glyph, ft_render_mode_normal );
		}
		if( error ) {
			return error;
		}

		/* Copy over information to cache */
		src = &glyph->bitmap;
		if ( mono ) {
			dst = &cached->bitmap;
		} else {
			dst = &cached->pixmap;
		}
		memcpy( dst, src, sizeof( *dst ) );
		if ( mono ) {
			dst->pitch *= 8;
		}

		/* Adjust for bold and italic text */
		if( font->style & TTF_STYLE_BOLD ) {
			int bump = font->glyph_overhang;
			dst->pitch += bump;
			dst->width += bump;
		}
		if( font->style & TTF_STYLE_ITALIC ) {
			int bump = (int)ceil(font->glyph_italics);
			dst->pitch += bump;
			dst->width += bump;
		}

		dst->buffer = malloc( dst->pitch * dst->rows );
		if( !dst->buffer ) {
			return FT_Err_Out_Of_Memory;
		}
		memset( dst->buffer, 0, dst->pitch * dst->rows );

		for( i = 0; i < src->rows; i++ ) {
			int soffset = i * src->pitch;
			int doffset = i * dst->pitch;
			if ( mono ) {
				char *srcp = src->buffer + soffset;
				char *dstp = dst->buffer + doffset;
				int j;
				for ( j = 0; j < src->width; j += 8 ) {
					unsigned char ch = *srcp++;
					*dstp++ = (ch&0x80) >> 7;
					ch <<= 1;
					*dstp++ = (ch&0x80) >> 7;
					ch <<= 1;
					*dstp++ = (ch&0x80) >> 7;
					ch <<= 1;
					*dstp++ = (ch&0x80) >> 7;
					ch <<= 1;
					*dstp++ = (ch&0x80) >> 7;
					ch <<= 1;
					*dstp++ = (ch&0x80) >> 7;
					ch <<= 1;
					*dstp++ = (ch&0x80) >> 7;
					ch <<= 1;
					*dstp++ = (ch&0x80) >> 7;
				}
			} else {
				memcpy(dst->buffer+doffset,
				       src->buffer+soffset, src->pitch);
			}
		}

		/* Handle the bold style */
		if ( font->style & TTF_STYLE_BOLD ) {
			int row;
			int col;
			int offset;
			int pixel;
			Uint8* pixmap;

			/* The pixmap is a little hard, we have to add and clamp */
			for( row = dst->rows - 1; row >= 0; --row ) {
				pixmap = (Uint8*) dst->buffer + row * dst->pitch;
				for( offset=1; offset <= font->glyph_overhang; ++offset ) {
					for( col = dst->width - 1; col > 0; --col ) {
						pixel = (pixmap[col] + pixmap[col-1]);
						if( pixel > NUM_GRAYS - 1 ) {
							pixel = NUM_GRAYS - 1;
						}
						pixmap[col] = (Uint8) pixel;
					}
				}
			}
		}

		/* Mark that we rendered this format */
		if ( mono ) {
			cached->stored |= CACHED_BITMAP;
		} else {
			cached->stored |= CACHED_PIXMAP;
		}
	}

	/* We're done, mark this glyph cached */
	cached->cached = ch;

	return 0;
}

static FT_Error Find_Glyph( TTF_Font* font, Uint16 ch, int want )
{
	int retval = 0;

	if( ch < 256 ) {
		font->current = &font->cache[ch];
	} else {
		if ( font->scratch.cached != ch ) {
			Flush_Glyph( &font->scratch );
		}
		font->current = &font->scratch;
	}
	if ( (font->current->stored & want) != want ) {
		retval = Load_Glyph( font, ch, font->current, want );
	}
	return retval;
}

void TTF_CloseFont( TTF_Font* font )
{
	Flush_Cache( font );
	FT_Done_Face( font->face );
	free( font );
}

static Uint16 *ASCII_to_UNICODE(Uint16 *unicode, const char *text, int len)
{
	int i;

	for ( i=0; i < len; ++i ) {
		unicode[i] = ((const unsigned char *)text)[i];
	}
	unicode[i] = 0;

	return unicode;
}

static Uint16 *UTF8_to_UNICODE(Uint16 *unicode, const char *utf8, int len)
{
	int i, j;
	Uint16 ch;

	for ( i=0, j=0; i < len; ++i, ++j ) {
		ch = ((const unsigned char *)utf8)[i];
		if ( ch >= 0xF0 ) {
			ch  =  (Uint16)(utf8[i]&0x07) << 18;
			ch |=  (Uint16)(utf8[++i]&0x3F) << 12;
			ch |=  (Uint16)(utf8[++i]&0x3F) << 6;
			ch |=  (Uint16)(utf8[++i]&0x3F);
		} else
		if ( ch >= 0xE0 ) {
			ch  =  (Uint16)(utf8[i]&0x3F) << 12;
			ch |=  (Uint16)(utf8[++i]&0x3F) << 6;
			ch |=  (Uint16)(utf8[++i]&0x3F);
		} else
		if ( ch >= 0xC0 ) {
			ch  =  (Uint16)(utf8[i]&0x3F) << 6;
			ch |=  (Uint16)(utf8[++i]&0x3F);
		}
		unicode[j] = ch;
	}
	unicode[j] = 0;

	return unicode;
}

int TTF_FontHeight(TTF_Font *font)
{
	return(font->height);
}

int TTF_FontAscent(TTF_Font *font)
{
       return(font->ascent);
}

int TTF_FontDescent(TTF_Font *font)
{
	return(font->descent);
}

int TTF_FontLineSkip(TTF_Font *font)
{
	return(font->lineskip);
}

long TTF_FontFaces(TTF_Font *font)
{
	return(font->face->num_faces);
}

int TTF_FontFaceIsFixedWidth(TTF_Font *font)
{
	return(FT_IS_FIXED_WIDTH(font->face));
}

char *TTF_FontFaceFamilyName(TTF_Font *font)
{
	return(font->face->family_name);
}

char *TTF_FontFaceStyleName(TTF_Font *font)
{
	return(font->face->style_name);
}

int TTF_GlyphMetrics(TTF_Font *font, Uint16 ch,
                     int* minx, int* maxx, int* miny, int* maxy, int* advance)
{
	FT_Error error;

	error = Find_Glyph(font, ch, CACHED_METRICS);
	if ( error ) {
		TTF_SetFTError("Couldn't find glyph", error);
		return -1;
	}

	if ( minx ) {
		*minx = font->current->minx;
	}
	if ( maxx ) {
		*maxx = font->current->maxx;
	}
	if ( miny ) {
		*miny = font->current->miny;
	}
	if ( maxy ) {
		*maxy = font->current->maxy;
	}
	if ( advance ) {
		*advance = font->current->advance;
	}
	return 0;
}

int TTF_SizeText(TTF_Font *font, const char *text, int *w, int *h)
{
	Uint16 *unicode_text;
	int unicode_len;
	int status;

	/* Copy the Latin-1 text to a UNICODE text buffer */
	unicode_len = strlen(text);
	unicode_text = (Uint16 *)malloc((unicode_len+1)*(sizeof *unicode_text));
	if ( unicode_text == NULL ) {
		TTF_SetError("Out of memory");
		return -1;
	}
	ASCII_to_UNICODE(unicode_text, text, unicode_len);

	/* Render the new text */
	status = TTF_SizeUNICODE(font, unicode_text, w, h);

	/* Free the text buffer and return */
	free(unicode_text);
	return status;
}

int TTF_SizeUTF8(TTF_Font *font, const char *text, int *w, int *h)
{
	Uint16 *unicode_text;
	int unicode_len;
	int status;

	/* Copy the UTF-8 text to a UNICODE text buffer */
	unicode_len = strlen(text);
	unicode_text = (Uint16 *)malloc((unicode_len+1)*(sizeof *unicode_text));
	if ( unicode_text == NULL ) {
		TTF_SetError("Out of memory");
		return -1;
	}
	UTF8_to_UNICODE(unicode_text, text, unicode_len);

	/* Render the new text */
	status = TTF_SizeUNICODE(font, unicode_text, w, h);

	/* Free the text buffer and return */
	free(unicode_text);
	return status;
}

int TTF_SizeUNICODE(TTF_Font *font, const Uint16 *text, int *w, int *h)
{
	int status;
	const Uint16 *ch;
	int x, z;
	int minx, maxx;
	int miny, maxy;
	c_glyph *glyph;
	FT_Error error;

	/* Initialize everything to 0 */
	if ( ! TTF_initialized ) {
		return -1;
	}
	status = 0;
	minx = maxx = 0;
	miny = maxy = 0;

	/* Load each character and sum it's bounding box */
	x= 0;
	for ( ch=text; *ch; ++ch ) {
		error = Find_Glyph(font, *ch, CACHED_METRICS);
		if ( error ) {
			return -1;
		}
		glyph = font->current;

		z = x + glyph->minx;
		if ( minx > z ) {
			minx = z;
		}
		if ( font->style & TTF_STYLE_BOLD ) {
			x += font->glyph_overhang;
		}
		if ( glyph->advance > glyph->maxx ) {
			z = x + glyph->advance;
		} else {
			z = x + glyph->maxx;
		}
		if ( maxx < z ) {
			maxx = z;
		}
		x += glyph->advance;

		if ( glyph->miny < miny ) {
			miny = glyph->miny;
		}
		if ( glyph->maxy > maxy ) {
			maxy = glyph->maxy;
		}
	}

	/* Fill the bounds rectangle */
	if ( w ) {
		*w = (maxx - minx);
	}
	if ( h ) {
#if 0 /* This is correct, but breaks many applications */
		*h = (maxy - miny);
#else
		*h = font->height;
#endif
	}
	return status;
}

/* Convert the Latin-1 text to UNICODE and render it
*/
SDL_Surface *TTF_RenderText_Solid(TTF_Font *font,
				const char *text, SDL_Color fg)
{
	SDL_Surface *textbuf;
	Uint16 *unicode_text;
	int unicode_len;

	/* Copy the Latin-1 text to a UNICODE text buffer */
	unicode_len = strlen(text);
	unicode_text = (Uint16 *)malloc((unicode_len+1)*(sizeof *unicode_text));
	if ( unicode_text == NULL ) {
		TTF_SetError("Out of memory");
		return(NULL);
	}
	ASCII_to_UNICODE(unicode_text, text, unicode_len);

	/* Render the new text */
	textbuf = TTF_RenderUNICODE_Solid(font, unicode_text, fg);

	/* Free the text buffer and return */
	free(unicode_text);
	return(textbuf);
}

/* Convert the UTF-8 text to UNICODE and render it
*/
SDL_Surface *TTF_RenderUTF8_Solid(TTF_Font *font,
				const char *text, SDL_Color fg)
{
	SDL_Surface *textbuf;
	Uint16 *unicode_text;
	int unicode_len;

	/* Copy the UTF-8 text to a UNICODE text buffer */
	unicode_len = strlen(text);
	unicode_text = (Uint16 *)malloc((unicode_len+1)*(sizeof *unicode_text));
	if ( unicode_text == NULL ) {
		TTF_SetError("Out of memory");
		return(NULL);
	}
	UTF8_to_UNICODE(unicode_text, text, unicode_len);

	/* Render the new text */
	textbuf = TTF_RenderUNICODE_Solid(font, unicode_text, fg);

	/* Free the text buffer and return */
	free(unicode_text);
	return(textbuf);
}

SDL_Surface *TTF_RenderUNICODE_Solid(TTF_Font *font,
				const Uint16 *text, SDL_Color fg)
{
	int xstart;
	int width;
	int height;
	SDL_Surface* textbuf;
	SDL_Palette* palette;
	const Uint16* ch;
	Uint8* src;
	Uint8* dst;
	int row;
	int col;
	c_glyph *glyph;
	FT_Error error;

	/* Get the dimensions of the text surface */
	if( ( TTF_SizeUNICODE(font, text, &width, NULL) < 0 ) || !width ) {
		TTF_SetError( "Text has zero width" );
		return NULL;
	}
	height = font->height;

	/* Create the target surface */
	textbuf = SDL_AllocSurface(SDL_SWSURFACE, width, height, 8, 0, 0, 0, 0);
	if( textbuf == NULL ) {
		return NULL;
	}

	/* Fill the palette with the foreground color */
	palette = textbuf->format->palette;
	palette->colors[0].r = 255 - fg.r;
	palette->colors[0].g = 255 - fg.g;
	palette->colors[0].b = 255 - fg.b;
	palette->colors[1].r = fg.r;
	palette->colors[1].g = fg.g;
	palette->colors[1].b = fg.b;
	SDL_SetColorKey( textbuf, SDL_SRCCOLORKEY, 0 );

	/* Load and render each character */
	xstart = 0;

	for( ch=text; *ch; ++ch ) {
		FT_Bitmap* current = NULL;

		error = Find_Glyph(font, *ch, CACHED_METRICS|CACHED_BITMAP);
		if( error ) {
			SDL_FreeSurface( textbuf );
			return NULL;
		}
		glyph = font->current;

		current = &glyph->bitmap;
		for( row = 0; row < current->rows; ++row ) {
			dst = (Uint8*) textbuf->pixels +
				(row+glyph->yoffset) * textbuf->pitch +
				xstart + glyph->minx;
			src = current->buffer + row * current->pitch;

			for ( col=current->width; col>0; --col ) {
				*dst++ |= *src++;
			}
		}

		xstart += glyph->advance;
		if ( font->style & TTF_STYLE_BOLD ) {
			xstart += font->glyph_overhang;
		}
	}

	/* Handle the underline style */
	if( font->style & TTF_STYLE_UNDERLINE ) {
		row = font->ascent - font->underline_offset - 1;
		if ( row >= textbuf->h) {
			row = (textbuf->h-1) - font->underline_height;
		}
		dst = (Uint8 *)textbuf->pixels + row * textbuf->pitch;
		for ( row=font->underline_height; row>0; --row ) {
			/* 1 because 0 is the bg color */
			memset( dst, 1, textbuf->w );
			dst += textbuf->pitch;
		}
	}
	return textbuf;
}

SDL_Surface *TTF_RenderGlyph_Solid(TTF_Font *font, Uint16 ch, SDL_Color fg)
{
	SDL_Surface *textbuf;
	SDL_Palette *palette;
	Uint8 *src, *dst;
	int row;
	FT_Error error;
	c_glyph *glyph;

	/* Get the glyph itself */
	error = Find_Glyph(font, ch, CACHED_METRICS|CACHED_BITMAP);
	if ( error ) {
		return(NULL);
	}
	glyph = font->current;

	/* Create the target surface */
	textbuf = SDL_CreateRGBSurface( SDL_SWSURFACE,
					glyph->pixmap.width,
					glyph->pixmap.rows,
					8, 0, 0, 0, 0 );
	if ( ! textbuf ) {
		return(NULL);
	}

	/* Fill the palette with the foreground color */
	palette = textbuf->format->palette;
	palette->colors[0].r = 255-fg.r;
	palette->colors[0].g = 255-fg.g;
	palette->colors[0].b = 255-fg.b;
	palette->colors[1].r = fg.r;
	palette->colors[1].g = fg.g;
	palette->colors[1].b = fg.b;
	SDL_SetColorKey(textbuf, SDL_SRCCOLORKEY, 0);

	/* Copy the character from the pixmap */
	src = glyph->pixmap.buffer;
	dst = (Uint8*) textbuf->pixels;
	for ( row = 0; row < textbuf->h; ++row ) {
		memcpy( dst, src, glyph->pixmap.pitch );
		src += glyph->pixmap.pitch;
		dst += textbuf->pitch;
	}

	/* Handle the underline style */
	if( font->style & TTF_STYLE_UNDERLINE ) {
		row = font->ascent - font->underline_offset - 1;
		if ( row >= textbuf->h) {
			row = (textbuf->h-1) - font->underline_height;
		}
		dst = (Uint8 *)textbuf->pixels + row * textbuf->pitch;
		for ( row=font->underline_height; row>0; --row ) {
			/* 1 because 0 is the bg color */
			memset( dst, 1, textbuf->w );
			dst += textbuf->pitch;
		}
	}
	return(textbuf);
}


/* Convert the Latin-1 text to UNICODE and render it
*/
SDL_Surface *TTF_RenderText_Shaded(TTF_Font *font,
				const char *text, SDL_Color fg, SDL_Color bg)
{
	SDL_Surface *textbuf;
	Uint16 *unicode_text;
	int unicode_len;

	/* Copy the Latin-1 text to a UNICODE text buffer */
	unicode_len = strlen(text);
	unicode_text = (Uint16 *)malloc((unicode_len+1)*(sizeof *unicode_text));
	if ( unicode_text == NULL ) {
		TTF_SetError("Out of memory");
		return(NULL);
	}
	ASCII_to_UNICODE(unicode_text, text, unicode_len);

	/* Render the new text */
	textbuf = TTF_RenderUNICODE_Shaded(font, unicode_text, fg, bg);

	/* Free the text buffer and return */
	free(unicode_text);
	return(textbuf);
}

/* Convert the UTF-8 text to UNICODE and render it
*/
SDL_Surface *TTF_RenderUTF8_Shaded(TTF_Font *font,
				const char *text, SDL_Color fg, SDL_Color bg)
{
	SDL_Surface *textbuf;
	Uint16 *unicode_text;
	int unicode_len;

	/* Copy the UTF-8 text to a UNICODE text buffer */
	unicode_len = strlen(text);
	unicode_text = (Uint16 *)malloc((unicode_len+1)*(sizeof *unicode_text));
	if ( unicode_text == NULL ) {
		TTF_SetError("Out of memory");
		return(NULL);
	}
	UTF8_to_UNICODE(unicode_text, text, unicode_len);

	/* Render the new text */
	textbuf = TTF_RenderUNICODE_Shaded(font, unicode_text, fg, bg);

	/* Free the text buffer and return */
	free(unicode_text);
	return(textbuf);
}

SDL_Surface* TTF_RenderUNICODE_Shaded( TTF_Font* font,
				       const Uint16* text,
				       SDL_Color fg,
				       SDL_Color bg )
{
	int xstart;
	int width;
	int height;
	SDL_Surface* textbuf;
	SDL_Palette* palette;
	int index;
	int rdiff;
	int gdiff;
	int bdiff;
	const Uint16* ch;
	Uint8* src;
	Uint8* dst;
	int row, col;
	c_glyph *glyph;
	FT_Error error;

	/* Get the dimensions of the text surface */
	if( ( TTF_SizeUNICODE(font, text, &width, NULL) < 0 ) || !width ) {
		TTF_SetError("Text has zero width");
		return NULL;
	}
	height = font->height;

	/* Create the target surface */
	textbuf = SDL_AllocSurface(SDL_SWSURFACE, width, height, 8, 0, 0, 0, 0);
	if( textbuf == NULL ) {
		return NULL;
	}

	/* Fill the palette with NUM_GRAYS levels of shading from bg to fg */
	palette = textbuf->format->palette;
	rdiff = fg.r - bg.r;
	gdiff = fg.g - bg.g;
	bdiff = fg.b - bg.b;

	for( index = 0; index < NUM_GRAYS; ++index ) {
		palette->colors[index].r = bg.r + (index*rdiff) / (NUM_GRAYS-1);
		palette->colors[index].g = bg.g + (index*gdiff) / (NUM_GRAYS-1);
		palette->colors[index].b = bg.b + (index*bdiff) / (NUM_GRAYS-1);
	}

	/* Load and render each character */
	xstart = 0;
	for( ch = text; *ch; ++ch ) {
		FT_Bitmap* current;

		error = Find_Glyph(font, *ch, CACHED_METRICS|CACHED_PIXMAP);
		if( error ) {
			SDL_FreeSurface( textbuf );
			return NULL;
		}
		glyph = font->current;

		current = &glyph->pixmap;
		for( row = 0; row < current->rows; ++row ) {
			dst = (Uint8*) textbuf->pixels +
				(row+glyph->yoffset) * textbuf->pitch +
				xstart + glyph->minx;
			src = current->buffer + row * current->pitch;
			for ( col=current->width; col>0; --col ) {
				*dst++ |= *src++;
			}
		}

		xstart += glyph->advance;
		if( font->style & TTF_STYLE_BOLD ) {
			xstart += font->glyph_overhang;
		}
	}

	/* Handle the underline style */
	if( font->style & TTF_STYLE_UNDERLINE ) {
		row = font->ascent - font->underline_offset - 1;
		if ( row >= textbuf->h) {
			row = (textbuf->h-1) - font->underline_height;
		}
		dst = (Uint8 *)textbuf->pixels + row * textbuf->pitch;
		for ( row=font->underline_height; row>0; --row ) {
			memset( dst, NUM_GRAYS - 1, textbuf->w );
			dst += textbuf->pitch;
		}
	}
	return textbuf;
}

SDL_Surface* TTF_RenderGlyph_Shaded( TTF_Font* font,
				     Uint16 ch,
				     SDL_Color fg,
				     SDL_Color bg )
{
	SDL_Surface* textbuf;
	SDL_Palette* palette;
	int index;
	int rdiff;
	int gdiff;
	int bdiff;
	Uint8* src;
	Uint8* dst;
	int row;
	FT_Error error;
	c_glyph* glyph;

	/* Get the glyph itself */
	error = Find_Glyph(font, ch, CACHED_METRICS|CACHED_PIXMAP);
	if( error ) {
		return NULL;
	}
	glyph = font->current;

	/* Create the target surface */
	textbuf = SDL_CreateRGBSurface( SDL_SWSURFACE,
					glyph->pixmap.width,
					glyph->pixmap.rows,
					8, 0, 0, 0, 0 );
	if( !textbuf ) {
		return NULL;
	}

	/* Fill the palette with NUM_GRAYS levels of shading from bg to fg */
	palette = textbuf->format->palette;
	rdiff = fg.r - bg.r;
	gdiff = fg.g - bg.g;
	bdiff = fg.b - bg.b;
	for( index = 0; index < NUM_GRAYS; ++index ) {
		palette->colors[index].r = bg.r + (index*rdiff) / (NUM_GRAYS-1);
		palette->colors[index].g = bg.g + (index*gdiff) / (NUM_GRAYS-1);
		palette->colors[index].b = bg.b + (index*bdiff) / (NUM_GRAYS-1);
	}

	/* Copy the character from the pixmap */
	src = glyph->pixmap.buffer;
	dst = (Uint8*) textbuf->pixels;
	for ( row = 0; row < textbuf->h; ++row ) {
		memcpy( dst, src, glyph->pixmap.pitch );
		src += glyph->pixmap.pitch;
		dst += textbuf->pitch;
	}

	/* Handle the underline style */
	if( font->style & TTF_STYLE_UNDERLINE ) {
		row = font->ascent - font->underline_offset - 1;
		if ( row >= textbuf->h) {
			row = (textbuf->h-1) - font->underline_height;
		}
		dst = (Uint8 *)textbuf->pixels + row * textbuf->pitch;
		for ( row=font->underline_height; row>0; --row ) {
			memset( dst, NUM_GRAYS - 1, textbuf->w );
			dst += textbuf->pitch;
		}
	}
	return textbuf;
}

/* Convert the Latin-1 text to UNICODE and render it
*/
SDL_Surface *TTF_RenderText_Blended(TTF_Font *font,
				const char *text, SDL_Color fg)
{
	SDL_Surface *textbuf;
	Uint16 *unicode_text;
	int unicode_len;

	/* Copy the Latin-1 text to a UNICODE text buffer */
	unicode_len = strlen(text);
	unicode_text = (Uint16 *)malloc((unicode_len+1)*(sizeof *unicode_text));
	if ( unicode_text == NULL ) {
		TTF_SetError("Out of memory");
		return(NULL);
	}
	ASCII_to_UNICODE(unicode_text, text, unicode_len);

	/* Render the new text */
	textbuf = TTF_RenderUNICODE_Blended(font, unicode_text, fg);

	/* Free the text buffer and return */
	free(unicode_text);
	return(textbuf);
}

/* Convert the UTF-8 text to UNICODE and render it
*/
SDL_Surface *TTF_RenderUTF8_Blended(TTF_Font *font,
				const char *text, SDL_Color fg)
{
	SDL_Surface *textbuf;
	Uint16 *unicode_text;
	int unicode_len;

	/* Copy the UTF-8 text to a UNICODE text buffer */
	unicode_len = strlen(text);
	unicode_text = (Uint16 *)malloc((unicode_len+1)*(sizeof *unicode_text));
	if ( unicode_text == NULL ) {
		TTF_SetError("Out of memory");
		return(NULL);
	}
	UTF8_to_UNICODE(unicode_text, text, unicode_len);

	/* Render the new text */
	textbuf = TTF_RenderUNICODE_Blended(font, unicode_text, fg);

	/* Free the text buffer and return */
	free(unicode_text);
	return(textbuf);
}

SDL_Surface *TTF_RenderUNICODE_Blended(TTF_Font *font,
				const Uint16 *text, SDL_Color fg)
{
	int xstart;
	int width, height;
	SDL_Surface *textbuf;
	Uint32 alpha;
	Uint32 pixel;
	const Uint16 *ch;
	Uint8 *src;
	Uint32 *dst;
	int row, col;
	c_glyph *glyph;
	FT_Error error;

	/* Get the dimensions of the text surface */
	if ( (TTF_SizeUNICODE(font, text, &width, NULL) < 0) || !width ) {
		TTF_SetError("Text has zero width");
		return(NULL);
	}
	height = font->height;

	textbuf = SDL_AllocSurface(SDL_SWSURFACE, width, height, 32,
                  0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	if ( textbuf == NULL ) {
		return(NULL);
	}

	/* Load and render each character */
	xstart = 0;
	pixel = (fg.r<<16)|(fg.g<<8)|fg.b;
	for ( ch=text; *ch; ++ch ) {
		error = Find_Glyph(font, *ch, CACHED_METRICS|CACHED_PIXMAP);
		if( error ) {
			SDL_FreeSurface( textbuf );
			return NULL;
		}
		glyph = font->current;

		width = glyph->pixmap.width;
		src = (Uint8 *)glyph->pixmap.buffer;
		for ( row = 0; row < glyph->pixmap.rows; ++row ) {
			dst = (Uint32*) textbuf->pixels +
				(row+glyph->yoffset) * textbuf->pitch/4 +
				xstart + glyph->minx;
			for ( col=width; col>0; --col ) {
				alpha = *src++;
				*dst++ |= pixel | (alpha << 24);
			}
		}

		xstart += glyph->advance;
		if ( font->style & TTF_STYLE_BOLD ) {
			xstart += font->glyph_overhang;
		}
	}

	/* Handle the underline style */
	if( font->style & TTF_STYLE_UNDERLINE ) {
		row = font->ascent - font->underline_offset - 1;
		if ( row >= textbuf->h) {
			row = (textbuf->h-1) - font->underline_height;
		}
		dst = (Uint32 *)textbuf->pixels + row * textbuf->pitch/4;
		pixel |= 0xFF000000; /* Amask */
		for ( row=font->underline_height; row>0; --row ) {
			for ( col=0; col < textbuf->w; ++col ) {
				dst[col] = pixel;
			}
			dst += textbuf->pitch/4;
		}
	}
	return(textbuf);
}

SDL_Surface *TTF_RenderGlyph_Blended(TTF_Font *font, Uint16 ch, SDL_Color fg)
{
	SDL_Surface *textbuf;
	Uint32 alpha;
	Uint32 pixel;
	Uint8 *src;
	Uint32 *dst;
	int row, col;
	FT_Error error;
	c_glyph *glyph;

	/* Get the glyph itself */
	error = Find_Glyph(font, ch, CACHED_METRICS|CACHED_PIXMAP);
	if ( error ) {
		return(NULL);
	}
	glyph = font->current;

	textbuf = SDL_CreateRGBSurface(SDL_SWSURFACE,
	              glyph->pixmap.width, glyph->pixmap.rows, 32,
                  0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	if ( ! textbuf ) {
		return(NULL);
	}

	/* Copy the character from the pixmap */
	pixel = (fg.r<<16)|(fg.g<<8)|fg.b;
	for ( row=0; row<textbuf->h; ++row ) {
		src = glyph->pixmap.buffer + row * glyph->pixmap.width;
		dst = (Uint32 *)textbuf->pixels + row * textbuf->pitch/4;
		for ( col=0; col<glyph->pixmap.width; ++col ) {
			alpha = *src++;
			*dst++ = pixel | (alpha << 24);
		}
	}

	/* Handle the underline style */
	if( font->style & TTF_STYLE_UNDERLINE ) {
		row = font->ascent - font->underline_offset - 1;
		if ( row >= textbuf->h) {
			row = (textbuf->h-1) - font->underline_height;
		}
		dst = (Uint32 *)textbuf->pixels + row * textbuf->pitch/4;
		pixel |= 0xFF000000; /* Amask */
		for ( row=font->underline_height; row>0; --row ) {
			for ( col=0; col < textbuf->w; ++col ) {
				dst[col] = pixel;
			}
			dst += textbuf->pitch/4;
		}
	}
	return(textbuf);
}

void TTF_SetFontStyle( TTF_Font* font, int style )
{
	font->style = style;
	Flush_Cache( font );
}

int TTF_GetFontStyle( TTF_Font* font )
{
	return font->style;
}

void TTF_Quit( void )
{
	if ( TTF_initialized ) {
		FT_Done_FreeType( library );
	}
	TTF_initialized = 0;
}
