/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#define SDL_CreateHashTable TTF_CreateHashTable
#define SDL_DestroyHashTable TTF_DestroyHashTable
#define SDL_InsertIntoHashTable TTF_InsertIntoHashTable
#define SDL_FindInHashTable TTF_FindInHashTable
#define SDL_RemoveFromHashTable TTF_RemoveFromHashTable
#define SDL_ClearHashTable TTF_ClearHashTable
#define SDL_HashTableEmpty TTF_HashTableEmpty
#define SDL_IterateHashTable TTF_IterateHashTable
#define SDL_HashPointer TTF_HashPointer
#define SDL_KeyMatchPointer TTF_KeyMatchPointer
#define SDL_HashString TTF_HashString
#define SDL_KeyMatchString TTF_KeyMatchString
#define SDL_HashID TTF_HashID
#define SDL_KeyMatchID TTF_KeyMatchID
#define SDL_DestroyHashKeyAndValue TTF_DestroyHashKeyAndValue
#define SDL_DestroyHashKey TTF_DestroyHashKey
#define SDL_DestroyHashValue TTF_DestroyHashValue
