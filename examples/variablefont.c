/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Demonstrates the OpenType variable-font axis API:
 *   - enumerate a font's variation axes
 *   - print each axis tag, its [min, default, max] range and current value
 *   - set the 'wght' (weight) axis and render a sample string at that weight
 *
 * Usage: variablefont <font.ttf> [ptsize] [wght]
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        SDL_Log("Usage: %s <font.ttf> [ptsize] [wght]", argv[0]);
        return 1;
    }

    const char *font_path = argv[1];
    float ptsize = (argc > 2) ? (float)SDL_atof(argv[2]) : 48.0f;

    if (!TTF_Init()) {
        SDL_Log("TTF_Init() failed: %s", SDL_GetError());
        return 1;
    }

    TTF_Font *font = TTF_OpenFont(font_path, ptsize);
    if (!font) {
        SDL_Log("TTF_OpenFont() failed: %s", SDL_GetError());
        TTF_Quit();
        return 1;
    }

    /* Enumerate the axes and print each tag with its range and current value. */
    int num_axes = TTF_GetNumFontAxes(font);
    if (num_axes <= 0) {
        SDL_Log("'%s' has no variable-font axes (TTF_GetNumFontAxes() returned %d).", font_path, num_axes);
    } else {
        SDL_Log("'%s' exposes %d variation axis/axes:", font_path, num_axes);
        for (int i = 0; i < num_axes; ++i) {
            TTF_AxisInfo info;
            char tag[5];
            float current = 0.0f;

            if (!TTF_GetFontAxisInfo(font, i, &info)) {
                SDL_Log("  axis %d: TTF_GetFontAxisInfo() failed: %s", i, SDL_GetError());
                continue;
            }
            const char *name = TTF_GetFontAxisName(font, i);

            TTF_TagToString(info.tag, tag, sizeof(tag));
            TTF_GetFontAxisValue(font, info.tag, &current);
            SDL_Log("  '%s' (%s): min=%.1f default=%.1f max=%.1f (current=%.1f)%s",
                    tag, name ? name : "?", info.min_value, info.default_value, info.max_value, current,
                    (info.flags & TTF_FONT_AXIS_HIDDEN) ? " [hidden]" : "");
        }
    }

    /* If the font has a weight axis, push it and render a sample. */
    Uint32 weight_tag = TTF_StringToTag("wght");
    TTF_AxisInfo wght;
    bool has_weight = false;
    for (int i = 0; i < num_axes; ++i) {
        if (TTF_GetFontAxisInfo(font, i, &wght) && wght.tag == weight_tag) {
            has_weight = true;
            break;
        }
    }

    if (has_weight) {
        float value = (argc > 3) ? (float)SDL_atof(argv[3]) : wght.max_value;
        SDL_Log("Setting 'wght' to %.1f ...", value);
        if (!TTF_SetFontAxisValue(font, weight_tag, value)) {
            SDL_Log("TTF_SetFontAxisValue() failed: %s", SDL_GetError());
        } else {
            SDL_Color white = { 255, 255, 255, 255 };
            SDL_Surface *surface;
            float now = 0.0f;

            TTF_GetFontAxisValue(font, weight_tag, &now);
            SDL_Log("'wght' is now %.1f", now);

            /* Render at the new weight and save it, to confirm the axis change
             * actually reaches the rasteriser. */
            surface = TTF_RenderText_Blended(font, "Variable 123", 0, white);
            if (surface) {
                if (SDL_SaveBMP(surface, "variablefont.bmp")) {
                    SDL_Log("Wrote variablefont.bmp (%dx%d)", surface->w, surface->h);
                }
                SDL_DestroySurface(surface);
            }
        }
    } else {
        SDL_Log("Font has no 'wght' axis; skipping the weight demo.");
    }

    TTF_CloseFont(font);
    TTF_Quit();
    return 0;
}
