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
 *  \file SDL_textengine.h
 *
 * Definitions for implementations of the TTF_TextEngine interface.
 */
#ifndef SDL_TTF_TEXTENGINE_H_
#define SDL_TTF_TEXTENGINE_H_

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <SDL3/SDL_begin_code.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* Private data in TTF_Text, available to implementations */
struct TTF_TextData
{
    TTF_TextEngine *engine; /**< The engine used to create this text, read-only. */
    SDL_PropertiesID props; /**< Custom properties associated with this text, read-write. */
    void *textrep;          /**< The implementation-specific representation of this text */
};

/**
 * A font atlas draw command.
 *
 * \since This enum is available since SDL_ttf 3.0.0.
 */
typedef enum TTF_DrawCommand
{
    TTF_DRAW_COMMAND_NOOP,
    TTF_DRAW_COMMAND_FILL,
    TTF_DRAW_COMMAND_COPY
} TTF_DrawCommand;

/**
 * A filled rectangle draw operation.
 *
 * \since This struct is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_DrawOperation
 */
typedef struct TTF_FillOperation
{
    TTF_DrawCommand cmd;    /**< TTF_DRAW_COMMAND_FILL */
    SDL_Rect rect;          /**< The rectangle to fill, in pixels. The x coordinate is relative to the left side of the text area, going right, and the y coordinate is relative to the top side of the text area, going down. */
} TTF_FillOperation;

/**
 * A texture copy draw operation.
 *
 * \since This struct is available since SDL_ttf 3.0.0.
 *
 * \sa TTF_DrawOperation
 */
typedef struct TTF_CopyOperation
{
    TTF_DrawCommand cmd;    /**< TTF_DRAW_COMMAND_COPY */
    Uint32 glyph_index;     /**< The glyph index of the glyph to be drawn, can be passed to TTF_GetGlyphForIndex() */
    SDL_Rect src;           /**< The area within the glyph to be drawn */
    SDL_Rect dst;           /**< The drawing coordinates of the glyph, in pixels. The x coordinate is relative to the left side of the text area, going right, and the y coordinate is relative to the top side of the text area, going down. */
    void *reserved;
} TTF_CopyOperation;

/**
 * A text engine draw operation.
 *
 * \since This struct is available since SDL_ttf 3.0.0.
 */
typedef union TTF_DrawOperation
{
    TTF_DrawCommand cmd;
    TTF_FillOperation fill;
    TTF_CopyOperation copy;
} TTF_DrawOperation;

/**
 * A text engine interface.
 *
 * This structure should be initialized using SDL_INIT_INTERFACE()
 *
 * \since This struct is available since SDL_ttf 3.0.0.
 *
 * \sa SDL_INIT_INTERFACE
 */
struct TTF_TextEngine
{
    Uint32 version;     /**< The version of this interface */

    void *userdata;     /**< User data pointer passed to callbacks */

    /* Create a text representation from draw instructions.
     *
     * All fields of `text` except `internal` will already be filled out.
     *
     * \param userdata the userdata pointer in this interface.
     * \param font the font being used.
     * \param font_generation the unique ID of the font generation being used. This changes whenever the font changes size or style and needs new glyphs, and is unique across all fonts.
     * \param ops the text drawing operations
     * \param num_ops the number of text drawing operations
     */
    bool (SDLCALL *CreateText)(void *userdata, TTF_Font *font, Uint32 font_generation, TTF_Text *text, TTF_DrawOperation *ops, int num_ops);

    /**
     * Destroy a text representation.
     */
    void (SDLCALL *DestroyText)(void *userdata, TTF_Text *text);

};

/* Check the size of TTF_TextEngine
 *
 * If this assert fails, either the compiler is padding to an unexpected size,
 * or the interface has been updated and this should be updated to match and
 * the code using this interface should be updated to handle the old version.
 */
SDL_COMPILE_TIME_ASSERT(TTF_TextEngine_SIZE,
    (sizeof(void *) == 4 && sizeof(TTF_TextEngine) == 16) ||
    (sizeof(void *) == 8 && sizeof(TTF_TextEngine) == 32));


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_TTF_TEXTENGINE_H_ */

