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

typedef void (*SDL_GlyphHashTable_NukeFn)(const void *value);

extern SDL_HashTable *SDL_CreateGlyphHashTable(SDL_GlyphHashTable_NukeFn nukefn);
extern bool SDL_InsertIntoGlyphHashTable(SDL_HashTable *table, TTF_Font *font, Uint32 glyph_index, const void *value);
extern  bool SDL_FindInGlyphHashTable(SDL_HashTable *table, TTF_Font *font, Uint32 glyph_index, const void **value);
extern void SDL_DestroyGlyphHashTable(SDL_HashTable *table);

