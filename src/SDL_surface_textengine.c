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
#include <SDL3_ttf/SDL_textengine.h>

#include "SDL_hashtable.h"


typedef struct TTF_SurfaceTextEngineGlyphData
{
    int refcount;
    SDL_Color color;
    SDL_Surface *surface;
} TTF_SurfaceTextEngineGlyphData;

typedef struct TTF_SurfaceTextEngineTextData
{
    SDL_FColor fcolor;
    SDL_Color color;
    TTF_DrawOperation *ops;
    int num_ops;
} TTF_SurfaceTextEngineTextData;

typedef struct TTF_SurfaceTextEngineFontData
{
    TTF_Font *font;
    Uint32 generation;
    SDL_HashTable *glyphs;
} TTF_SurfaceTextEngineFontData;

typedef struct TTF_SurfaceTextEngineData
{
    SDL_HashTable *fonts;
} TTF_SurfaceTextEngineData;


static void DestroyGlyphData(TTF_SurfaceTextEngineGlyphData *data)
{
    if (!data) {
        return;
    }

    --data->refcount;
    if (data->refcount == 0) {
        if (data->surface) {
            SDL_DestroySurface(data->surface);
        }
        SDL_free(data);
    }
}

static TTF_SurfaceTextEngineGlyphData *CreateGlyphData(SDL_Surface *surface)
{
    TTF_SurfaceTextEngineGlyphData *data = (TTF_SurfaceTextEngineGlyphData *)SDL_malloc(sizeof(*data));
    if (data) {
        data->refcount = 1;
        data->color.r = 0xFF;
        data->color.g = 0xFF;
        data->color.b = 0xFF;
        data->color.a = 0xFF;
        data->surface = surface;
    }
    return data;
}

static TTF_SurfaceTextEngineGlyphData *GetGlyphData(TTF_SurfaceTextEngineFontData *fontdata, Uint32 idx)
{
    TTF_SurfaceTextEngineGlyphData *data;

    if (!SDL_FindInHashTable(fontdata->glyphs, (const void *)(uintptr_t)idx, (const void **)&data)) {
        SDL_Surface *surface = TTF_GetGlyphImageForIndex(fontdata->font, idx);
        if (!surface) {
            return NULL;
        }

        data = CreateGlyphData(surface);
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

static void DestroyTextData(TTF_SurfaceTextEngineTextData *data)
{
    if (!data) {
        return;
    }

    if (data->ops) {
        for (int i = 0; i < data->num_ops; ++i) {
            const TTF_DrawOperation *op = &data->ops[i];
            if (op->cmd == TTF_DRAW_COMMAND_COPY) {
                TTF_SurfaceTextEngineGlyphData *glyph = (TTF_SurfaceTextEngineGlyphData *)op->copy.reserved;
                DestroyGlyphData(glyph);
            }
        }
        SDL_free(data->ops);
    }
    SDL_free(data);
}

static TTF_SurfaceTextEngineTextData *CreateTextData(TTF_SurfaceTextEngineFontData *fontdata, const TTF_DrawOperation *ops, int num_ops)
{
    TTF_SurfaceTextEngineTextData *data = (TTF_SurfaceTextEngineTextData *)SDL_calloc(1, sizeof(*data));
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
            TTF_SurfaceTextEngineGlyphData *glyph = GetGlyphData(fontdata, op->copy.glyph_index);
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

static void DestroyFontData(TTF_SurfaceTextEngineFontData *data)
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
    TTF_SurfaceTextEngineGlyphData *data = (TTF_SurfaceTextEngineGlyphData *)value;
    (void)key;
    (void)unused;
    DestroyGlyphData(data);
}

static TTF_SurfaceTextEngineFontData *CreateFontData(TTF_SurfaceTextEngineData *enginedata, TTF_Font *font, Uint32 font_generation)
{
    TTF_SurfaceTextEngineFontData *data = (TTF_SurfaceTextEngineFontData *)SDL_calloc(1, sizeof(*data));
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

static void DestroyEngineData(TTF_SurfaceTextEngineData *data)
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
    TTF_SurfaceTextEngineFontData *data = (TTF_SurfaceTextEngineFontData *)value;
    (void)key;
    (void)unused;
    DestroyFontData(data);
}

static TTF_SurfaceTextEngineData *CreateEngineData(void)
{
    TTF_SurfaceTextEngineData *data = (TTF_SurfaceTextEngineData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return NULL;
    }

    data->fonts = SDL_CreateHashTable(NULL, 4, SDL_HashPointer, SDL_KeyMatchPointer, NukeFontData, false);
    if (!data->fonts) {
        DestroyEngineData(data);
        return NULL;
    }
    return data;
}

static bool SDLCALL CreateText(void *userdata, TTF_Text *text)
{
    TTF_Font *font = text->internal->font;
    Uint32 font_generation = TTF_GetFontGeneration(font);
    int num_ops = text->internal->num_ops;
    const TTF_DrawOperation *ops = text->internal->ops;
    TTF_SurfaceTextEngineData *enginedata = (TTF_SurfaceTextEngineData *)userdata;
    TTF_SurfaceTextEngineFontData *fontdata;
    TTF_SurfaceTextEngineTextData *data;

    if (!SDL_FindInHashTable(enginedata->fonts, font, (const void **)&fontdata)) {
        fontdata = CreateFontData(enginedata, font, font_generation);
        if (!fontdata) {
            return false;
        }
    } else if (font_generation != fontdata->generation) {
        SDL_EmptyHashTable(fontdata->glyphs);
        fontdata->generation = font_generation;
    }

    data = CreateTextData(fontdata, ops, num_ops);
    if (!data) {
        return false;
    }
    text->internal->engine_text = data;
    return true;
}

static void SDLCALL DestroyText(void *userdata, TTF_Text *text)
{
    TTF_SurfaceTextEngineTextData *data = (TTF_SurfaceTextEngineTextData *)text->internal->engine_text;

    (void)userdata;
    DestroyTextData(data);
}

TTF_TextEngine *TTF_CreateSurfaceTextEngine(void)
{
    TTF_TextEngine *engine = (TTF_TextEngine *)SDL_malloc(sizeof(*engine));
    if (!engine) {
        return NULL;
    }

    SDL_INIT_INTERFACE(engine);
    engine->CreateText = CreateText;
    engine->DestroyText = DestroyText;
    engine->userdata = CreateEngineData();
    if (!engine->userdata) {
        TTF_DestroySurfaceTextEngine(engine);
        return NULL;
    }
    return engine;
}

static void UpdateColor(TTF_SurfaceTextEngineTextData *data, const SDL_FColor *color)
{
    data->color.r = (Uint8)SDL_roundf(SDL_clamp(color->r, 0.0f, 1.0f) * 255.0f);
    data->color.g = (Uint8)SDL_roundf(SDL_clamp(color->g, 0.0f, 1.0f) * 255.0f);
    data->color.b = (Uint8)SDL_roundf(SDL_clamp(color->b, 0.0f, 1.0f) * 255.0f);
    data->color.a = (Uint8)SDL_roundf(SDL_clamp(color->a, 0.0f, 1.0f) * 255.0f);
    SDL_copyp(&data->fcolor, color);
}

static void DrawFill(TTF_SurfaceTextEngineTextData *data, const TTF_FillOperation *op, int x, int y, SDL_Surface *surface)
{
    Uint32 color = SDL_MapSurfaceRGBA(surface, data->color.r, data->color.g, data->color.b, data->color.a);

    SDL_Rect dst;
    SDL_copyp(&dst, &op->rect);
    dst.x += x;
    dst.y += y;
    SDL_FillSurfaceRect(surface, &dst, color);
}

static void DrawCopy(TTF_SurfaceTextEngineTextData *data, const TTF_CopyOperation *op, int x, int y, SDL_Surface *surface)
{
    TTF_SurfaceTextEngineGlyphData *glyph = (TTF_SurfaceTextEngineGlyphData *)op->reserved;

    if (data->color.r != glyph->color.r ||
        data->color.g != glyph->color.g ||
        data->color.b != glyph->color.b ||
        data->color.a != glyph->color.a) {
        SDL_SetSurfaceColorMod(glyph->surface, data->color.r, data->color.g, data->color.b);
        SDL_SetSurfaceAlphaMod(glyph->surface, data->color.a);
        SDL_copyp(&glyph->color, &data->color);
    }

    SDL_Rect dst;
    SDL_copyp(&dst, &op->dst);
    dst.x += x;
    dst.y += y;
    SDL_BlitSurface(glyph->surface, &op->src, surface, &dst);
}

bool TTF_DrawSurfaceText(TTF_Text *text, int x, int y, SDL_Surface *surface)
{
    if (!text || !text->internal || text->internal->engine->CreateText != CreateText) {
        return SDL_InvalidParamError("text");
    }
    if (!surface) {
        return SDL_InvalidParamError("surface");
    }

    // Make sure the text is up to date
    if (!TTF_UpdateText(text)) {
        return false;
    }

    TTF_SurfaceTextEngineTextData *data = (TTF_SurfaceTextEngineTextData *)text->internal->engine_text;
    if (!data) {
        // Empty string, nothing to do
        return true;
    }

    if (text->internal->color.r != data->fcolor.r ||
        text->internal->color.g != data->fcolor.g ||
        text->internal->color.b != data->fcolor.b ||
        text->internal->color.a != data->fcolor.a) {
        UpdateColor(data, &text->internal->color);
    }

    for (int i = 0; i < data->num_ops; ++i) {
        const TTF_DrawOperation *op = &data->ops[i];
        switch (op->cmd) {
        case TTF_DRAW_COMMAND_FILL:
            DrawFill(data, &op->fill, x, y, surface);
            break;
        case TTF_DRAW_COMMAND_COPY:
            DrawCopy(data, &op->copy, x, y, surface);
            break;
        default:
            break;
        }
    }
    return true;
}

void TTF_DestroySurfaceTextEngine(TTF_TextEngine *engine)
{
    if (!engine || engine->CreateText != CreateText) {
        return;
    }

    DestroyEngineData((TTF_SurfaceTextEngineData *)engine->userdata);
    engine->CreateText = NULL;
    SDL_free(engine);
}

