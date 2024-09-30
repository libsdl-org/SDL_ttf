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
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_ttf/SDL_textengine.h>

#include "SDL_hashtable.h"

/* Note: This is a naive implementation using one texture per glyph.
 *       We can easily turn this into a texture atlas implementation later.
 */

typedef struct TTF_RendererTextEngineGlyphData
{
    int refcount;
    SDL_FColor color;
    SDL_Texture *texture;
} TTF_RendererTextEngineGlyphData;

typedef struct TTF_RendererTextEngineTextData
{
    TTF_DrawOperation *ops;
    int num_ops;
} TTF_RendererTextEngineTextData;

typedef struct TTF_RendererTextEngineFontData
{
    TTF_Font *font;
    Uint32 generation;
    SDL_HashTable *glyphs;
} TTF_RendererTextEngineFontData;

typedef struct TTF_RendererTextEngineData
{
    SDL_Renderer *renderer;
    SDL_HashTable *fonts;
} TTF_RendererTextEngineData;


static void DestroyGlyphData(TTF_RendererTextEngineGlyphData *data)
{
    if (!data) {
        return;
    }

    --data->refcount;
    if (data->refcount == 0) {
        if (data->texture) {
            SDL_DestroyTexture(data->texture);
        }
        SDL_free(data);
    }
}

static TTF_RendererTextEngineGlyphData *CreateGlyphData(SDL_Texture *texture)
{
    TTF_RendererTextEngineGlyphData *data = (TTF_RendererTextEngineGlyphData *)SDL_malloc(sizeof(*data));
    if (data) {
        data->refcount = 1;
        data->color.r = 1.0f;
        data->color.g = 1.0f;
        data->color.b = 1.0f;
        data->color.a = 1.0f;
        data->texture = texture;
    }
    return data;
}

static TTF_RendererTextEngineGlyphData *GetGlyphData(SDL_Renderer *renderer, TTF_RendererTextEngineFontData *fontdata, Uint32 idx)
{
    TTF_RendererTextEngineGlyphData *data;

    if (!SDL_FindInHashTable(fontdata->glyphs, (const void *)(uintptr_t)idx, (const void **)&data)) {
        SDL_Texture *texture;
        SDL_Surface *surface = TTF_GetGlyphImageForIndex(fontdata->font, idx);
        if (!surface) {
            return NULL;
        }

        texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_DestroySurface(surface);
        if (!texture) {
            return NULL;
        }

        data = CreateGlyphData(texture);
        if (!data) {
            return NULL;
        }

        if (!SDL_InsertIntoHashTable(fontdata->glyphs, (const void *)(uintptr_t)idx, data)) {
            DestroyGlyphData(data);
            return NULL;
        }
    }
    return data;
}

static void DestroyTextData(TTF_RendererTextEngineTextData *data)
{
    if (!data) {
        return;
    }

    if (data->ops) {
        int i;

        for (i = 0; i < data->num_ops; ++i) {
            const TTF_DrawOperation *op = &data->ops[i];
            if (op->cmd == TTF_DRAW_COMMAND_COPY) {
                TTF_RendererTextEngineGlyphData *glyph = (TTF_RendererTextEngineGlyphData *)op->copy.reserved;
                DestroyGlyphData(glyph);
            }
        }
        SDL_free(data->ops);
    }
    SDL_free(data);
}

static TTF_RendererTextEngineTextData *CreateTextData(SDL_Renderer *renderer, TTF_RendererTextEngineFontData *fontdata, const TTF_DrawOperation *ops, int num_ops)
{
    TTF_RendererTextEngineTextData *data = (TTF_RendererTextEngineTextData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return NULL;
    }

    data->ops = (TTF_DrawOperation *)SDL_malloc(num_ops * sizeof(*data->ops));
    if (!data->ops) {
        DestroyTextData(data);
        return NULL;
    }
    SDL_memcpy(data->ops, ops, num_ops * sizeof(*data->ops));
    data->num_ops = num_ops;

    for (int i = 0; i < data->num_ops; ++i) {
        TTF_DrawOperation *op = &data->ops[i];
        if (op->cmd == TTF_DRAW_COMMAND_COPY) {
            TTF_RendererTextEngineGlyphData *glyph = GetGlyphData(renderer, fontdata, op->copy.glyph_index);
            if (!glyph) {
                DestroyTextData(data);
                return NULL;
            }
            ++glyph->refcount;
            op->copy.reserved = glyph;
        }
    }
    return data;
}

static void DestroyFontData(TTF_RendererTextEngineFontData *data)
{
    if (!data) {
        return;
    }

    if (data->glyphs) {
        SDL_DestroyHashTable(data->glyphs);
    }
    SDL_free(data);
}

static void NukeGlyphData(const void *key, const void *value, void *unused)
{
    TTF_RendererTextEngineGlyphData *data = (TTF_RendererTextEngineGlyphData *)value;
    (void)key;
    (void)unused;
    DestroyGlyphData(data);
}

static TTF_RendererTextEngineFontData *CreateFontData(TTF_RendererTextEngineData *enginedata, TTF_Font *font, Uint32 font_generation)
{
    TTF_RendererTextEngineFontData *data = (TTF_RendererTextEngineFontData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return NULL;
    }
    data->font = font;
    data->generation = font_generation;
    data->glyphs = SDL_CreateHashTable(NULL, 4, SDL_HashID, SDL_KeyMatchID, NukeGlyphData, false);
    if (!data->glyphs) {
        DestroyFontData(data);
        return NULL;
    }

    if (!SDL_InsertIntoHashTable(enginedata->fonts, font, data)) {
        DestroyFontData(data);
        return NULL;
    }

    return data;
}

static void DestroyEngineData(TTF_RendererTextEngineData *data)
{
    if (!data) {
        return;
    }

    if (data->fonts) {
        SDL_DestroyHashTable(data->fonts);
    }
    SDL_free(data);
}

static void NukeFontData(const void *key, const void *value, void *unused)
{
    TTF_RendererTextEngineFontData *data = (TTF_RendererTextEngineFontData *)value;
    (void)key;
    (void)unused;
    DestroyFontData(data);
}

static TTF_RendererTextEngineData *CreateEngineData(SDL_Renderer *renderer)
{
    TTF_RendererTextEngineData *data = (TTF_RendererTextEngineData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return NULL;
    }
    data->renderer = renderer;

    data->fonts = SDL_CreateHashTable(NULL, 4, SDL_HashPointer, SDL_KeyMatchPointer, NukeFontData, false);
    if (!data->fonts) {
        DestroyEngineData(data);
        return NULL;
    }
    return data;
}

static bool SDLCALL CreateText(void *userdata, TTF_Font *font, Uint32 font_generation, TTF_Text *text, TTF_DrawOperation *ops, int num_ops)
{
    TTF_RendererTextEngineData *enginedata = (TTF_RendererTextEngineData *)userdata;
    TTF_RendererTextEngineFontData *fontdata;
    TTF_RendererTextEngineTextData *data;

    if (!SDL_FindInHashTable(enginedata->fonts, font, (const void **)&fontdata)) {
        fontdata = CreateFontData(enginedata, font, font_generation);
        if (!fontdata) {
            return false;
        }
    } else if (font_generation != fontdata->generation) {
        SDL_EmptyHashTable(fontdata->glyphs);
        fontdata->generation = font_generation;
    }

    data = CreateTextData(enginedata->renderer, fontdata, ops, num_ops);
    if (!data) {
        return false;
    }
    text->internal = data;
    return true;
}

static void SDLCALL DestroyText(void *userdata, TTF_Text *text)
{
    TTF_RendererTextEngineTextData *data = (TTF_RendererTextEngineTextData *)text->internal;

    (void)userdata;
    DestroyTextData(data);
}

TTF_TextEngine *TTF_CreateRendererTextEngine(SDL_Renderer *renderer)
{
    TTF_TextEngine *engine = (TTF_TextEngine *)SDL_malloc(sizeof(*engine));
    if (!engine) {
        return NULL;
    }

    SDL_INIT_INTERFACE(engine);
    engine->CreateText = CreateText;
    engine->DestroyText = DestroyText;
    engine->userdata = CreateEngineData(renderer);
    if (!engine->userdata) {
        TTF_DestroyRendererTextEngine(engine);
        return NULL;
    }
    return engine;
}

static void DrawFill(SDL_Renderer *renderer, TTF_Text *text, const TTF_FillOperation *op, float x, float y)
{
    SDL_FColor color;
    SDL_GetRenderDrawColorFloat(renderer, &color.r, &color.g, &color.b, &color.a);
    SDL_SetRenderDrawColorFloat(renderer, text->color.r, text->color.g, text->color.b, text->color.a);

    SDL_FRect dst;
    SDL_RectToFRect(&op->rect, &dst);
    dst.x += x;
    dst.y += y;
    SDL_RenderFillRect(renderer, &dst);

    SDL_SetRenderDrawColorFloat(renderer, color.r, color.g, color.b, color.a);
}

static void DrawCopy(SDL_Renderer *renderer, TTF_Text *text, const TTF_CopyOperation *op, float x, float y)
{
    TTF_RendererTextEngineGlyphData *glyph = (TTF_RendererTextEngineGlyphData *)op->reserved;

    if (text->color.r != glyph->color.r ||
        text->color.g != glyph->color.g ||
        text->color.b != glyph->color.b ||
        text->color.a != glyph->color.a) {
        SDL_SetTextureColorModFloat(glyph->texture, text->color.r, text->color.g, text->color.b);
        SDL_SetTextureAlphaModFloat(glyph->texture, text->color.a);
        SDL_copyp(&glyph->color, &text->color);
    }

    SDL_FRect src, dst;
    SDL_RectToFRect(&op->src, &src);
    SDL_RectToFRect(&op->dst, &dst);
    dst.x += x;
    dst.y += y;
    SDL_RenderTexture(renderer, glyph->texture, &src, &dst);
}

bool TTF_DrawRendererText(TTF_Text *text, float x, float y)
{
    TTF_RendererTextEngineTextData *data;
    SDL_Renderer *renderer;

    if (!text || !text->engine || text->engine->CreateText != CreateText) {
        return SDL_InvalidParamError("text");
    }

    renderer = ((TTF_RendererTextEngineData *)text->engine->userdata)->renderer;
    data = (TTF_RendererTextEngineTextData *)text->internal;

    for (int i = 0; i < data->num_ops; ++i) {
        const TTF_DrawOperation *op = &data->ops[i];
        switch (op->cmd) {
        case TTF_DRAW_COMMAND_FILL:
            DrawFill(renderer, text, &op->fill, x, y);
            break;
        case TTF_DRAW_COMMAND_COPY:
            DrawCopy(renderer, text, &op->copy, x, y);
            break;
        default:
            break;
        }
    }
    return false;
}

void TTF_DestroyRendererTextEngine(TTF_TextEngine *engine)
{
    if (!engine || engine->CreateText != CreateText) {
        return;
    }

    DestroyEngineData((TTF_RendererTextEngineData *)engine->userdata);
    engine->CreateText = NULL;
    SDL_free(engine);
}

