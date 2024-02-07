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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PTSIZE 18
#define DEFAULT_TEXT   "The quick brown fox jumped over the lazy dog."
#define WIDTH          1280
#define HEIGHT         800

#define TTF_SHOWFONT_USAGE                                                                                                                                                                                                                                    \
    "Usage:\n"                                                                                                                                                                                                                                                \
    "    %s [-windowsize w,h] [-solid] [-shaded] [-blended] [-wrapped] [-b] [-i] [-u] [-s] [-outline size] [-hintlight | -hintmono | -hintnone] [-nokerning] [-wrap] [-fgcol r,g,b,a] [-bgcol r,g,b,a] [-utf8txtfile pathName] <font>.ttf [ptsize] [text] \n" \
    "Example:\n"                                                                                                                                                                                                                                              \
    "    %s -b -wrap -fgcol 255,0,255,255 -bgcol 64,64,64,255 NotoSansSC.ttf 32 \"This is a test.\"\n"                                                                                                                                                        \
    "    %s -windowsize 1920,1200 -utf8txtfile content.txt NotoSansSC.ttf"

typedef enum
{
    TextRenderSolid,
    TextRenderShaded,
    TextRenderBlended
} TextRenderMethod;

typedef struct
{
    SDL_Texture *caption;
    SDL_FRect captionRect;
    SDL_Texture *message;
    SDL_FRect messageRect;
} Scene;

static void draw_scene(SDL_Renderer *renderer, Scene *scene)
{
    /* Clear the background to background color */
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(renderer);

    SDL_RenderTexture(renderer, scene->caption, NULL, &scene->captionRect);
    SDL_RenderTexture(renderer, scene->message, NULL, &scene->messageRect);
    SDL_RenderPresent(renderer);
}

static void cleanup(int exitcode)
{
    TTF_Quit();
    SDL_Quit();
    exit(exitcode);
}

void show_usage(const char *appName)
{
    SDL_Log(TTF_SHOWFONT_USAGE, appName, appName, appName);
}

int load_file(char **bufferAddr, const char *pathName)
{
    FILE *file = fopen(pathName, "rb");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);

    if (file_size == 0) {
        fclose(file);
        return 0;
    }

    fseek(file, 0, SEEK_SET);

    *bufferAddr = malloc(file_size + 1);
    char *buffer = *bufferAddr;
    memset(buffer, 0, file_size + 1);

    size_t bytesRead = fread(buffer, 1, file_size, file);
    if (bytesRead == 0) {
        perror("Error reading file");
        fclose(file);
        return 1;
    }

    if (fread(buffer, file_size, 1, file) != 0) {
        return 1;
    }

    for (int i = 0; i < file_size; ++i) {
        if (iscntrl(buffer[i])) {
            buffer[i] = 32;
        }
    }

    fclose(file);

    return 0;
}

int main(int argc, char *argv[])
{
    char *argv0 = argv[0];
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    SDL_Surface *text = NULL;
    Scene scene;
    int ptsize;
    int i, done;
    SDL_Color white = { 0xFF, 0xFF, 0xFF, 0 };
    SDL_Color black = { 0x00, 0x00, 0x00, 0 };
    SDL_Color *forecol;
    SDL_Color *backcol;
    SDL_Event event;
    TextRenderMethod rendermethod;
    int renderstyle;
    int outline;
    int hinting;
    int kerning;
    int wrap;
    int dump;
    int winWidth = WIDTH;
    int winHeight = HEIGHT;
    int wrapWidth = WIDTH - 10;
    enum
    {
        RENDER_LATIN1,
        RENDER_UTF8,
        RENDER_UNICODE
    } rendertype;
    char *message, *fileMessage, string[128];
    fileMessage = NULL;

    /* Look for special execution mode */
    dump = 0;
    /* Look for special rendering types */
    rendermethod = TextRenderShaded;
    renderstyle = TTF_STYLE_NORMAL;
    rendertype = RENDER_LATIN1;
    outline = 0;
    hinting = TTF_HINTING_NORMAL;
    kerning = 1;
    /* Default is black and white */
    forecol = &black;
    backcol = &white;

    if (argc == 1) {
        show_usage(argv0);
        return (1);
    }

    for (i = 1; argv[i] && argv[i][0] == '-'; ++i) {
        if (SDL_strcmp(argv[i], "-solid") == 0) {
            rendermethod = TextRenderSolid;
        } else if (SDL_strcmp(argv[i], "-shaded") == 0) {
            rendermethod = TextRenderShaded;
        } else if (SDL_strcmp(argv[i], "-blended") == 0) {
            rendermethod = TextRenderBlended;
        } else if (SDL_strcmp(argv[i], "-b") == 0) {
            renderstyle |= TTF_STYLE_BOLD;
        } else if (SDL_strcmp(argv[i], "-i") == 0) {
            renderstyle |= TTF_STYLE_ITALIC;
        } else if (SDL_strcmp(argv[i], "-u") == 0) {
            renderstyle |= TTF_STYLE_UNDERLINE;
        } else if (SDL_strcmp(argv[i], "-s") == 0) {
            renderstyle |= TTF_STYLE_STRIKETHROUGH;
        } else if (SDL_strcmp(argv[i], "-outline") == 0) {
            if (SDL_sscanf(argv[++i], "%d", &outline) != 1) {
                show_usage(argv0);
                return (1);
            }
        } else if (SDL_strcmp(argv[i], "-hintlight") == 0) {
            hinting = TTF_HINTING_LIGHT;
        } else if (SDL_strcmp(argv[i], "-hintmono") == 0) {
            hinting = TTF_HINTING_MONO;
        } else if (SDL_strcmp(argv[i], "-hintnone") == 0) {
            hinting = TTF_HINTING_NONE;
        } else if (SDL_strcmp(argv[i], "-nokerning") == 0) {
            kerning = 0;
        } else if (SDL_strcmp(argv[i], "-wrap") == 0) {
            wrap = 1;
        } else if (SDL_strcmp(argv[i], "-dump") == 0) {
            dump = 1;
        } else if (SDL_strcmp(argv[i], "-fgcol") == 0) {
            int r, g, b, a = 0xFF;
            if (SDL_sscanf(argv[++i], "%d,%d,%d,%d", &r, &g, &b, &a) < 3) {
                show_usage(argv0);
                return (1);
            }
            forecol->r = (Uint8)r;
            forecol->g = (Uint8)g;
            forecol->b = (Uint8)b;
            forecol->a = (Uint8)a;
        } else if (SDL_strcmp(argv[i], "-bgcol") == 0) {
            int r, g, b, a = 0xFF;
            if (SDL_sscanf(argv[++i], "%d,%d,%d,%d", &r, &g, &b, &a) < 3) {
                show_usage(argv0);
                return (1);
            }
            backcol->r = (Uint8)r;
            backcol->g = (Uint8)g;
            backcol->b = (Uint8)b;
            backcol->a = (Uint8)a;
        } else if (SDL_strcmp(argv[i], "-utf8txtfile") == 0) {
            load_file(&fileMessage, argv[++i]);
            rendertype = RENDER_UTF8;
            wrap = 1;
        } else if (SDL_strcmp(argv[i], "-windowsize") == 0) {
            if (SDL_sscanf(argv[++i], "%d,%d", &winWidth, &winHeight) < 2) {
                show_usage(argv0);
                return (1);
            }
            wrapWidth = winWidth;
        } else {
            show_usage(argv0);
            return (1);
        }
    }
    argv += i;
    argc -= i;

    /* Check usage */
    if (!argv[0]) {
        SDL_Log(TTF_SHOWFONT_USAGE, argv0);
        return (1);
    }

    /* Initialize the TTF library */
    if (TTF_Init() < 0) {
        SDL_Log("Couldn't initialize TTF: %s\n", SDL_GetError());
        SDL_Quit();
        return (2);
    }

    /* Open the font file with the requested point size */
    ptsize = 0;
    if (argc > 1) {
        ptsize = SDL_atoi(argv[1]);
    }
    if (ptsize == 0) {
        i = 2;
        ptsize = DEFAULT_PTSIZE;
    } else {
        i = 3;
    }
    font = TTF_OpenFont(argv[0], ptsize);
    if (font == NULL) {
        SDL_Log("Couldn't load %d pt font from %s: %s\n",
                ptsize, argv[0], SDL_GetError());
        cleanup(2);
    }
    TTF_SetFontStyle(font, renderstyle);
    TTF_SetFontOutline(font, outline);
    TTF_SetFontKerning(font, kerning);
    TTF_SetFontHinting(font, hinting);

    if (dump) {
        for (i = 48; i < 123; i++) {
            SDL_Surface *glyph = NULL;

            glyph = TTF_RenderGlyph_Shaded(font, i, *forecol, *backcol);

            if (glyph) {
                char outname[64];
                SDL_snprintf(outname, sizeof(outname), "glyph-%d.bmp", i);
                SDL_SaveBMP(glyph, outname);
            }
        }
        cleanup(0);
    }

    /* Create a window */
    if (SDL_CreateWindowAndRenderer(winWidth, winHeight, 0, &window, &renderer) < 0) {
        SDL_Log("SDL_CreateWindowAndRenderer() failed: %s\n", SDL_GetError());
        cleanup(2);
    }

    if (window) {
        SDL_SetWindowTitle(window, argv0);
    }

    /* Show which font file we're looking at */
    SDL_snprintf(string, sizeof(string), "Font file: %s", argv[0]); /* possible overflow */
    switch (rendermethod) {
    case TextRenderSolid:
        text = TTF_RenderText_Solid(font, string, *forecol);
        break;
    case TextRenderShaded:
        text = TTF_RenderText_Shaded(font, string, *forecol, *backcol);
        break;
    case TextRenderBlended:
        text = TTF_RenderText_Blended(font, string, *forecol);
        break;
    }
    if (text != NULL) {
        scene.captionRect.x = 4.0f;
        scene.captionRect.y = 4.0f;
        scene.captionRect.w = (float)text->w;
        scene.captionRect.h = (float)text->h;
        scene.caption = SDL_CreateTextureFromSurface(renderer, text);
        SDL_DestroySurface(text);
    }

    /* Render and center the message */
    if (fileMessage != NULL) {
        message = fileMessage;
    } else {
        if (argc > 2) {
            message = argv[2];
        } else {
            message = DEFAULT_TEXT;
        }
    }

    switch (rendertype) {
    case RENDER_LATIN1:
        switch (rendermethod) {
        case TextRenderSolid:
            if (wrap) {
                text = TTF_RenderText_Solid_Wrapped(font, message, *forecol, wrapWidth);
            } else {
                text = TTF_RenderText_Solid(font, message, *forecol);
            }
            break;
        case TextRenderShaded:
            if (wrap) {
                text = TTF_RenderText_Shaded_Wrapped(font, message, *forecol, *backcol, wrapWidth);
            } else {
                text = TTF_RenderText_Shaded(font, message, *forecol, *backcol);
            }
            break;
        case TextRenderBlended:
            if (wrap) {
                text = TTF_RenderText_Blended_Wrapped(font, message, *forecol, wrapWidth);
            } else {
                text = TTF_RenderText_Blended(font, message, *forecol);
            }
            break;
        }
        break;

    case RENDER_UTF8:
        switch (rendermethod) {
        case TextRenderSolid:
            if (wrap) {
                text = TTF_RenderUTF8_Solid_Wrapped(font, message, *forecol, wrapWidth);
            } else {
                text = TTF_RenderUTF8_Solid(font, message, *forecol);
            }
            break;
        case TextRenderShaded:
            if (wrap) {
                text = TTF_RenderUTF8_Shaded_Wrapped(font, message, *forecol, *backcol, wrapWidth);
            } else {
                text = TTF_RenderUTF8_Shaded(font, message, *forecol, *backcol);
            }
            break;
        case TextRenderBlended:
            if (wrap) {
                text = TTF_RenderUTF8_Blended_Wrapped(font, message, *forecol, wrapWidth);
            } else {
                text = TTF_RenderUTF8_Blended(font, message, *forecol);
            }
            break;
        }
        break;

    case RENDER_UNICODE:
    {
        Uint16 *unicode_text = SDL_iconv_utf8_ucs2(message);
        switch (rendermethod) {
        case TextRenderSolid:
            if (wrap) {
                text = TTF_RenderUNICODE_Solid_Wrapped(font, unicode_text, *forecol, wrapWidth);
            } else {
                text = TTF_RenderUNICODE_Solid(font, unicode_text, *forecol);
            }
            break;
        case TextRenderShaded:
            if (wrap) {
                text = TTF_RenderUNICODE_Shaded_Wrapped(font, unicode_text, *forecol, *backcol, wrapWidth);
            } else {
                text = TTF_RenderUNICODE_Shaded(font, unicode_text, *forecol, *backcol);
            }
            break;
        case TextRenderBlended:
            if (wrap) {
                text = TTF_RenderUNICODE_Blended_Wrapped(font, unicode_text, *forecol, wrapWidth);
            } else {
                text = TTF_RenderUNICODE_Blended(font, unicode_text, *forecol);
            }
            break;
        }
        SDL_free(unicode_text);
    } break;
    }
    if (text == NULL) {
        SDL_Log("Couldn't render text: %s\n", SDL_GetError());
        TTF_CloseFont(font);
        cleanup(2);
    }
    scene.messageRect.x = (float)((winWidth - text->w) / 2);
    scene.messageRect.y = (float)((winHeight - text->h) / 2);
    scene.messageRect.w = (float)text->w;
    scene.messageRect.h = (float)text->h;
    scene.message = SDL_CreateTextureFromSurface(renderer, text);
    SDL_Log("Font is generally %d big, and string is %d big\n",
            TTF_FontHeight(font), text->h);

    draw_scene(renderer, &scene);

    /* Wait for a keystroke, and blit text on mouse press */
    done = 0;
    while (!done) {
        if (!SDL_WaitEvent(&event)) {
            SDL_Log("SDL_PullEvent() error: %s\n", SDL_GetError());
            done = 1;
            continue;
        }
        switch (event.type) {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            scene.messageRect.x = (float)(event.button.x - text->w / 2);
            scene.messageRect.y = (float)(event.button.y - text->h / 2);
            scene.messageRect.w = (float)text->w;
            scene.messageRect.h = (float)text->h;
            draw_scene(renderer, &scene);
            break;

            // case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_QUIT:
            done = 1;
            break;
        default:
            break;
        }
    }
    SDL_DestroySurface(text);
    TTF_CloseFont(font);
    SDL_DestroyTexture(scene.caption);
    SDL_DestroyTexture(scene.message);
    cleanup(0);

    if (fileMessage != NULL) {
        free(fileMessage);
    }

    /* Not reached, but fixes compiler warnings */
    return 0;
}
