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

#define ATLAS_TEXTURE_SIZE  256

#define STB_RECT_PACK_IMPLEMENTATION
#define STBRP_STATIC
#define STBRP_SORT SDL_qsort
#define STBRP_ASSERT SDL_assert
#define STBRP__CDECL SDLCALL
#include "stb_rect_pack.h"

typedef struct AtlasGlyph AtlasGlyph;
typedef struct AtlasTexture AtlasTexture;
typedef struct AtlasDrawSequence AtlasDrawSequence;

struct AtlasGlyph
{
    int refcount;
    AtlasTexture *atlas;
    SDL_Rect rect;
    float texcoords[8];
    AtlasGlyph *next;
};

struct AtlasTexture
{
    SDL_Texture *texture;
    stbrp_context packer;
    stbrp_node *packing_nodes;
    AtlasGlyph *free_glyphs;
    AtlasTexture *next;
};

struct AtlasDrawSequence
{
    SDL_Texture *texture;
    int num_rects;
    SDL_Rect *rects;
    float *texcoords;
    float *positions;
    int *indices;
    AtlasDrawSequence *next;
};

typedef struct TTF_RendererTextEngineTextData
{
    int num_glyphs;
    AtlasGlyph **glyphs;
    AtlasDrawSequence *draw_sequence;
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
    AtlasTexture *atlas;
} TTF_RendererTextEngineData;


static int SDLCALL SortMissing(void *userdata, const void *a, const void *b)
{
    const TTF_DrawOperation *ops = (const TTF_DrawOperation *)userdata;
    const stbrp_rect *A = (const stbrp_rect *)a;
    const stbrp_rect *B = (const stbrp_rect *)b;

    // Sort missing first
    if (!ops[A->id].copy.reserved) {
        if (ops[B->id].copy.reserved) {
            return -1;
        }
    }
    if (!ops[B->id].copy.reserved) {
        if (ops[A->id].copy.reserved) {
            return 1;
        }
    }

    // Sort largest first
    if (A->w != B->w) {
        if (A->w > B->w) {
            return -1;
        } else {
            return 1;
        }
    }
    if (A->h != B->h) {
        if (A->h > B->h) {
            return -1;
        } else {
            return 1;
        }
    }

    // It doesn't matter, sort by ID
    if (A->id < B->id) {
        return -1;
    } else {
        return 1;
    }
}

static int SDLCALL SortOperations(const void *a, const void *b)
{
    const TTF_DrawOperation *A = (const TTF_DrawOperation *)a;
    const TTF_DrawOperation *B = (const TTF_DrawOperation *)b;

    if (A->cmd == TTF_DRAW_COMMAND_COPY &&
        B->cmd == TTF_DRAW_COMMAND_COPY) {
        AtlasGlyph *glyphA = (AtlasGlyph *)A->copy.reserved;
        AtlasGlyph *glyphB = (AtlasGlyph *)B->copy.reserved;
        if (glyphA->atlas != glyphB->atlas) {
            // It's not important how we sort this, just that it's consistent
            return (glyphA->atlas < glyphB->atlas) ? -1 : 1;
        }

        // We could sort by texture coordinate or whatever, if we cared.
        return 0;
    }

    if (A->cmd == TTF_DRAW_COMMAND_COPY) {
        return -1;
    }
    if (B->cmd == TTF_DRAW_COMMAND_COPY) {
        return 1;
    }
    return 0;
}

static void DestroyGlyph(AtlasGlyph* glyph)
{
    if (!glyph) {
        return;
    }

    SDL_free(glyph);
}

static void DestroyAtlas(AtlasTexture *atlas)
{
    if (!atlas) {
        return;
    }

    AtlasGlyph *next;
    for (AtlasGlyph *glyph = atlas->free_glyphs; glyph; glyph = next) {
        next = glyph->next;
        DestroyGlyph(glyph);
    }

    SDL_DestroyTexture(atlas->texture);
    SDL_free(atlas->packing_nodes);
    SDL_free(atlas);
}

static AtlasTexture *CreateAtlas(SDL_Renderer *renderer)
{
    AtlasTexture *atlas = (AtlasTexture *)SDL_calloc(1, sizeof(*atlas));
    if (!atlas) {
        return NULL;
    }

    atlas->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ATLAS_TEXTURE_SIZE, ATLAS_TEXTURE_SIZE);
    if (!atlas->texture) {
        DestroyAtlas(atlas);
        return NULL;
    }
    SDL_SetTextureScaleMode(atlas->texture, SDL_SCALEMODE_NEAREST);

    int num_nodes = ATLAS_TEXTURE_SIZE / 4;
    atlas->packing_nodes = (stbrp_node *)SDL_calloc(num_nodes, sizeof(*atlas->packing_nodes));
    if (!atlas->packing_nodes) {
        DestroyAtlas(atlas);
        return NULL;
    }
    stbrp_init_target(&atlas->packer, ATLAS_TEXTURE_SIZE, ATLAS_TEXTURE_SIZE, atlas->packing_nodes, num_nodes);
    stbrp_setup_heuristic(&atlas->packer, STBRP_HEURISTIC_Skyline_default);

    return atlas;
}

static void ReleaseGlyph(AtlasGlyph *glyph)
{
    if (!glyph) {
        return;
    }

    --glyph->refcount;
    if (glyph->refcount == 0) {
        if (glyph->atlas) {
            // Insert into free list sorted smallest first
            AtlasGlyph *entry, *prev = NULL;
            int size = (glyph->rect.w * glyph->rect.h);
            for (entry = glyph->atlas->free_glyphs; entry; entry = entry->next) {
                if (size <= (entry->rect.w * entry->rect.h)) {
                    break;
                }

                prev = entry;
            }

            if (prev) {
                prev->next = glyph;
            } else {
                glyph->atlas->free_glyphs = glyph;
            }
            glyph->next = entry;
        } else {
            DestroyGlyph(glyph);
        }
    }
}

static AtlasGlyph *CreateGlyph(AtlasTexture *atlas, const stbrp_rect *area)
{
    AtlasGlyph *glyph = (AtlasGlyph *)SDL_calloc(1, sizeof(*glyph));
    if (!glyph) {
        return NULL;
    }

    glyph->refcount = 1;
    glyph->atlas = atlas;
    glyph->rect.x = area->x;
    glyph->rect.y = area->y;
    glyph->rect.w = area->w;
    glyph->rect.h = area->h;

    const float minu = (float)area->x / ATLAS_TEXTURE_SIZE;
    const float minv = (float)area->y / ATLAS_TEXTURE_SIZE;
    const float maxu = (float)(area->x + area->w) / ATLAS_TEXTURE_SIZE;
    const float maxv = (float)(area->y + area->h) / ATLAS_TEXTURE_SIZE;
    glyph->texcoords[0] = minu;
    glyph->texcoords[1] = minv;
    glyph->texcoords[2] = maxu;
    glyph->texcoords[3] = minv;
    glyph->texcoords[4] = maxu;
    glyph->texcoords[5] = maxv;
    glyph->texcoords[6] = minu;
    glyph->texcoords[7] = maxv;

    return glyph;
}

static AtlasGlyph *FindUnusedGlyph(AtlasTexture *atlas, int width, int height)
{
    AtlasGlyph *glyph, *prev = NULL;

    int size = (width * height);
    for (glyph = atlas->free_glyphs; glyph; glyph = glyph->next) {
        if (width == glyph->rect.w && height == glyph->rect.h) {
            if (prev) {
                prev->next = glyph->next;
            } else {
                atlas->free_glyphs = glyph->next;
            }
            ++glyph->refcount;
            return glyph;
        }

        if (size < (glyph->rect.w * glyph->rect.h)) {
            // We didn't find any entries our size, everything else is larger than we want
            break;
        }

        prev = glyph;
    }

    if (atlas->next) {
        return FindUnusedGlyph(atlas->next, width, height);
    }
    return NULL;
}

static bool UpdateGlyph(AtlasGlyph *glyph, SDL_Surface *surface)
{
    SDL_Texture *texture = glyph->atlas->texture;
    void *pixels;
    int pitch;
    if (!SDL_LockTexture(texture, &glyph->rect, &pixels, &pitch)) {
        return false;
    }

    const Uint8 *src = (const Uint8 *)surface->pixels;
    const int src_pitch = surface->pitch;
    Uint8 *dst = (Uint8 *)pixels;
    const int dst_pitch = pitch;
    for (int i = 0; i < glyph->rect.h; ++i) {
        SDL_memcpy(dst, src, glyph->rect.w * 4);
        src += src_pitch;
        dst += dst_pitch;
    }
    SDL_UnlockTexture(texture);
    return true;
}

static bool AddGlyphToFont(TTF_RendererTextEngineFontData *fontdata, Uint32 glyph_index, AtlasGlyph *glyph)
{
    if (!SDL_InsertIntoHashTable(fontdata->glyphs, (const void *)(uintptr_t)glyph_index, glyph)) {
        return false;
    }
    return true;
}

static bool ResolveMissingGlyphs(TTF_RendererTextEngineData *enginedata, AtlasTexture *atlas, TTF_RendererTextEngineFontData *fontdata, SDL_Surface **surfaces, TTF_DrawOperation *ops, int num_ops, stbrp_rect *missing, int num_missing)
{
    // See if we can reuse any existing entries
    if (atlas->free_glyphs) {
        // Search from the smallest to the largest to minimize time spent searching the free list and shortening the missing entries
        for (int i = num_missing; i--; ) {
            AtlasGlyph *glyph = FindUnusedGlyph(atlas, missing[i].w, missing[i].h);
            if (!glyph) {
                continue;
            }

            if (!UpdateGlyph(glyph, surfaces[missing[i].id])) {
                ReleaseGlyph(glyph);
                return false;
            }

            if (!AddGlyphToFont(fontdata, ops[missing[i].id].copy.glyph_index, glyph)) {
                ReleaseGlyph(glyph);
                return false;
            }

            ops[missing[i].id].copy.reserved = glyph;

            // Remove this from the missing entries
            --num_missing;
            if (i < num_missing) {
                SDL_memcpy(&missing[i], &missing[i+1], (num_missing - i) * sizeof(missing[i]));
            }
        }
        if (num_missing == 0) {
            return true;
        }
    }

    // Try to pack all the missing glyphs into the current atlas
    bool all_packed = (stbrp_pack_rects(&atlas->packer, missing, num_missing) == 1);

    for (int i = 0; i < num_missing; ++i) {
        if (!missing[i].was_packed) {
            continue;
        }

        AtlasGlyph *glyph = CreateGlyph(atlas, &missing[i]);
        if (!glyph) {
            return false;
        }

        if (!UpdateGlyph(glyph, surfaces[missing[i].id])) {
            ReleaseGlyph(glyph);
            return false;
        }

        if (!AddGlyphToFont(fontdata, ops[missing[i].id].copy.glyph_index, glyph)) {
            ReleaseGlyph(glyph);
            return false;
        }

        ops[missing[i].id].copy.reserved = glyph;
    }

    if (all_packed) {
        return true;
    }

    // Sort the remaining missing glyphs and try in the next atlas
    SDL_qsort_r(missing, num_missing, sizeof(*missing), SortMissing, ops);
    for (int i = 0; i < num_missing; ++i) {
        if (ops[missing[i].id].copy.reserved) {
            // No longer missing!
            num_missing = i;
            break;
        }
    }

    if (!atlas->next) {
        atlas->next = CreateAtlas(enginedata->renderer);
        if (!atlas->next) {
            return false;
        }
    }
    return ResolveMissingGlyphs(enginedata, atlas->next, fontdata, surfaces, ops, num_ops, missing, num_missing);
}

static bool CreateMissingGlyphs(TTF_RendererTextEngineData *enginedata, TTF_RendererTextEngineFontData *fontdata, TTF_DrawOperation *ops, int num_ops, int num_missing)
{
    stbrp_rect *missing = NULL;
    SDL_Surface **surfaces = NULL;
    SDL_HashTable *checked = NULL;
    bool result = false;

    // Build a list of missing glyphs
    missing = (stbrp_rect *)SDL_calloc(num_missing, sizeof(*missing));
    if (!missing) {
        goto done;
    }

    surfaces = (SDL_Surface **)SDL_calloc(num_ops, sizeof(*surfaces));
    if (!surfaces) {
        goto done;
    }

    checked = SDL_CreateHashTable(NULL, 4, SDL_HashID, SDL_KeyMatchID, NULL, false);
    if (!checked) {
        goto done;
    }

    int missing_index = 0;
    for (int i = 0; i < num_ops; ++i) {
        TTF_DrawOperation *op = &ops[i];
        if (op->cmd == TTF_DRAW_COMMAND_COPY && !op->copy.reserved) {
            Uint32 glyph_index = op->copy.glyph_index;
            if (SDL_FindInHashTable(checked, (const void *)(uintptr_t)glyph_index, NULL)) {
                continue;
            }
            if (!SDL_InsertIntoHashTable(checked, (const void *)(uintptr_t)glyph_index, NULL)) {
                goto done;
            }

            surfaces[i] = TTF_GetGlyphImageForIndex(fontdata->font, glyph_index);
            if (!surfaces[i]) {
                goto done;
            }
            if (surfaces[i]->w > ATLAS_TEXTURE_SIZE || surfaces[i]->h > ATLAS_TEXTURE_SIZE) {
                SDL_SetError("Glyph surface %dx%d larger than atlas texture %dx%d",
                    surfaces[i]->w, surfaces[i]->h,
                    ATLAS_TEXTURE_SIZE, ATLAS_TEXTURE_SIZE);
                goto done;
            }

            missing[missing_index].id = i;
            missing[missing_index].w = surfaces[i]->w;
            missing[missing_index].h = surfaces[i]->h;
            ++missing_index;
        }
    }
    num_missing = missing_index;

    // Sort the glyphs by size
    SDL_qsort_r(missing, num_missing, sizeof(*missing), SortMissing, ops);

    // Create the texture atlas if necessary
    if (!enginedata->atlas) {
        enginedata->atlas = CreateAtlas(enginedata->renderer);
        if (!enginedata->atlas) {
            goto done;
        }
    }

    if (!ResolveMissingGlyphs(enginedata, enginedata->atlas, fontdata, surfaces, ops, num_ops, missing, num_missing)) {
        goto done;
    }

    // Resolve any duplicates
    for (int i = 0; i < num_ops; ++i) {
        TTF_DrawOperation *op = &ops[i];
        if (op->cmd == TTF_DRAW_COMMAND_COPY && !op->copy.reserved) {
            if (!SDL_FindInHashTable(fontdata->glyphs, (const void *)(uintptr_t)op->copy.glyph_index, (const void **)&op->copy.reserved)) {
                // Something is very wrong...
                goto done;
            }
        }
    }

    result = true;

done:
    SDL_DestroyHashTable(checked);
    if (surfaces) {
        for (int i = 0; i < num_ops; ++i) {
            SDL_DestroySurface(surfaces[i]);
        }
        SDL_free(surfaces);
    }
    SDL_free(missing);
    return result;
}

static void DestroyDrawSequence(AtlasDrawSequence *data)
{
    if (!data) {
        return;
    }

    if (data->next) {
        DestroyDrawSequence(data->next);
    }
    SDL_free(data->rects);
    SDL_free(data->texcoords);
    SDL_free(data->positions);
    SDL_free(data->indices);
    SDL_free(data);
}

static SDL_Texture *GetOperationTexture(TTF_DrawOperation *op)
{
    if (op->cmd == TTF_DRAW_COMMAND_COPY) {
        AtlasGlyph *glyph = (AtlasGlyph *)op->copy.reserved;
        return glyph->atlas->texture;
    }
    return NULL;
}

static AtlasDrawSequence *CreateDrawSequence(TTF_DrawOperation *ops, int num_ops)
{
    AtlasDrawSequence *sequence = (AtlasDrawSequence *)SDL_calloc(1, sizeof(*sequence));
    if (!sequence) {
        return NULL;
    }

    SDL_Texture *texture = GetOperationTexture(&ops[0]);
    TTF_DrawOperation *end = NULL;
    for (int i = 1; i < num_ops; ++i) {
        if (GetOperationTexture(&ops[i]) != texture) {
            end = &ops[i];
            break;
        }
    }

    int count = (end ? (int)(end - ops) : num_ops);
    sequence->texture = texture;
    sequence->num_rects = count;
    sequence->rects = (SDL_Rect *)SDL_malloc(count * sizeof(*sequence->rects));
    if (!sequence->rects) {
        DestroyDrawSequence(sequence);
        return NULL;
    }

    for (int i = 0; i < count; ++i) {
        TTF_DrawOperation *op = &ops[i];
        SDL_Rect *dst = NULL;
        switch (op->cmd) {
        case TTF_DRAW_COMMAND_FILL:
            dst = &op->fill.rect;
            break;
        case TTF_DRAW_COMMAND_COPY:
            dst = &op->copy.dst;
            break;
        default:
            break;
        }
        SDL_copyp(&sequence->rects[i], dst);
    }

    if (texture) {
        AtlasGlyph *glyph;

        sequence->texcoords = (float *)SDL_malloc(count * sizeof(glyph->texcoords));
        if (!sequence->texcoords) {
            DestroyDrawSequence(sequence);
            return NULL;
        }

        float *texcoords = sequence->texcoords;
        for (int i = 0; i < count; ++i) {
            glyph = (AtlasGlyph *)ops[i].copy.reserved;
            SDL_memcpy(texcoords, glyph->texcoords, sizeof(glyph->texcoords));
            texcoords += SDL_arraysize(glyph->texcoords);
        }
    }

    sequence->positions = (float *)SDL_malloc(count * 8 * sizeof(*sequence->positions));
    if (!sequence->positions) {
        DestroyDrawSequence(sequence);
        return NULL;
    }

    sequence->indices = (int *)SDL_malloc(count * 12 * sizeof(*sequence->indices));
    if (!sequence->indices) {
        DestroyDrawSequence(sequence);
        return NULL;
    }

    static const Uint8 rect_index_order[] = { 0, 1, 2, 0, 2, 3 };
    int vertex_index = 0;
    int *indices = sequence->indices;
    for (int i = 0; i < count; ++i) {
        *indices++ = vertex_index + rect_index_order[0];
        *indices++ = vertex_index + rect_index_order[1];
        *indices++ = vertex_index + rect_index_order[2];
        *indices++ = vertex_index + rect_index_order[3];
        *indices++ = vertex_index + rect_index_order[4];
        *indices++ = vertex_index + rect_index_order[5];
        vertex_index += 4;
    }

    if (count < num_ops) {
        sequence->next = CreateDrawSequence(ops + count, num_ops - count);
        if (!sequence->next) {
            DestroyDrawSequence(sequence);
            return NULL;
        }
    }
    return sequence;
}

static void DestroyTextData(TTF_RendererTextEngineTextData *data)
{
    if (!data) {
        return;
    }

    DestroyDrawSequence(data->draw_sequence);

    for (int i = 0; i < data->num_glyphs; ++i) {
        ReleaseGlyph(data->glyphs[i]);
    }
    SDL_free(data->glyphs);
    SDL_free(data);
}

static TTF_RendererTextEngineTextData *CreateTextData(TTF_RendererTextEngineData *enginedata, TTF_RendererTextEngineFontData *fontdata, TTF_DrawOperation *ops, int num_ops)
{
    TTF_RendererTextEngineTextData *data = (TTF_RendererTextEngineTextData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return NULL;
    }

    // First, match draw operations to existing glyphs
    int num_glyphs = 0;
    int num_missing = 0;
    for (int i = 0; i < num_ops; ++i) {
        TTF_DrawOperation *op = &ops[i];

        if (op->cmd != TTF_DRAW_COMMAND_COPY) {
            continue;
        }

        ++num_glyphs;

        if (!SDL_FindInHashTable(fontdata->glyphs, (const void *)(uintptr_t)op->copy.glyph_index, (const void **)&op->copy.reserved)) {
            ++num_missing;
        }
    }

    // Create any missing glyphs
    if (num_missing > 0) {
        if (!CreateMissingGlyphs(enginedata, fontdata, ops, num_ops, num_missing)) {
            DestroyTextData(data);
            return NULL;
        }
    }

    // Add references to all the glyphs
    data->glyphs = (AtlasGlyph **)SDL_malloc(num_glyphs * sizeof(*data->glyphs));
    for (int i = 0; i < num_ops; ++i) {
        TTF_DrawOperation *op = &ops[i];

        if (op->cmd != TTF_DRAW_COMMAND_COPY) {
            continue;
        }

        AtlasGlyph *glyph = (AtlasGlyph*)op->copy.reserved;
        ++glyph->refcount;
        data->glyphs[data->num_glyphs++] = glyph;
    }

    // Sort the operations to batch by texture
    SDL_qsort(ops, num_ops, sizeof(*ops), SortOperations);

    // Create batched draw sequences
    data->draw_sequence = CreateDrawSequence(ops, num_ops);
    if (!data->draw_sequence) {
        DestroyTextData(data);
        return NULL;
    }

    return data;
}

static void DestroyFontData(TTF_RendererTextEngineFontData *data)
{
    if (data) {
        if (data->glyphs) {
            SDL_DestroyHashTable(data->glyphs);
        }
        SDL_free(data);
    }
}

static void NukeGlyph(const void *key, const void *value, void *unused)
{
    AtlasGlyph *glyph = (AtlasGlyph *)value;
    (void)key;
    (void)unused;
    ReleaseGlyph(glyph);
}

static TTF_RendererTextEngineFontData *CreateFontData(TTF_RendererTextEngineData *enginedata, TTF_Font *font, Uint32 font_generation)
{
    TTF_RendererTextEngineFontData *data = (TTF_RendererTextEngineFontData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return NULL;
    }
    data->font = font;
    data->generation = font_generation;
    data->glyphs = SDL_CreateHashTable(NULL, 4, SDL_HashID, SDL_KeyMatchID, NukeGlyph, false);
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

    AtlasTexture *next;
    for (AtlasTexture *atlas = data->atlas; atlas; atlas = next) {
        next = atlas->next;
        DestroyAtlas(atlas);
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

static bool SDLCALL CreateText(void *userdata, TTF_Text *text)
{
    TTF_Font *font = text->internal->font;
    Uint32 font_generation = TTF_GetFontGeneration(font);
    int num_ops = text->internal->num_ops;
    TTF_DrawOperation *ops;
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

    // Make a sortable copy of the draw operations
    ops = (TTF_DrawOperation *)SDL_malloc(num_ops * sizeof(*ops));
    if (!ops) {
        return false;
    }
    SDL_memcpy(ops, text->internal->ops, num_ops * sizeof(*ops));

    data = CreateTextData(enginedata, fontdata, ops, num_ops);
    SDL_free(ops);
    if (!data) {
        return false;
    }
    text->internal->engine_text = data;
    return true;
}

static void SDLCALL DestroyText(void *userdata, TTF_Text *text)
{
    TTF_RendererTextEngineTextData *data = (TTF_RendererTextEngineTextData *)text->internal->engine_text;

    (void)userdata;
    DestroyTextData(data);
}

TTF_TextEngine *TTF_CreateRendererTextEngine(SDL_Renderer *renderer)
{
    if (!renderer) {
        SDL_InvalidParamError("renderer");
        return NULL;
    }

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

bool TTF_DrawRendererText(TTF_Text *text, float x, float y)
{
    if (!text || !text->internal || text->internal->engine->CreateText != CreateText) {
        return SDL_InvalidParamError("text");
    }

    // Make sure the text is up to date
    if (!TTF_UpdateText(text)) {
        return false;
    }

    TTF_RendererTextEngineTextData *data = (TTF_RendererTextEngineTextData *)text->internal->engine_text;
    if (!data) {
        // Empty string, nothing to do
        return true;
    }

    SDL_Renderer *renderer = ((TTF_RendererTextEngineData *)text->internal->engine->userdata)->renderer;
    AtlasDrawSequence *sequence = data->draw_sequence;
    while (sequence) {
        float *position = sequence->positions;
        for (int i = 0; i < sequence->num_rects; ++i) {
            const SDL_Rect *dst = &sequence->rects[i];
            float minx = x + dst->x;
            float maxx = x + dst->x + dst->w;
            float miny = y + dst->y;
            float maxy = y + dst->y + dst->h;

            *position++ = minx;
            *position++ = miny;
            *position++ = maxx;
            *position++ = miny;
            *position++ = maxx;
            *position++ = maxy;
            *position++ = minx;
            *position++ = maxy;
        }

        SDL_RenderGeometryRaw(renderer,
                              sequence->texture,
                              sequence->positions, 2 * sizeof(float),
                              &text->internal->color, 0,
                              sequence->texcoords, 2 * sizeof(float),
                              sequence->num_rects * 4,
                              sequence->indices, sequence->num_rects * 6, sizeof(*sequence->indices));

        sequence = sequence->next;
    }
    return true;
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
