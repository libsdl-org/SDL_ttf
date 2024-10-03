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
// Combining characters
//#define DEFAULT_TEXT    "\xc5\xab\xcc\x80\x20\xe1\xba\x83\x20\x6e\xcc\x82\x20\x48\xcc\xa8\x20\x6f\xcd\x9c\x75"
// Chinese text
//#define DEFAULT_TEXT    "\xe5\xad\xa6\xe4\xb9\xa0\xe6\x9f\x90\xe8\xaf\xbe\xe7\xa8\x8b\xe5\xbf\x85\xe8\xaf\xbb\xe7\x9a\x84"
#define WIDTH   640
#define HEIGHT  480
#define CURSOR_BLINK_INTERVAL_MS    500


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
    TTF_Font *font;
    SDL_Texture *caption;
    SDL_FRect captionRect;
    SDL_Texture *message;
    SDL_FRect messageRect;
    TextEngine textEngine;
    TTF_Text *text;
    SDL_FRect textRect;
    bool textFocus;
    int cursor;
    bool cursorVisible;
    Uint64 lastCursorChange;
} Scene;

static void DrawScene(Scene *scene)
{
    SDL_Renderer *renderer = scene->renderer;

    /* Clear the background to background color */
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(renderer);

    if (scene->text) {
        float x = scene->textRect.x + 4.0f;
        float y = scene->textRect.y + 4.0f;

        /* Clear the text rect to light gray */
        SDL_SetRenderDrawColor(renderer, 0xCC, 0xCC, 0xCC, 0xFF);
        SDL_RenderFillRect(renderer, &scene->textRect);

        if (scene->textFocus) {
            SDL_FRect focusRect = scene->textRect;
            focusRect.x -= 1;
            focusRect.y -= 1;
            focusRect.w += 2;
            focusRect.h += 2;
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
            SDL_RenderRect(renderer, &focusRect);
        }

        switch (scene->textEngine) {
        case TextEngineSurface:
            /* Flush the renderer so we can draw directly to the window surface */
            SDL_FlushRenderer(renderer);
            TTF_DrawSurfaceText(scene->text, (int)x, (int)y, scene->window_surface);
            break;
        case TextEngineRenderer:
            TTF_DrawRendererText(scene->text, x, y);
            break;
        default:
            break;
        }

        if (scene->textFocus) {
            /* Draw the cursor */
            Uint64 now = SDL_GetTicks();
            if ((now - scene->lastCursorChange) >= CURSOR_BLINK_INTERVAL_MS) {
                scene->cursorVisible = !scene->cursorVisible;
                scene->lastCursorChange = now;
            }

            TTF_SubString cursor;
            if (scene->cursorVisible && TTF_GetTextSubString(scene->text, scene->cursor, &cursor)) {
                SDL_FRect cursorRect;
                if (TTF_GetFontDirection(scene->font) == TTF_DIRECTION_RTL) {
                    cursorRect.x = x + cursor.rect.x + cursor.rect.w;
                } else {
                    cursorRect.x = x + cursor.rect.x;
                }
                cursorRect.y = y + cursor.rect.y;
                cursorRect.w = 1.0f;
                cursorRect.h = (float)cursor.rect.h;

                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
                SDL_RenderFillRect(renderer, &cursorRect);
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

static int GetCursorTextIndex(TTF_Font *font, int x, const TTF_SubString *substring)
{
    bool round_down;
    if (TTF_GetFontDirection(font) == TTF_DIRECTION_RTL) {
        round_down = (x > (substring->rect.x + substring->rect.w / 2));
    } else {
        round_down = (x < (substring->rect.x + substring->rect.w / 2));
    }
    if (round_down) {
        /* Start the cursor before the selected text */
        return substring->offset;
    } else {
        /* Place the cursor after the selected text */
        return substring->offset + substring->length;
    }
}

static void MoveCursorIndex(Scene *scene, int direction)
{
    TTF_SubString substring;

    if (direction < 0) {
        if (TTF_GetTextSubString(scene->text, scene->cursor - 1, &substring)) {
            scene->cursor = substring.offset;
        }
    } else {
        if (TTF_GetTextSubString(scene->text, scene->cursor, &substring) &&
            TTF_GetTextSubString(scene->text, substring.offset + substring.length, &substring)) {
            scene->cursor = substring.offset;
        }
    }
}

static void MoveCursorLeft(Scene *scene)
{
    if (TTF_GetFontDirection(scene->font) == TTF_DIRECTION_RTL) {
        MoveCursorIndex(scene, 1);
    } else {
        MoveCursorIndex(scene, -1);
    }
}

static void MoveCursorRight(Scene *scene)
{
    if (TTF_GetFontDirection(scene->font) == TTF_DIRECTION_RTL) {
        MoveCursorIndex(scene, -1);
    } else {
        MoveCursorIndex(scene, 1);
    }
}

static void MoveCursorUp(Scene *scene)
{
    TTF_SubString substring;

    if (TTF_GetTextSubString(scene->text, scene->cursor, &substring)) {
        int fontHeight = TTF_GetFontHeight(scene->font);
        int x, y;
        if (TTF_GetFontDirection(scene->font) == TTF_DIRECTION_RTL) {
            x = substring.rect.x + substring.rect.w;
        } else {
            x = substring.rect.x;
        }
        y = substring.rect.y - fontHeight;
        if (TTF_GetTextSubStringAtPoint(scene->text, x, y, &substring)) {
            scene->cursor = GetCursorTextIndex(scene->font, x, &substring);
        }
    }
}

static void MoveCursorDown(Scene *scene)
{
    TTF_SubString substring;

    if (TTF_GetTextSubString(scene->text, scene->cursor, &substring)) {
        int fontHeight = TTF_GetFontHeight(scene->font);
        int x, y;
        if (TTF_GetFontDirection(scene->font) == TTF_DIRECTION_RTL) {
            x = substring.rect.x + substring.rect.w;
        } else {
            x = substring.rect.x;
        }
        y = substring.rect.y + substring.rect.h + fontHeight;
        if (TTF_GetTextSubStringAtPoint(scene->text, x, y, &substring)) {
            scene->cursor = GetCursorTextIndex(scene->font, x, &substring);
        }
    }
}

static void SetTextFocus(Scene *scene, bool focused)
{
    if (!scene->text) {
        return;
    }

    scene->textFocus = focused;

    if (focused) {
        SDL_StartTextInput(scene->window);
    } else {
        SDL_StopTextInput(scene->window);
    }
}

static void HandleTextClick(Scene *scene, float x, float y)
{
    TTF_SubString substring;

    if (!scene->textFocus) {
        SetTextFocus(scene, true);
        return;
    }

    /* Set the cursor position */
    int textX = (int)SDL_roundf(x - (scene->textRect.x + 4.0f));
    int textY = (int)SDL_roundf(y - (scene->textRect.y + 4.0f));
    if (!TTF_GetTextSubStringAtPoint(scene->text, textX, textY, &substring)) {
        SDL_Log("Couldn't get cursor location: %s\n", SDL_GetError());
        return;
    }

    scene->cursor = GetCursorTextIndex(scene->font, textX, &substring);
}

static void Cleanup(int exitcode)
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
    int i;
    bool done = false;
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
        Cleanup(2);
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
        Cleanup(0);
    }

    /* Create a window */
    scene.window = SDL_CreateWindow("showfont demo", WIDTH, HEIGHT, 0);
    if (!scene.window) {
        SDL_Log("SDL_CreateWindow() failed: %s\n", SDL_GetError());
        Cleanup(2);
    }
    if (scene.textEngine == TextEngineSurface) {
        scene.window_surface = SDL_GetWindowSurface(scene.window);
        if (!scene.window_surface) {
            SDL_Log("SDL_CreateWindowSurface() failed: %s\n", SDL_GetError());
            Cleanup(2);
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
        Cleanup(2);
    }
    scene.font = font;

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
        Cleanup(2);
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
        scene.textRect.x = 8.0f;
        scene.textRect.y = scene.captionRect.y + scene.captionRect.h + 4.0f;
        scene.textRect.w = WIDTH / 2 - scene.textRect.x * 2;
        scene.textRect.h = scene.messageRect.y - scene.textRect.y - 16.0f;

        scene.text = TTF_CreateText_Wrapped(engine, font, message, 0, (int)scene.textRect.w - 8);
        if (scene.text) {
            scene.text->color.r = forecol->r / 255.0f;
            scene.text->color.g = forecol->g / 255.0f;
            scene.text->color.b = forecol->b / 255.0f;
            scene.text->color.a = forecol->a / 255.0f;
        }
    }

    /* Wait for a keystroke, and blit text on mouse press */
    while (!done) {
        while (SDL_PollEvent(&event)) {
            SDL_ConvertEventToRenderCoordinates(scene.renderer, &event);

            switch (event.type) {
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    {
                        SDL_FPoint pt = { event.button.x, event.button.y };
                        if (SDL_PointInRectFloat(&pt, &scene.textRect)) {
                            HandleTextClick(&scene, pt.x, pt.y);
                        } else if (scene.textFocus) {
                            SetTextFocus(&scene, false);
                        } else {
                            scene.messageRect.x = (event.button.x - text->w/2);
                            scene.messageRect.y = (event.button.y - text->h/2);
                            scene.messageRect.w = (float)text->w;
                            scene.messageRect.h = (float)text->h;
                        }
                    }
                    break;

                case SDL_EVENT_KEY_DOWN:
                    switch (event.key.key) {
                    case SDLK_A:
                        if (!scene.textFocus) {
                            /* Cycle alignment */
                            switch (TTF_GetFontWrapAlignment(font)) {
                            case TTF_HORIZONTAL_ALIGN_LEFT:
                                TTF_SetFontWrapAlignment(font, TTF_HORIZONTAL_ALIGN_CENTER);
                                break;
                            case TTF_HORIZONTAL_ALIGN_CENTER:
                                TTF_SetFontWrapAlignment(font, TTF_HORIZONTAL_ALIGN_RIGHT);
                                break;
                            case TTF_HORIZONTAL_ALIGN_RIGHT:
                                TTF_SetFontWrapAlignment(font, TTF_HORIZONTAL_ALIGN_LEFT);
                                break;
                            default:
                                SDL_Log("Unknown wrap alignment: %d\n", TTF_GetFontWrapAlignment(font));
                                break;
                            }
                        }
                        break;
                    case SDLK_B:
                        if (!scene.textFocus) {
                            /* Toggle bold style */
                            int style = TTF_GetFontStyle(font);
                            if (style & TTF_STYLE_BOLD) {
                                style &= ~TTF_STYLE_BOLD;
                            } else {
                                style |= TTF_STYLE_BOLD;
                            }
                            TTF_SetFontStyle(font, style);
                        }
                        break;
                    case SDLK_C:
                        if (scene.textFocus) {
                            /* Copy to clipboard */
                            if (event.key.mod & SDL_KMOD_CTRL) {
                                SDL_SetClipboardText(scene.text->text);
                            }
                        }
                        break;
                    case SDLK_I:
                        if (!scene.textFocus) {
                            /* Toggle italic style */
                            int style = TTF_GetFontStyle(font);
                            if (style & TTF_STYLE_ITALIC) {
                                style &= ~TTF_STYLE_ITALIC;
                            } else {
                                style |= TTF_STYLE_ITALIC;
                            }
                            TTF_SetFontStyle(font, style);
                        }
                        break;
                    case SDLK_O:
                        if (!scene.textFocus) {
                            /* Toggle font outline */
                            outline = TTF_GetFontOutline(font);
                            if (outline) {
                                outline = 0;
                            } else {
                                outline = 1;
                            }
                            TTF_SetFontOutline(font, outline);
                        }
                        break;
                    case SDLK_R:
                        if (!scene.textFocus) {
                            /* Toggle layout direction */
                            if (TTF_GetFontDirection(font) == TTF_DIRECTION_LTR) {
                                TTF_SetFontDirection(font, TTF_DIRECTION_RTL);
                            } else if (TTF_GetFontDirection(font) == TTF_DIRECTION_RTL) {
                                TTF_SetFontDirection(font, TTF_DIRECTION_LTR);
                            } else if (TTF_GetFontDirection(font) == TTF_DIRECTION_TTB) {
                                TTF_SetFontDirection(font, TTF_DIRECTION_BTT);
                            } else if (TTF_GetFontDirection(font) == TTF_DIRECTION_BTT) {
                                TTF_SetFontDirection(font, TTF_DIRECTION_TTB);
                            }
                        }
                        break;
                    case SDLK_S:
                        if (!scene.textFocus) {
                            /* Toggle strike-through style */
                            int style = TTF_GetFontStyle(font);
                            if (style & TTF_STYLE_STRIKETHROUGH) {
                                style &= ~TTF_STYLE_STRIKETHROUGH;
                            } else {
                                style |= TTF_STYLE_STRIKETHROUGH;
                            }
                            TTF_SetFontStyle(font, style);
                        }
                        break;
                    case SDLK_U:
                        /* Toggle underline style */
                        if (!scene.textFocus) {
                            int style = TTF_GetFontStyle(font);
                            if (style & TTF_STYLE_UNDERLINE) {
                                style &= ~TTF_STYLE_UNDERLINE;
                            } else {
                                style |= TTF_STYLE_UNDERLINE;
                            }
                            TTF_SetFontStyle(font, style);
                        }
                        break;
                    case SDLK_V:
                        if (scene.textFocus) {
                            if (event.key.mod & SDL_KMOD_CTRL) {
                                /* Paste from clipboard */
                                const char *text = SDL_GetClipboardText();
                                size_t length = SDL_strlen(text);
                                TTF_InsertTextString(scene.text, scene.cursor, text, length);
                                scene.cursor = (int)(scene.cursor + length);
                            }
                        }
                        break;
                    case SDLK_X:
                        if (scene.textFocus) {
                            if (event.key.mod & SDL_KMOD_CTRL) {
                                /* Copy to clipboard and delete text */
                                if (scene.text->text) {
                                    SDL_SetClipboardText(scene.text->text);
                                    TTF_DeleteTextString(scene.text, 0, -1);
                                }
                            }
                        }
                        break;
                    case SDLK_LEFT:
                        if (scene.textFocus) {
                            if (event.key.mod & SDL_KMOD_CTRL) {
                                /* Move to the beginning of the line (FIXME) */
                                scene.cursor = 0;
                            } else {
                                MoveCursorLeft(&scene);
                            }
                        }
                        break;
                    case SDLK_RIGHT:
                        if (scene.textFocus) {
                            if (event.key.mod & SDL_KMOD_CTRL) {
                                /* Move to the end of the line (FIXME) */
                            } else {
                                MoveCursorRight(&scene);
                            }
                        }
                        break;
                    case SDLK_UP:
                        if (scene.textFocus) {
                            if (event.key.mod & SDL_KMOD_CTRL) {
                                /* Move to the beginning of the text */
                                scene.cursor = 0;
                            } else {
                                MoveCursorUp(&scene);
                            }
                        } else {
                            /* Increase font size */
                            ptsize = TTF_GetFontSize(font);
                            TTF_SetFontSize(font, ptsize + 1.0f);
                        }
                        break;
                    case SDLK_DOWN:
                        if (scene.textFocus) {
                            if (event.key.mod & SDL_KMOD_CTRL) {
                                /* Move to the end of the text */
                                if (scene.text->text) {
                                    scene.cursor = (int)SDL_strlen(scene.text->text);
                                }
                            } else {
                                MoveCursorDown(&scene);
                            }
                        } else {
                            /* Decrease font size */
                            ptsize = TTF_GetFontSize(font);
                            TTF_SetFontSize(font, ptsize - 1.0f);
                        }
                        break;
                    case SDLK_HOME:
                        if (scene.textFocus) {
                            /* Move to the beginning of the text */
                            scene.cursor = 0;
                        }
                        break;
                    case SDLK_END:
                        if (scene.textFocus) {
                            /* Move to the end of the text */
                            if (scene.text->text) {
                                scene.cursor = (int)SDL_strlen(scene.text->text);
                            }
                        }
                        break;
                    case SDLK_BACKSPACE:
                        if (scene.textFocus) {
                            if (event.key.mod & SDL_KMOD_CTRL) {
                                /* Delete to the beginning of the string */
                                TTF_DeleteTextString(scene.text, 0, scene.cursor);
                                scene.cursor = 0;
                            } else if (scene.text->text) {
                                const char *start = &scene.text->text[scene.cursor];
                                const char *current = start;
                                /* Step back over the previous UTF-8 character */
                                do {
                                    if (current == scene.text->text) {
                                        break;
                                    }
                                    --current;
                                } while ((*current & 0xC0) == 0x80);

                                int length = (int)(start - current);
                                TTF_DeleteTextString(scene.text, scene.cursor - length, length);
                                scene.cursor -= length;
                            }
                        }
                        break;
                    case SDLK_DELETE:
                        if (scene.textFocus) {
                            if (event.key.mod & SDL_KMOD_CTRL) {
                                /* Delete to the end of the string */
                                TTF_DeleteTextString(scene.text, scene.cursor, -1);
                            } else if (scene.text->text) {
                                const char *start = &scene.text->text[scene.cursor];
                                const char *next = start;
                                size_t length = SDL_strlen(next);
                                SDL_StepUTF8(&next, &length);
                                length = (next - start);
                                TTF_DeleteTextString(scene.text, scene.cursor, (int)length);
                            }
                        }
                        break;
                    case SDLK_ESCAPE:
                        if (scene.textFocus) {
                            SetTextFocus(&scene, false);
                        } else {
                            done = true;
                        }
                        break;
                    default:
                        break;
                    }
                    break;

                case SDL_EVENT_TEXT_INPUT:
                    if (scene.text) {
                        size_t length = SDL_strlen(event.text.text);
                        TTF_InsertTextString(scene.text, scene.cursor, event.text.text, length);
                        scene.cursor = (int)(scene.cursor + length);
                    }
                    break;
                case SDL_EVENT_QUIT:
                    done = true;
                    break;
                default:
                    break;
            }
        }
        DrawScene(&scene);
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
    Cleanup(0);

    /* Not reached, but fixes compiler warnings */
    return 0;
}
