/*
  showfont:  An example of using the SDL_ttf library with 2D graphics.
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

/* A simple program to test the text rendering feature of the TTF library */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DEFAULT_PTSIZE  18.0f
#define DEFAULT_TEXT    "The quick brown fox jumped over the lazy dog"
#define WIDTH   640
#define HEIGHT  480

#define TTF_SHOWFONT_USAGE \
"Usage: %s [-textengine surface|renderer] [-shaded] [-blended] [-wrapped] [-b] [-i] [-u] [-s] [-outline size] [-hintlight|-hintmono|-hintnone] [-nokerning] [-wrap] [-align left|center|right] [-fgcol r,g,b,a] [-bgcol r,g,b,a] <font>.ttf [ptsize] [text]\n"

typedef enum
{
    TextEngineNone,
    TextEngineSurface,
    TextEngineRenderer
} TextEngine;

typedef enum
{
    TextRenderShaded,
    TextRenderBlended
} TextRenderMethod;

typedef struct {
    SDL_Window *window;
    SDL_Surface *window_surface;
    SDL_Renderer *renderer;
    SDL_Texture *caption;
    SDL_FRect captionRect;
    SDL_Texture *message;
    SDL_FRect messageRect;
    TextEngine textEngine;
    TTF_Text *text;
} Scene;

static void draw_scene(Scene *scene)
{
    SDL_Renderer *renderer = scene->renderer;

    /* Clear the background to background color */
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(renderer);

    /* Flush the renderer so we can draw to the window surface in TextEngineSurface mode */
    SDL_FlushRenderer(renderer);

    if (scene->text) {
        int i;
        int w, h;

        SDL_GetRenderOutputSize(renderer, &w, &h);

        for (i = 0; i < 100; ++i) {
            int x = SDL_rand(w) - scene->text->w / 2;
            int y = SDL_rand(h) - scene->text->h / 2;

            switch (scene->textEngine) {
            case TextEngineSurface:
                TTF_DrawSurfaceText(scene->text, x, y, scene->window_surface);
                break;
            case TextEngineRenderer:
                TTF_DrawRendererText(scene->text, (float)x, (float)y);
                break;
            default:
                break;
            }
        }
    }

    SDL_RenderTexture(renderer, scene->caption, NULL, &scene->captionRect);
    SDL_RenderTexture(renderer, scene->message, NULL, &scene->messageRect);
    SDL_RenderPresent(renderer);

    if (scene->window_surface) {
        SDL_UpdateWindowSurface(scene->window);
    }
}

static void cleanup(int exitcode)
{
    TTF_Quit();
    SDL_Quit();
    exit(exitcode);
}

int main(int argc, char *argv[])
{
    char *argv0 = argv[0];
    TTF_Font *font = NULL;
    SDL_Surface *text = NULL;
    Scene scene;
    float ptsize;
    int i, done;
    SDL_Color white = { 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE };
    SDL_Color black = { 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE };
    SDL_Color *forecol;
    SDL_Color *backcol;
    SDL_Event event;
    TTF_TextEngine *engine = NULL;
    TextRenderMethod rendermethod;
    int renderstyle;
    int outline;
    int hinting;
    int kerning;
    int wrap;
    TTF_HorizontalAlignment align = TTF_HORIZONTAL_ALIGN_LEFT;
    int dump;
    char *message, string[128];

    /* Look for special execution mode */
    dump = 0;
    /* Look for special rendering types */
    SDL_zero(scene);
    rendermethod = TextRenderShaded;
    renderstyle = TTF_STYLE_NORMAL;
    outline = 0;
    hinting = TTF_HINTING_NORMAL;
    kerning = 1;
    wrap = 0;
    /* Default is black and white */
    forecol = &black;
    backcol = &white;
    for (i=1; argv[i] && argv[i][0] == '-'; ++i) {
        if (SDL_strcmp(argv[i], "-textengine") == 0 && argv[i+1]) {
            ++i;
            if (SDL_strcmp(argv[i], "surface") == 0) {
                scene.textEngine = TextEngineSurface;
            }  else if (SDL_strcmp(argv[i], "renderer") == 0) {
                scene.textEngine = TextEngineRenderer;
            } else {
                SDL_Log(TTF_SHOWFONT_USAGE, argv0);
                return(1);
            }
        } else
        if (SDL_strcmp(argv[i], "-shaded") == 0) {
            rendermethod = TextRenderShaded;
        } else
        if (SDL_strcmp(argv[i], "-blended") == 0) {
            rendermethod = TextRenderBlended;
        } else
        if (SDL_strcmp(argv[i], "-b") == 0) {
            renderstyle |= TTF_STYLE_BOLD;
        } else
        if (SDL_strcmp(argv[i], "-i") == 0) {
            renderstyle |= TTF_STYLE_ITALIC;
        } else
        if (SDL_strcmp(argv[i], "-u") == 0) {
            renderstyle |= TTF_STYLE_UNDERLINE;
        } else
        if (SDL_strcmp(argv[i], "-s") == 0) {
            renderstyle |= TTF_STYLE_STRIKETHROUGH;
        } else
        if (SDL_strcmp(argv[i], "-outline") == 0 && argv[i+1]) {
            if (SDL_sscanf(argv[++i], "%d", &outline) != 1) {
                SDL_Log(TTF_SHOWFONT_USAGE, argv0);
                return(1);
            }
        } else
        if (SDL_strcmp(argv[i], "-hintlight") == 0) {
            hinting = TTF_HINTING_LIGHT;
        } else
        if (SDL_strcmp(argv[i], "-hintmono") == 0) {
            hinting = TTF_HINTING_MONO;
        } else
        if (SDL_strcmp(argv[i], "-hintnone") == 0) {
            hinting = TTF_HINTING_NONE;
        } else
        if (SDL_strcmp(argv[i], "-nokerning") == 0) {
            kerning = 0;
        } else
        if (SDL_strcmp(argv[i], "-wrap") == 0) {
            wrap = 1;
        } else
        if (SDL_strcmp(argv[i], "-align") == 0 && argv[i+1]) {
            ++i;
            if (SDL_strcmp(argv[i], "left") == 0) {
                align = TTF_HORIZONTAL_ALIGN_LEFT;
            } else if (SDL_strcmp(argv[i], "center") == 0) {
                align = TTF_HORIZONTAL_ALIGN_CENTER;
            } else if (SDL_strcmp(argv[i], "right") == 0) {
                align = TTF_HORIZONTAL_ALIGN_RIGHT;
            } else {
                SDL_Log(TTF_SHOWFONT_USAGE, argv0);
                return (1);
            }
        } else
        if (SDL_strcmp(argv[i], "-dump") == 0) {
            dump = 1;
        } else
        if (SDL_strcmp(argv[i], "-fgcol") == 0 && argv[i+1]) {
            int r, g, b, a = SDL_ALPHA_OPAQUE;
            if (SDL_sscanf(argv[++i], "%d,%d,%d,%d", &r, &g, &b, &a) < 3) {
                SDL_Log(TTF_SHOWFONT_USAGE, argv0);
                return(1);
            }
            forecol->r = (Uint8)r;
            forecol->g = (Uint8)g;
            forecol->b = (Uint8)b;
            forecol->a = (Uint8)a;
        } else
        if (SDL_strcmp(argv[i], "-bgcol") == 0 && argv[i+1]) {
            int r, g, b, a = SDL_ALPHA_OPAQUE;
            if (SDL_sscanf(argv[++i], "%d,%d,%d,%d", &r, &g, &b, &a) < 3) {
                SDL_Log(TTF_SHOWFONT_USAGE, argv0);
                return(1);
            }
            backcol->r = (Uint8)r;
            backcol->g = (Uint8)g;
            backcol->b = (Uint8)b;
            backcol->a = (Uint8)a;
        } else {
            SDL_Log(TTF_SHOWFONT_USAGE, argv0);
            return(1);
        }
    }
    argv += i;
    argc -= i;

    /* Check usage */
    if (!argv[0]) {
        SDL_Log(TTF_SHOWFONT_USAGE, argv0);
        return(1);
    }

    /* Initialize the TTF library */
    if (!TTF_Init()) {
        SDL_Log("Couldn't initialize TTF: %s\n",SDL_GetError());
        SDL_Quit();
        return(2);
    }

    /* Open the font file with the requested point size */
    ptsize = 0.0f;
    if (argc > 1) {
        ptsize = (float)SDL_atof(argv[1]);
    }
    if (ptsize == 0.0f) {
        i = 2;
        ptsize = DEFAULT_PTSIZE;
    } else {
        i = 3;
    }
    font = TTF_OpenFont(argv[0], ptsize);
    if (font == NULL) {
        SDL_Log("Couldn't load %g pt font from %s: %s\n",
                    ptsize, argv[0], SDL_GetError());
        cleanup(2);
    }
    TTF_SetFontStyle(font, renderstyle);
    TTF_SetFontOutline(font, outline);
    TTF_SetFontKerning(font, kerning);
    TTF_SetFontHinting(font, hinting);
    TTF_SetFontWrapAlignment(font, align);

    if(dump) {
        for(i = 48; i < 123; i++) {
            SDL_Surface* glyph = NULL;

            glyph = TTF_RenderGlyph_Shaded(font, i, *forecol, *backcol);

            if(glyph) {
                char outname[64];
                SDL_snprintf(outname, sizeof(outname), "glyph-%d.bmp", i);
                SDL_SaveBMP(glyph, outname);
            }

        }
        cleanup(0);
    }

    /* Create a window */
    scene.window = SDL_CreateWindow("showfont demo", WIDTH, HEIGHT, 0);
    if (!scene.window) {
        SDL_Log("SDL_CreateWindow() failed: %s\n", SDL_GetError());
        cleanup(2);
    }
    if (scene.textEngine == TextEngineSurface) {
        scene.window_surface = SDL_GetWindowSurface(scene.window);
        if (!scene.window_surface) {
            SDL_Log("SDL_CreateWindowSurface() failed: %s\n", SDL_GetError());
            cleanup(2);
        }
        SDL_SetWindowSurfaceVSync(scene.window, 1);

        scene.renderer = SDL_CreateSoftwareRenderer(scene.window_surface);
    } else {
        scene.renderer = SDL_CreateRenderer(scene.window, NULL);
        if (scene.renderer) {
            SDL_SetRenderVSync(scene.renderer, 1);
        }
    }
    if (!scene.renderer) {
        SDL_Log("SDL_CreateRenderer() failed: %s\n", SDL_GetError());
        cleanup(2);
    }

    /* Show which font file we're looking at */
    SDL_snprintf(string, sizeof(string), "Font file: %s", argv[0]);  /* possible overflow */
    switch (rendermethod) {
    case TextRenderShaded:
        text = TTF_RenderText_Shaded(font, string, 0, *forecol, *backcol);
        break;
    case TextRenderBlended:
        text = TTF_RenderText_Blended(font, string, 0, *forecol);
        break;
    }
    if (text != NULL) {
        scene.captionRect.x = 4.0f;
        scene.captionRect.y = 4.0f;
        scene.captionRect.w = (float)text->w;
        scene.captionRect.h = (float)text->h;
        scene.caption = SDL_CreateTextureFromSurface(scene.renderer, text);
        SDL_DestroySurface(text);
    }

    /* Render and center the message */
    if (argc > 2) {
        message = argv[2];
    } else {
        message = DEFAULT_TEXT;
    }
    switch (rendermethod) {
    case TextRenderShaded:
        if (wrap) {
            text = TTF_RenderText_Shaded_Wrapped(font, message, 0, *forecol, *backcol, 0);
        } else {
            text = TTF_RenderText_Shaded(font, message, 0, *forecol, *backcol);
        }
        break;
    case TextRenderBlended:
        if (wrap) {
            text = TTF_RenderText_Blended_Wrapped(font, message, 0, *forecol, 0);
        } else {
            text = TTF_RenderText_Blended(font, message, 0, *forecol);
        }
        break;
    }
    if (text == NULL) {
        SDL_Log("Couldn't render text: %s\n", SDL_GetError());
        TTF_CloseFont(font);
        cleanup(2);
    }
    scene.messageRect.x = (float)((WIDTH - text->w)/2);
    scene.messageRect.y = (float)((HEIGHT - text->h)/2);
    scene.messageRect.w = (float)text->w;
    scene.messageRect.h = (float)text->h;
    scene.message = SDL_CreateTextureFromSurface(scene.renderer, text);
    SDL_Log("Font is generally %d big, and string is %d big\n",
                        TTF_GetFontHeight(font), text->h);

    switch (scene.textEngine) {
    case TextEngineSurface:
        engine = TTF_CreateSurfaceTextEngine();
        if (!engine) {
            SDL_Log("Couldn't create surface text engine: %s\n", SDL_GetError());
        }
        break;
    case TextEngineRenderer:
        engine = TTF_CreateRendererTextEngine(scene.renderer);
        if (!engine) {
            SDL_Log("Couldn't create renderer text engine: %s\n", SDL_GetError());
        }
        break;
    default:
        break;
    }
    if (engine) {
        if (wrap) {
            scene.text = TTF_CreateText_Wrapped(engine, font, message, 0, 0);
        } else {
            scene.text = TTF_CreateText(engine, font, message, 0);
        }
        if (scene.text) {
            scene.text->color.r = forecol->r / 255.0f;
            scene.text->color.g = forecol->g / 255.0f;
            scene.text->color.b = forecol->b / 255.0f;
            scene.text->color.a = forecol->a / 255.0f;
        }
    }

    /* Wait for a keystroke, and blit text on mouse press */
    done = 0;
    while (!done) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    scene.messageRect.x = (float)(event.button.x - text->w/2);
                    scene.messageRect.y = (float)(event.button.y - text->h/2);
                    scene.messageRect.w = (float)text->w;
                    scene.messageRect.h = (float)text->h;
                    break;

                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_QUIT:
                    done = 1;
                    break;
                default:
                    break;
            }
        }
        draw_scene(&scene);
    }
    SDL_DestroySurface(text);
    TTF_CloseFont(font);
    TTF_DestroyText(scene.text);
    switch (scene.textEngine) {
    case TextEngineSurface:
        TTF_DestroySurfaceTextEngine(engine);
        break;
    case TextEngineRenderer:
        TTF_DestroyRendererTextEngine(engine);
        break;
    default:
        break;
    }
    SDL_DestroyTexture(scene.caption);
    SDL_DestroyTexture(scene.message);
    cleanup(0);

    /* Not reached, but fixes compiler warnings */
    return 0;
}
