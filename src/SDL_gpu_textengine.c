/*
  SDL_ttf:  A companion library to SDL for working with TrueType (tm) fonts
  Copyright (C) 2001-2025 Sam Lantinga <slouken@libsdl.org>

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
#include <SDL3_ttf/SDL_textengine.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "SDL_hashtable.h"

#define ATLAS_TEXTURE_SIZE 1024

#define STB_RECT_PACK_IMPLEMENTATION
#define STBRP_STATIC
#define STBRP_SORT   SDL_qsort
#define STBRP_ASSERT SDL_assert
#define STBRP__CDECL SDLCALL
#include "stb_rect_pack.h"

typedef struct AtlasGlyph AtlasGlyph;
typedef struct AtlasTexture AtlasTexture;
typedef struct TTF_GPUAtlasDrawSequence AtlasDrawSequence;

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
    SDL_GPUTexture *texture;
    stbrp_context packer;
    stbrp_node *packing_nodes;
    AtlasGlyph *free_glyphs;
    AtlasTexture *next;
};

typedef struct TTF_GPUTextEngineTextData
{
    int num_glyphs;
    AtlasGlyph **glyphs;
    AtlasDrawSequence *draw_sequence;
} TTF_GPUTextEngineTextData;

typedef struct TTF_GPUTextEngineFontData
{
    TTF_Font *font;
    Uint32 generation;
    SDL_HashTable *glyphs;
} TTF_GPUTextEngineFontData;

typedef struct TTF_GPUTextEngineData
{
    SDL_GPUDevice *device;
    SDL_HashTable *fonts;
    AtlasTexture *atlas;
    TTF_GPUTextEngineWinding winding;
} TTF_GPUTextEngineData;

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

static void DestroyGlyph(AtlasGlyph *glyph)
{
    if (!glyph) {
        return;
    }

    SDL_free(glyph);
}

static void DestroyAtlas(SDL_GPUDevice *device, AtlasTexture *atlas)
{
    if (!atlas) {
        return;
    }

    AtlasGlyph *next;
    for (AtlasGlyph *glyph = atlas->free_glyphs; glyph; glyph = next) {
        next = glyph->next;
        DestroyGlyph(glyph);
    }

    SDL_ReleaseGPUTexture(device, atlas->texture);
    SDL_free(atlas->packing_nodes);
    SDL_free(atlas);
}

static AtlasTexture *CreateAtlas(SDL_GPUDevice *device)
{
    AtlasTexture *atlas = (AtlasTexture *)SDL_calloc(1, sizeof(*atlas));
    if (!atlas) {
        return NULL;
    }

    SDL_GPUTextureCreateInfo info = { 0 };
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width = ATLAS_TEXTURE_SIZE;
    info.height = ATLAS_TEXTURE_SIZE;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;

    atlas->texture = SDL_CreateGPUTexture(device, &info);

    if (!atlas->texture) {
        DestroyAtlas(device, atlas);
        return NULL;
    }

    SDL_GPUColorTargetInfo target_info;
    SDL_zero(target_info);
    target_info.texture = atlas->texture;
    target_info.clear_color = (SDL_FColor) { 0, 0, 0, 0 };
    target_info.load_op = SDL_GPU_LOADOP_CLEAR;

    SDL_GPUCommandBuffer *cbuf = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPURenderPass *rpass = SDL_BeginGPURenderPass(cbuf, &target_info, 1, NULL);
    SDL_EndGPURenderPass(rpass);
    SDL_SubmitGPUCommandBuffer(cbuf);

    int num_nodes = ATLAS_TEXTURE_SIZE / 4;
    atlas->packing_nodes = (stbrp_node *)SDL_calloc(num_nodes, sizeof(*atlas->packing_nodes));
    if (!atlas->packing_nodes) {
        DestroyAtlas(device, atlas);
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
    // Remove the one pixel extra padding between glyphs
    glyph->rect.w = area->w - 1;
    glyph->rect.h = area->h - 1;

    const float minu = (float)glyph->rect.x / ATLAS_TEXTURE_SIZE;
    const float minv = (float)glyph->rect.y / ATLAS_TEXTURE_SIZE;
    const float maxu = (float)(glyph->rect.x + glyph->rect.w) / ATLAS_TEXTURE_SIZE;
    const float maxv = (float)(glyph->rect.y + glyph->rect.h) / ATLAS_TEXTURE_SIZE;
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

static bool UpdateGPUTexture(SDL_GPUDevice *device, SDL_GPUTexture *texture,
                             const SDL_Rect *rect, const void *pixels, int pitch)
{
    const Uint32 texturebpp = 4;

    size_t row_size, data_size;

    if (!SDL_size_mul_check_overflow(rect->w, texturebpp, &row_size) ||
        !SDL_size_mul_check_overflow(rect->h, row_size, &data_size)) {
        return SDL_SetError("update size overflow");
    }

    SDL_GPUTransferBufferCreateInfo tbci;
    SDL_zero(tbci);
    tbci.size = (Uint32)data_size;
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

    SDL_GPUTransferBuffer *tbuf = SDL_CreateGPUTransferBuffer(device, &tbci);

    if (tbuf == NULL) {
        return false;
    }

    Uint8 *output = SDL_MapGPUTransferBuffer(device, tbuf, false);

    if ((size_t)pitch == row_size) {
        SDL_memcpy(output, pixels, data_size);
    } else {
        // FIXME is negative pitch supposed to work?
        // If not, maybe use SDL_GPUTextureTransferInfo::pixels_per_row instead of this
        const Uint8 *input = pixels;

        for (int i = 0; i < rect->h; ++i) {
            SDL_memcpy(output, input, row_size);
            output += row_size;
            input += pitch;
        }
    }

    SDL_UnmapGPUTransferBuffer(device, tbuf);

    SDL_GPUCommandBuffer *cbuf = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass *cpass = SDL_BeginGPUCopyPass(cbuf);

    SDL_GPUTextureTransferInfo tex_src;
    SDL_zero(tex_src);
    tex_src.transfer_buffer = tbuf;
    tex_src.rows_per_layer = rect->h;
    tex_src.pixels_per_row = rect->w;

    SDL_GPUTextureRegion tex_dst;
    SDL_zero(tex_dst);
    tex_dst.texture = texture;
    tex_dst.x = rect->x;
    tex_dst.y = rect->y;
    tex_dst.w = rect->w;
    tex_dst.h = rect->h;
    tex_dst.d = 1;

    SDL_UploadToGPUTexture(cpass, &tex_src, &tex_dst, false);
    SDL_EndGPUCopyPass(cpass);
    SDL_ReleaseGPUTransferBuffer(device, tbuf);
    SDL_SubmitGPUCommandBuffer(cbuf);

    return true;
}

static bool UpdateGlyph(SDL_GPUDevice *device, AtlasGlyph *glyph, SDL_Surface *surface)
{
    if (glyph->rect.w > 0 && glyph->rect.h > 0) {
        /* FIXME: We should update the whole texture at once or at least cache the transfer buffers */
        UpdateGPUTexture(device, glyph->atlas->texture, &glyph->rect, surface->pixels, surface->pitch);
    }
    return true;
}

static bool AddGlyphToFont(TTF_GPUTextEngineFontData *fontdata, Uint32 glyph_index, AtlasGlyph *glyph)
{
    if (!SDL_InsertIntoHashTable(fontdata->glyphs, (const void *)(uintptr_t)glyph_index, glyph)) {
        return false;
    }
    return true;
}

static bool ResolveMissingGlyphs(TTF_GPUTextEngineData *enginedata, AtlasTexture *atlas, TTF_GPUTextEngineFontData *fontdata, SDL_Surface **surfaces, TTF_DrawOperation *ops, int num_ops, stbrp_rect *missing, int num_missing)
{
    // See if we can reuse any existing entries
    if (atlas->free_glyphs) {
        // Search from the smallest to the largest to minimize time spent searching the free list and shortening the missing entries
        for (int i = num_missing; i--;) {
            AtlasGlyph *glyph = FindUnusedGlyph(atlas, missing[i].w, missing[i].h);
            if (!glyph) {
                continue;
            }

            if (!UpdateGlyph(enginedata->device, glyph, surfaces[missing[i].id])) {
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
                SDL_memcpy(&missing[i], &missing[i + 1], (num_missing - i) * sizeof(missing[i]));
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

        if (!UpdateGlyph(enginedata->device, glyph, surfaces[missing[i].id])) {
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
        atlas->next = CreateAtlas(enginedata->device);
        if (!atlas->next) {
            return false;
        }
    }
    return ResolveMissingGlyphs(enginedata, atlas->next, fontdata, surfaces, ops, num_ops, missing, num_missing);
}

static bool CreateMissingGlyphs(TTF_GPUTextEngineData *enginedata, TTF_GPUTextEngineFontData *fontdata, TTF_DrawOperation *ops, int num_ops, int num_missing)
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

    checked = SDL_CreateHashTable(NULL, 4, SDL_HashID, SDL_KeyMatchID, NULL, false, false);
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
            // Add one pixel extra padding between glyphs
            missing[missing_index].w = surfaces[i]->w + 1;
            missing[missing_index].h = surfaces[i]->h + 1;
            ++missing_index;
        }
    }
    num_missing = missing_index;

    // Sort the glyphs by size
    SDL_qsort_r(missing, num_missing, sizeof(*missing), SortMissing, ops);

    // Create the texture atlas if necessary
    if (!enginedata->atlas) {
        enginedata->atlas = CreateAtlas(enginedata->device);
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
    SDL_free(data->xy);
    SDL_free(data->uv);
    SDL_free(data->indices);
    SDL_free(data);
}

static SDL_GPUTexture *GetOperationTexture(TTF_DrawOperation *op)
{
    if (op->cmd == TTF_DRAW_COMMAND_COPY) {
        AtlasGlyph *glyph = (AtlasGlyph *)op->copy.reserved;
        return glyph->atlas->texture;
    }
    return NULL;
}

static AtlasDrawSequence *CreateDrawSequence(TTF_DrawOperation *ops, int num_ops, TTF_GPUTextEngineWinding winding)
{
    AtlasDrawSequence *sequence = (AtlasDrawSequence *)SDL_calloc(1, sizeof(*sequence));
    if (!sequence) {
        return NULL;
    }

    SDL_assert(num_ops > 0);
    SDL_COMPILE_TIME_ASSERT(sizeof_SDL_FPoint, sizeof(SDL_FPoint) == 2 * sizeof(float));

    SDL_GPUTexture *texture = GetOperationTexture(&ops[0]);
    TTF_DrawOperation *end = NULL;
    for (int i = 1; i < num_ops; ++i) {
        if (GetOperationTexture(&ops[i]) != texture) {
            end = &ops[i];
            break;
        }
    }

    int count = (end ? (int)(end - ops) : num_ops);
    sequence->atlas_texture = texture;
    sequence->num_vertices = count * 4;
    sequence->num_indices = count * 6;

    if (texture) {
        AtlasGlyph *glyph;

        sequence->uv = (SDL_FPoint *)SDL_malloc(count * sizeof(glyph->texcoords));
        if (!sequence->uv) {
            DestroyDrawSequence(sequence);
            return NULL;
        }

        float *uv = (float *)sequence->uv;
        for (int i = 0; i < count; ++i) {
            glyph = (AtlasGlyph *)ops[i].copy.reserved;
            SDL_memcpy(uv, glyph->texcoords, sizeof(glyph->texcoords));
            uv += SDL_arraysize(glyph->texcoords);
        }
    }

    sequence->xy = (SDL_FPoint *)SDL_malloc(count * 4 * sizeof(*sequence->xy));
    if (!sequence->xy) {
        DestroyDrawSequence(sequence);
        return NULL;
    }
    float *xy = (float *)sequence->xy;
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

        float minx = (float)dst->x;
        float maxx = (float)(dst->x + dst->w);
        float miny = (float)dst->y;
        float maxy = (float)(dst->y + dst->h);

        // In the GPU API postive y-axis is upwards so the signs of the y-coords is reversed
        *xy++ =  minx;
        *xy++ = -miny;
        *xy++ =  maxx;
        *xy++ = -miny;
        *xy++ =  maxx;
        *xy++ = -maxy;
        *xy++ =  minx;
        *xy++ = -maxy;
    }

    sequence->indices = (int *)SDL_malloc(count * 12 * sizeof(*sequence->indices));
    if (!sequence->indices) {
        DestroyDrawSequence(sequence);
        return NULL;
    }

    static const Uint8 rect_index_order_cw[] = { 0, 1, 2, 0, 2, 3 };
    static const Uint8 rect_index_order_ccw[] = { 0, 2, 1, 0, 3, 2 };

    const Uint8 *rect_index_order;
    if (winding == TTF_GPU_TEXTENGINE_WINDING_CLOCKWISE) {
        rect_index_order = rect_index_order_cw;
    } else {
        rect_index_order = rect_index_order_ccw;
    }

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
        sequence->next = CreateDrawSequence(ops + count, num_ops - count, winding);
        if (!sequence->next) {
            DestroyDrawSequence(sequence);
            return NULL;
        }
    }
    return sequence;
}

static void DestroyTextData(TTF_GPUTextEngineTextData *data)
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

static TTF_GPUTextEngineTextData *CreateTextData(TTF_GPUTextEngineData *enginedata, TTF_GPUTextEngineFontData *fontdata, TTF_DrawOperation *ops, int num_ops)
{
    TTF_GPUTextEngineTextData *data = (TTF_GPUTextEngineTextData *)SDL_calloc(1, sizeof(*data));
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

        AtlasGlyph *glyph = (AtlasGlyph *)op->copy.reserved;
        ++glyph->refcount;
        data->glyphs[data->num_glyphs++] = glyph;
    }

    // Sort the operations to batch by texture
    SDL_qsort(ops, num_ops, sizeof(*ops), SortOperations);

    // Create batched draw sequences
    data->draw_sequence = CreateDrawSequence(ops, num_ops, enginedata->winding);
    if (!data->draw_sequence) {
        DestroyTextData(data);
        return NULL;
    }

    return data;
}

static void DestroyFontData(TTF_GPUTextEngineFontData *data)
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

static TTF_GPUTextEngineFontData *CreateFontData(TTF_GPUTextEngineData *enginedata, TTF_Font *font, Uint32 font_generation)
{
    TTF_GPUTextEngineFontData *data = (TTF_GPUTextEngineFontData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return NULL;
    }
    data->font = font;
    data->generation = font_generation;
    data->glyphs = SDL_CreateHashTable(NULL, 4, SDL_HashID, SDL_KeyMatchID, NukeGlyph, false, false);
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

static void DestroyEngineData(TTF_GPUTextEngineData *data)
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
        DestroyAtlas(data->device, atlas);
    }
    SDL_free(data);
}

static void NukeFontData(const void *key, const void *value, void *unused)
{
    TTF_GPUTextEngineFontData *data = (TTF_GPUTextEngineFontData *)value;
    (void)key;
    (void)unused;
    DestroyFontData(data);
}

static TTF_GPUTextEngineData *CreateEngineData(SDL_GPUDevice *device)
{
    TTF_GPUTextEngineData *data = (TTF_GPUTextEngineData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return NULL;
    }
    data->device = device;
    data->winding = TTF_GPU_TEXTENGINE_WINDING_CLOCKWISE;

    data->fonts = SDL_CreateHashTable(NULL, 4, SDL_HashPointer, SDL_KeyMatchPointer, NukeFontData, false, false);
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
    TTF_GPUTextEngineData *enginedata = (TTF_GPUTextEngineData *)userdata;
    TTF_GPUTextEngineFontData *fontdata;
    TTF_GPUTextEngineTextData *data;

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
    TTF_GPUTextEngineTextData *data = (TTF_GPUTextEngineTextData *)text->internal->engine_text;

    (void)userdata;
    DestroyTextData(data);
}

void TTF_DestroyGPUTextEngine(TTF_TextEngine *engine)
{
    if (!engine || engine->CreateText != CreateText) {
        return;
    }

    DestroyEngineData((TTF_GPUTextEngineData *)engine->userdata);
    engine->CreateText = NULL;
    SDL_free(engine);
}

TTF_TextEngine *TTF_CreateGPUTextEngine(SDL_GPUDevice *device)
{
    if (!device) {
        SDL_InvalidParamError("device");
        return NULL;
    }

    TTF_TextEngine *engine = (TTF_TextEngine *)SDL_malloc(sizeof(*engine));
    if (!engine) {
        return NULL;
    }

    SDL_INIT_INTERFACE(engine);
    engine->CreateText = CreateText;
    engine->DestroyText = DestroyText;
    engine->userdata = CreateEngineData(device);
    if (!engine->userdata) {
        TTF_DestroyGPUTextEngine(engine);
        return NULL;
    }
    return engine;
}

AtlasDrawSequence *TTF_GetGPUTextDrawData(TTF_Text *text)
{
    if (!text || !text->internal || text->internal->engine->CreateText != CreateText) {
        SDL_InvalidParamError("text");
        return NULL;
    }

    // Make sure the text is up to date
    if (!TTF_UpdateText(text)) {
        return NULL;
    }

    TTF_GPUTextEngineTextData *data = (TTF_GPUTextEngineTextData *)text->internal->engine_text;
    if (!data) {
        // Empty string, nothing to do
        return NULL;
    }

    return data->draw_sequence;
}

void TTF_SetGPUTextEngineWinding(TTF_TextEngine *engine, TTF_GPUTextEngineWinding winding)
{
    if (!engine || engine->CreateText != CreateText) {
        SDL_InvalidParamError("engine");
        return;
    }

    if (winding == TTF_GPU_TEXTENGINE_WINDING_INVALID) {
        SDL_InvalidParamError("winding");
        return;
    }

    ((TTF_GPUTextEngineData *)engine->userdata)->winding = winding;
}

TTF_GPUTextEngineWinding TTF_GetGPUTextEngineWinding(const TTF_TextEngine *engine)
{
    if (!engine || engine->CreateText != CreateText) {
        SDL_InvalidParamError("engine");
        return TTF_GPU_TEXTENGINE_WINDING_INVALID;
    }

    return ((TTF_GPUTextEngineData *)engine->userdata)->winding;
}
