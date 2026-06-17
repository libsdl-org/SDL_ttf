/*
  SDL_ttf:  A companion library to SDL for working with TrueType (tm) fonts
  Copyright (C) 2026 Ivan Vecera <ivan@cera.cz>

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
#define SDL_OPENGL_1_FUNCTION_TYPEDEFS
#include <SDL3/SDL_opengl.h>
#include <SDL3_ttf/SDL_textengine.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "SDL_hashtable.h"
#include "SDL_hashtable_ttf.h"

#define STB_RECT_PACK_IMPLEMENTATION
#define STBRP_STATIC
#define STBRP_SORT   SDL_qsort
#define STBRP_ASSERT SDL_assert
#define STBRP__CDECL SDLCALL
#include "stb_rect_pack.h"

typedef struct AtlasGlyph AtlasGlyph;
typedef struct AtlasTexture AtlasTexture;
typedef struct TTF_GLAtlasDrawSequence AtlasDrawSequence;

typedef struct GlyphSurface
{
    SDL_Surface *surface;
    TTF_ImageType image_type;
} GlyphSurface;

struct AtlasGlyph
{
    int refcount;
    AtlasTexture *atlas;
    TTF_ImageType image_type;
    SDL_Rect rect;
    float texcoords[8];
    AtlasGlyph *next;
};

struct AtlasTexture
{
    GLuint texture;
    stbrp_context packer;
    stbrp_node *packing_nodes;
    AtlasGlyph *free_glyphs;
    AtlasTexture *next;
};

typedef struct TTF_GLTextEngineTextData
{
    int num_glyphs;
    AtlasGlyph **glyphs;
    AtlasDrawSequence *draw_sequence;
} TTF_GLTextEngineTextData;

typedef struct TTF_GLTextEngineFontData
{
    TTF_Font *font;
    Uint32 generation;
    SDL_HashTable *glyphs;
} TTF_GLTextEngineFontData;

typedef struct TTF_GLTextEngineData
{
    /* GL function pointers */
    PFNGLGENTEXTURESPROC glGenTextures;
    PFNGLDELETETEXTURESPROC glDeleteTextures;
    PFNGLBINDTEXTUREPROC glBindTexture;
    PFNGLTEXIMAGE2DPROC glTexImage2D;
    PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
    PFNGLTEXPARAMETERIPROC glTexParameteri;
    PFNGLPIXELSTOREIPROC glPixelStorei;

    SDL_HashTable *fonts;
    AtlasTexture *atlas;
    int atlas_texture_size;
    TTF_GLTextEngineWinding winding;
    bool has_bgra;
    bool has_unpack_row_length;
} TTF_GLTextEngineData;

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

static void DestroyAtlas(TTF_GLTextEngineData *enginedata, AtlasTexture *atlas)
{
    if (!atlas) {
        return;
    }

    AtlasGlyph *next;
    for (AtlasGlyph *glyph = atlas->free_glyphs; glyph; glyph = next) {
        next = glyph->next;
        DestroyGlyph(glyph);
    }

    if (atlas->texture) {
        enginedata->glDeleteTextures(1, &atlas->texture);
    }
    SDL_free(atlas->packing_nodes);
    SDL_free(atlas);
}

static AtlasTexture *CreateAtlas(TTF_GLTextEngineData *enginedata, int atlas_texture_size)
{
    AtlasTexture *atlas = (AtlasTexture *)SDL_calloc(1, sizeof(*atlas));
    if (!atlas) {
        return NULL;
    }

    enginedata->glGenTextures(1, &atlas->texture);
    if (!atlas->texture) {
        DestroyAtlas(enginedata, atlas);
        return NULL;
    }

    enginedata->glBindTexture(GL_TEXTURE_2D, atlas->texture);
    enginedata->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    enginedata->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /* Allocate zeroed texture storage */
    size_t buffer_size;
    if (!SDL_size_mul_check_overflow((size_t)atlas_texture_size * atlas_texture_size, 4, &buffer_size)) {
        DestroyAtlas(enginedata, atlas);
        SDL_SetError("Atlas texture size overflow");
        return NULL;
    }

    void *zeroed = SDL_calloc(1, buffer_size);
    if (!zeroed) {
        DestroyAtlas(enginedata, atlas);
        return NULL;
    }
    enginedata->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                             atlas_texture_size, atlas_texture_size, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, zeroed);
    SDL_free(zeroed);

    int num_nodes = atlas_texture_size / 4;
    atlas->packing_nodes = (stbrp_node *)SDL_calloc(num_nodes, sizeof(*atlas->packing_nodes));
    if (!atlas->packing_nodes) {
        DestroyAtlas(enginedata, atlas);
        return NULL;
    }
    stbrp_init_target(&atlas->packer, atlas_texture_size, atlas_texture_size, atlas->packing_nodes, num_nodes);
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

static AtlasGlyph *CreateGlyph(AtlasTexture *atlas, int atlas_texture_size, const stbrp_rect *area)
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

    const float minu = (float)glyph->rect.x / atlas_texture_size;
    const float minv = (float)glyph->rect.y / atlas_texture_size;
    const float maxu = (float)(glyph->rect.x + glyph->rect.w) / atlas_texture_size;
    const float maxv = (float)(glyph->rect.y + glyph->rect.h) / atlas_texture_size;
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

static bool UpdateGLTexture(TTF_GLTextEngineData *enginedata, unsigned int texture,
                            const SDL_Rect *rect, const void *pixels, int pitch)
{
    const Uint32 texturebpp = 4;

    enginedata->glBindTexture(GL_TEXTURE_2D, texture);

    if (enginedata->has_bgra) {
        if (enginedata->has_unpack_row_length) {
            enginedata->glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / (int)texturebpp);
            enginedata->glTexSubImage2D(GL_TEXTURE_2D, 0,
                                        rect->x, rect->y, rect->w, rect->h,
                                        GL_BGRA, GL_UNSIGNED_BYTE, pixels);
            enginedata->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        } else {
            /* No UNPACK_ROW_LENGTH: copy to tightly-packed buffer */
            size_t row_size = (size_t)rect->w * texturebpp;
            size_t data_size = (size_t)rect->h * row_size;
            Uint8 *packed = (Uint8 *)SDL_malloc(data_size);
            if (!packed) {
                return false;
            }
            const Uint8 *src = (const Uint8 *)pixels;
            Uint8 *dst = packed;
            for (int i = 0; i < rect->h; ++i) {
                SDL_memcpy(dst, src, row_size);
                dst += row_size;
                src += pitch;
            }
            enginedata->glTexSubImage2D(GL_TEXTURE_2D, 0,
                                        rect->x, rect->y, rect->w, rect->h,
                                        GL_BGRA, GL_UNSIGNED_BYTE, packed);
            SDL_free(packed);
        }
    } else {
        /* No BGRA support: convert to RGBA */
        size_t row_size = (size_t)rect->w * texturebpp;
        size_t data_size = (size_t)rect->h * row_size;
        Uint8 *converted = (Uint8 *)SDL_malloc(data_size);
        if (!converted) {
            return false;
        }
        const Uint8 *src = (const Uint8 *)pixels;
        Uint8 *dst_row = converted;
        for (int y = 0; y < rect->h; ++y) {
            const Uint32 *src_pixels = (const Uint32 *)src;
            Uint32 *dst_pixels = (Uint32 *)dst_row;
            for (int x = 0; x < rect->w; ++x) {
                Uint32 p = src_pixels[x];
                /* BGRA byte order -> RGBA byte order: swap B and R */
                dst_pixels[x] = (p & 0xFF00FF00u) | ((p >> 16) & 0xFFu) | ((p & 0xFFu) << 16);
            }
            src += pitch;
            dst_row += row_size;
        }
        enginedata->glTexSubImage2D(GL_TEXTURE_2D, 0,
                                    rect->x, rect->y, rect->w, rect->h,
                                    GL_RGBA, GL_UNSIGNED_BYTE, converted);
        SDL_free(converted);
    }

    return true;
}

static bool UpdateGlyph(TTF_GLTextEngineData *enginedata, AtlasGlyph *glyph, SDL_Surface *surface, TTF_ImageType image_type)
{
    SDL_assert(glyph->rect.w > 0 && glyph->rect.h > 0);

    UpdateGLTexture(enginedata, glyph->atlas->texture, &glyph->rect, surface->pixels, surface->pitch);
    glyph->image_type = image_type;
    return true;
}

static bool AddGlyphToFont(TTF_GLTextEngineFontData *fontdata, TTF_Font *glyph_font, Uint32 glyph_index, AtlasGlyph *glyph)
{
    if (!SDL_InsertIntoGlyphHashTable(fontdata->glyphs, glyph_font, glyph_index, glyph)) {
        return false;
    }
    return true;
}

static bool ResolveMissingGlyphs(TTF_GLTextEngineData *enginedata, AtlasTexture *atlas, TTF_GLTextEngineFontData *fontdata, GlyphSurface *surfaces, TTF_DrawOperation *ops, int num_ops, stbrp_rect *missing, int num_missing)
{
    // See if we can reuse any existing entries
    if (atlas->free_glyphs) {
        // Search from the smallest to the largest to minimize time spent searching the free list and shortening the missing entries
        for (int i = num_missing; i--;) {
            AtlasGlyph *glyph = FindUnusedGlyph(atlas, missing[i].w, missing[i].h);
            if (!glyph) {
                continue;
            }

            GlyphSurface *surface = &surfaces[missing[i].id];
            if (!UpdateGlyph(enginedata, glyph, surface->surface, surface->image_type)) {
                ReleaseGlyph(glyph);
                return false;
            }

            TTF_DrawOperation *op = &ops[missing[i].id];
            if (!AddGlyphToFont(fontdata, op->copy.glyph_font, op->copy.glyph_index, glyph)) {
                ReleaseGlyph(glyph);
                return false;
            }

            op->copy.reserved = glyph;

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

        AtlasGlyph *glyph = CreateGlyph(atlas, enginedata->atlas_texture_size, &missing[i]);
        if (!glyph) {
            return false;
        }

        GlyphSurface *surface = &surfaces[missing[i].id];
        if (!UpdateGlyph(enginedata, glyph, surface->surface, surface->image_type)) {
            ReleaseGlyph(glyph);
            return false;
        }

        TTF_DrawOperation *op = &ops[missing[i].id];
        if (!AddGlyphToFont(fontdata, op->copy.glyph_font, op->copy.glyph_index, glyph)) {
            ReleaseGlyph(glyph);
            return false;
        }

        op->copy.reserved = glyph;
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
        atlas->next = CreateAtlas(enginedata, enginedata->atlas_texture_size);
        if (!atlas->next) {
            return false;
        }
    }
    return ResolveMissingGlyphs(enginedata, atlas->next, fontdata, surfaces, ops, num_ops, missing, num_missing);
}

static bool CreateMissingGlyphs(TTF_GLTextEngineData *enginedata, TTF_GLTextEngineFontData *fontdata, TTF_DrawOperation *ops, int num_ops, int num_missing)
{
    stbrp_rect *missing = NULL;
    GlyphSurface *surfaces = NULL;
    SDL_HashTable *checked = NULL;
    bool result = false;
    int atlas_texture_size = enginedata->atlas_texture_size;

    // Build a list of missing glyphs
    missing = (stbrp_rect *)SDL_calloc(num_missing, sizeof(*missing));
    if (!missing) {
        goto done;
    }

    surfaces = (GlyphSurface *)SDL_calloc(num_ops, sizeof(*surfaces));
    if (!surfaces) {
        goto done;
    }

    checked = SDL_CreateGlyphHashTable(NULL);
    if (!checked) {
        goto done;
    }

    int missing_index = 0;
    for (int i = 0; i < num_ops; ++i) {
        TTF_DrawOperation *op = &ops[i];
        if (op->cmd == TTF_DRAW_COMMAND_COPY && !op->copy.reserved) {
            TTF_Font *glyph_font = op->copy.glyph_font;
            Uint32 glyph_index = op->copy.glyph_index;
            if (SDL_FindInGlyphHashTable(checked, glyph_font, glyph_index, NULL)) {
                continue;
            }
            if (!SDL_InsertIntoGlyphHashTable(checked, glyph_font, glyph_index, NULL)) {
                goto done;
            }

            TTF_ImageType image_type = TTF_IMAGE_INVALID;
            SDL_Surface *surface = TTF_GetGlyphImageForIndex(glyph_font, glyph_index, &image_type);
            if (!surface) {
                goto done;
            }
            if (surface->w > atlas_texture_size || surface->h > atlas_texture_size) {
                SDL_SetError("Glyph surface %dx%d larger than atlas texture %dx%d",
                             surface->w, surface->h,
                             atlas_texture_size, atlas_texture_size);
                goto done;
            }

            surfaces[i].surface = surface;
            surfaces[i].image_type = image_type;

            missing[missing_index].id = i;
            // Add one pixel extra padding between glyphs
            missing[missing_index].w = surface->w + 1;
            missing[missing_index].h = surface->h + 1;
            ++missing_index;
        }
    }
    num_missing = missing_index;

    // Sort the glyphs by size
    SDL_qsort_r(missing, num_missing, sizeof(*missing), SortMissing, ops);

    // Create the texture atlas if necessary
    if (!enginedata->atlas) {
        enginedata->atlas = CreateAtlas(enginedata, atlas_texture_size);
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
            if (!SDL_FindInGlyphHashTable(fontdata->glyphs, op->copy.glyph_font, op->copy.glyph_index, (const void **)&op->copy.reserved)) {
                // Something is very wrong...
                goto done;
            }
        }
    }

    result = true;

done:
    SDL_DestroyGlyphHashTable(checked);
    if (surfaces) {
        for (int i = 0; i < num_ops; ++i) {
            SDL_DestroySurface(surfaces[i].surface);
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
    SDL_free(data->vertices);
    SDL_free(data->indices);
    SDL_free(data);
}

static unsigned int GetOperationTexture(TTF_DrawOperation *op)
{
    if (op->cmd == TTF_DRAW_COMMAND_COPY) {
        AtlasGlyph *glyph = (AtlasGlyph *)op->copy.reserved;
        return glyph->atlas->texture;
    }
    return 0;
}

static TTF_ImageType GetOperationImageType(TTF_DrawOperation *op)
{
    if (op->cmd == TTF_DRAW_COMMAND_COPY) {
        AtlasGlyph *glyph = (AtlasGlyph *)op->copy.reserved;
        return glyph->image_type;
    }
    return TTF_IMAGE_INVALID;
}

static AtlasDrawSequence *CreateDrawSequence(TTF_DrawOperation *ops, int num_ops, TTF_GLTextEngineWinding winding)
{
    AtlasDrawSequence *sequence = (AtlasDrawSequence *)SDL_calloc(1, sizeof(*sequence));
    if (!sequence) {
        return NULL;
    }

    SDL_assert(num_ops > 0);
    SDL_COMPILE_TIME_ASSERT(sizeof_SDL_FPoint, sizeof(SDL_FPoint) == 2 * sizeof(float));

    unsigned int texture = GetOperationTexture(&ops[0]);
    TTF_ImageType image_type = GetOperationImageType(&ops[0]);
    TTF_DrawOperation *end = NULL;
    for (int i = 1; i < num_ops; ++i) {
        if (GetOperationTexture(&ops[i]) != texture ||
            GetOperationImageType(&ops[i]) != image_type) {
            end = &ops[i];
            break;
        }
    }

    int count = (end ? (int)(end - ops) : num_ops);
    sequence->atlas_texture = texture;
    sequence->image_type = image_type;
    sequence->num_vertices = count * 4;
    sequence->num_indices = count * 6;

    sequence->vertices = (TTF_GLAtlasDrawVertex *)SDL_malloc(count * 4 * sizeof(*sequence->vertices));
    if (!sequence->vertices) {
        DestroyDrawSequence(sequence);
        return NULL;
    }

    TTF_GLAtlasDrawVertex *vtx = sequence->vertices;
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

        /* Ensure fill rects are at least 3px tall for shader-based edge AA */
        if (op->cmd == TTF_DRAW_COMMAND_FILL && dst->h < 3) {
            float pad = (3.0f - dst->h) / 2.0f;
            miny -= pad;
            maxy += pad;
        }

        float u0, v0, u1, v1, u2, v2, u3, v3;
        if (texture) {
            AtlasGlyph *glyph = (AtlasGlyph *)ops[i].copy.reserved;
            u0 = glyph->texcoords[0];
            v0 = glyph->texcoords[1];
            u1 = glyph->texcoords[2];
            v1 = glyph->texcoords[3];
            u2 = glyph->texcoords[4];
            v2 = glyph->texcoords[5];
            u3 = glyph->texcoords[6];
            v3 = glyph->texcoords[7];
        } else {
            u0 = 0.0f;
            v0 = 0.0f;
            u1 = 1.0f;
            v1 = 0.0f;
            u2 = 1.0f;
            v2 = 1.0f;
            u3 = 0.0f;
            v3 = 1.0f;
        }

        // In OpenGL positive y-axis is upwards so the signs of the y-coords are reversed
        vtx->position.x = minx;
        vtx->position.y = -miny;
        vtx->texcoord.x = u0;
        vtx->texcoord.y = v0;
        vtx++;
        vtx->position.x = maxx;
        vtx->position.y = -miny;
        vtx->texcoord.x = u1;
        vtx->texcoord.y = v1;
        vtx++;
        vtx->position.x = maxx;
        vtx->position.y = -maxy;
        vtx->texcoord.x = u2;
        vtx->texcoord.y = v2;
        vtx++;
        vtx->position.x = minx;
        vtx->position.y = -maxy;
        vtx->texcoord.x = u3;
        vtx->texcoord.y = v3;
        vtx++;
    }

    sequence->indices = (Uint16 *)SDL_malloc(count * 6 * sizeof(*sequence->indices));
    if (!sequence->indices) {
        DestroyDrawSequence(sequence);
        return NULL;
    }

    static const Uint8 rect_index_order_cw[] = { 0, 1, 2, 0, 2, 3 };
    static const Uint8 rect_index_order_ccw[] = { 0, 2, 1, 0, 3, 2 };

    const Uint8 *rect_index_order;
    if (winding == TTF_GL_TEXTENGINE_WINDING_CLOCKWISE) {
        rect_index_order = rect_index_order_cw;
    } else {
        rect_index_order = rect_index_order_ccw;
    }

    Uint16 vertex_index = 0;
    Uint16 *indices = sequence->indices;
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

static void DestroyTextData(TTF_GLTextEngineTextData *data)
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

static TTF_GLTextEngineTextData *CreateTextData(TTF_GLTextEngineData *enginedata, TTF_GLTextEngineFontData *fontdata, TTF_DrawOperation *ops, int num_ops)
{
    TTF_GLTextEngineTextData *data = (TTF_GLTextEngineTextData *)SDL_calloc(1, sizeof(*data));
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

        if (!SDL_FindInGlyphHashTable(fontdata->glyphs, op->copy.glyph_font, op->copy.glyph_index, (const void **)&op->copy.reserved)) {
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

static void DestroyFontData(TTF_GLTextEngineFontData *data)
{
    if (data) {
        if (data->glyphs) {
            SDL_DestroyGlyphHashTable(data->glyphs);
        }
        SDL_free(data);
    }
}

static void NukeGlyph(const void *value)
{
    AtlasGlyph *glyph = (AtlasGlyph *)value;
    ReleaseGlyph(glyph);
}

static TTF_GLTextEngineFontData *CreateFontData(TTF_GLTextEngineData *enginedata, TTF_Font *font, Uint32 font_generation)
{
    TTF_GLTextEngineFontData *data = (TTF_GLTextEngineFontData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return NULL;
    }
    data->font = font;
    data->generation = font_generation;
    data->glyphs = SDL_CreateGlyphHashTable(NukeGlyph);
    if (!data->glyphs) {
        DestroyFontData(data);
        return NULL;
    }

    if (!SDL_InsertIntoHashTable(enginedata->fonts, font, data, true)) {
        DestroyFontData(data);
        return NULL;
    }

    return data;
}

static void DestroyEngineData(TTF_GLTextEngineData *data)
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
        DestroyAtlas(data, atlas);
    }
    SDL_free(data);
}

static void SDLCALL NukeFontData(void *userdata, const void *key, const void *value)
{
    TTF_GLTextEngineFontData *data = (TTF_GLTextEngineFontData *)value;
    DestroyFontData(data);
}

static bool LoadGLFunctions(TTF_GLTextEngineData *data)
{
    data->glGenTextures = (PFNGLGENTEXTURESPROC)(uintptr_t)SDL_GL_GetProcAddress("glGenTextures");
    data->glDeleteTextures = (PFNGLDELETETEXTURESPROC)(uintptr_t)SDL_GL_GetProcAddress("glDeleteTextures");
    data->glBindTexture = (PFNGLBINDTEXTUREPROC)(uintptr_t)SDL_GL_GetProcAddress("glBindTexture");
    data->glTexImage2D = (PFNGLTEXIMAGE2DPROC)(uintptr_t)SDL_GL_GetProcAddress("glTexImage2D");
    data->glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC)(uintptr_t)SDL_GL_GetProcAddress("glTexSubImage2D");
    data->glTexParameteri = (PFNGLTEXPARAMETERIPROC)(uintptr_t)SDL_GL_GetProcAddress("glTexParameteri");
    data->glPixelStorei = (PFNGLPIXELSTOREIPROC)(uintptr_t)SDL_GL_GetProcAddress("glPixelStorei");

    if (!data->glGenTextures || !data->glDeleteTextures || !data->glBindTexture ||
        !data->glTexImage2D || !data->glTexSubImage2D || !data->glTexParameteri ||
        !data->glPixelStorei) {
        return SDL_SetError("Failed to load required OpenGL functions");
    }

    return true;
}

static bool ProbeGLCapabilities(TTF_GLTextEngineData *data)
{
    int profile = 0;
    if (!SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile)) {
        return SDL_SetError("Failed to query OpenGL context profile");
    }

    if (profile & SDL_GL_CONTEXT_PROFILE_ES) {
        data->has_bgra = SDL_GL_ExtensionSupported("GL_EXT_texture_format_BGRA8888") ||
                         SDL_GL_ExtensionSupported("GL_APPLE_texture_format_BGRA8888");
        data->has_unpack_row_length = SDL_GL_ExtensionSupported("GL_EXT_unpack_subimage");
    } else {
        /* Desktop GL 1.2+ always has BGRA and UNPACK_ROW_LENGTH */
        data->has_bgra = true;
        data->has_unpack_row_length = true;
    }

    return true;
}

static TTF_GLTextEngineData *CreateEngineData(int atlas_texture_size)
{
    TTF_GLTextEngineData *data = (TTF_GLTextEngineData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return NULL;
    }
    data->atlas_texture_size = atlas_texture_size;
    data->winding = TTF_GL_TEXTENGINE_WINDING_CLOCKWISE;

    if (!LoadGLFunctions(data)) {
        DestroyEngineData(data);
        return NULL;
    }

    if (!ProbeGLCapabilities(data)) {
        DestroyEngineData(data);
        return NULL;
    }

    data->fonts = SDL_CreateHashTable(0, false, SDL_HashPointer, SDL_KeyMatchPointer, NukeFontData, NULL);
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
    TTF_GLTextEngineData *enginedata = (TTF_GLTextEngineData *)userdata;
    TTF_GLTextEngineFontData *fontdata;
    TTF_GLTextEngineTextData *data;

    if (!SDL_FindInHashTable(enginedata->fonts, font, (const void **)&fontdata)) {
        fontdata = CreateFontData(enginedata, font, font_generation);
        if (!fontdata) {
            return false;
        }
    } else if (font_generation != fontdata->generation) {
        SDL_ClearHashTable(fontdata->glyphs);
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
    TTF_GLTextEngineTextData *data = (TTF_GLTextEngineTextData *)text->internal->engine_text;

    DestroyTextData(data);
}

void TTF_DestroyGLTextEngine(TTF_TextEngine *engine)
{
    if (!engine || engine->CreateText != CreateText) {
        return;
    }

    DestroyEngineData((TTF_GLTextEngineData *)engine->userdata);
    engine->CreateText = NULL;
    SDL_free(engine);
}

TTF_TextEngine *TTF_CreateGLTextEngine(void)
{
    SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0) {
        SDL_SetError("Failed to create GL text engine.");
        return NULL;
    }

    TTF_TextEngine *engine = TTF_CreateGLTextEngineWithProperties(props);
    SDL_DestroyProperties(props);
    return engine;
}

TTF_TextEngine *TTF_CreateGLTextEngineWithProperties(SDL_PropertiesID props)
{
    TTF_TextEngine *engine = (TTF_TextEngine *)SDL_malloc(sizeof(*engine));
    if (!engine) {
        return NULL;
    }

    int atlas_texture_size = (int)SDL_GetNumberProperty(props, TTF_PROP_GL_TEXT_ENGINE_ATLAS_TEXTURE_SIZE_NUMBER, 1024);
    if (atlas_texture_size <= 0) {
        SDL_SetError("Failed to create GL text engine: Invalid texture atlas size.");
        SDL_free(engine);
        return NULL;
    }

    SDL_INIT_INTERFACE(engine);
    engine->CreateText = CreateText;
    engine->DestroyText = DestroyText;
    engine->userdata = CreateEngineData(atlas_texture_size);
    if (!engine->userdata) {
        TTF_DestroyGLTextEngine(engine);
        return NULL;
    }
    return engine;
}

AtlasDrawSequence *TTF_GetGLTextDrawData(TTF_Text *text)
{
    if (!text || !text->internal || text->internal->engine->CreateText != CreateText) {
        SDL_InvalidParamError("text");
        return NULL;
    }

    // Make sure the text is up to date
    if (!TTF_UpdateText(text)) {
        return NULL;
    }

    TTF_GLTextEngineTextData *data = (TTF_GLTextEngineTextData *)text->internal->engine_text;
    if (!data) {
        // Empty string, nothing to do
        return NULL;
    }

    return data->draw_sequence;
}

void TTF_SetGLTextEngineWinding(TTF_TextEngine *engine, TTF_GLTextEngineWinding winding)
{
    if (!engine || engine->CreateText != CreateText) {
        SDL_InvalidParamError("engine");
        return;
    }

    if (winding == TTF_GL_TEXTENGINE_WINDING_INVALID) {
        SDL_InvalidParamError("winding");
        return;
    }

    ((TTF_GLTextEngineData *)engine->userdata)->winding = winding;
}

TTF_GLTextEngineWinding TTF_GetGLTextEngineWinding(const TTF_TextEngine *engine)
{
    if (!engine || engine->CreateText != CreateText) {
        SDL_InvalidParamError("engine");
        return TTF_GL_TEXTENGINE_WINDING_INVALID;
    }

    return ((TTF_GLTextEngineData *)engine->userdata)->winding;
}
