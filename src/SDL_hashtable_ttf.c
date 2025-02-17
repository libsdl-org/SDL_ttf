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
#include <SDL3_ttf/SDL_ttf.h>

#include "SDL_hashtable.h"
#include "SDL_hashtable_ttf.h"

typedef struct GlyphHashtableKey {
    TTF_Font *font;
    Uint32 glyph_index;
} GlyphHashtableKey;

static Uint32 SDLCALL SDL_HashGlyphHashtableKey(void *unused, const void *key)
{
    (void)unused;
    return SDL_murmur3_32(key, sizeof(GlyphHashtableKey), 0);
}

static bool SDLCALL SDL_KeyMatchGlyphHashtableKey(void *unused, const void *a, const void *b)
{
    (void)unused;
    GlyphHashtableKey *A = (GlyphHashtableKey *)a;
    GlyphHashtableKey *B = (GlyphHashtableKey *)b;
    return (A->font == B->font && A->glyph_index == B->glyph_index);
}

static void SDLCALL SDL_NukeFreeGlyphHashtableKey(void *data, const void *key, const void *value)
{
    SDL_GlyphHashTable_NukeFn nukefn = (SDL_GlyphHashTable_NukeFn)data;

    if (nukefn) {
        nukefn(value);
    }
    SDL_free((void *)key);
}

SDL_HashTable *SDL_CreateGlyphHashTable(SDL_GlyphHashTable_NukeFn nukefn)
{
    return SDL_CreateHashTable(128, false, SDL_HashGlyphHashtableKey, SDL_KeyMatchGlyphHashtableKey, SDL_NukeFreeGlyphHashtableKey, nukefn);
}

bool SDL_InsertIntoGlyphHashTable(SDL_HashTable *table, TTF_Font *font, Uint32 glyph_index, const void *value)
{
    GlyphHashtableKey *key = (GlyphHashtableKey *)SDL_calloc(1, sizeof(*key));
    key->font = font;
    key->glyph_index = glyph_index;
    return SDL_InsertIntoHashTable(table, key, value, true);
}

bool SDL_FindInGlyphHashTable(SDL_HashTable *table, TTF_Font *font, Uint32 glyph_index, const void **value)
{
    GlyphHashtableKey key;
    SDL_zero(key);
    key.font = font;
    key.glyph_index = glyph_index;
    return SDL_FindInHashTable(table, &key, value);
}

void SDL_DestroyGlyphHashTable(SDL_HashTable *table)
{
    SDL_DestroyHashTable(table);
}

