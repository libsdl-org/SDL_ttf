/*
  testapp:  An example of using the SDL_ttf library with OpenGL.
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

/* A simple program to test the text rendering feature of the TTF library,
 * and find bugs
 *
 * <F1> for console help
 *
 * Use
 * - fill/replace test_fonts[] with your fonts
 * - replay a case, adding the log at line ~650 (search for 'replay')
 * - add more string to test_strings[]
 *
 */

static const int g_force_no_SDF = 0; /* make random fuzzer faster by disabling SDF rendering */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

static char *testapp_basename(const char *path) {
    static char buffer[1024];
    const char *pos, *sep, *prev_sep;
    int prev_was_sep = 0;

    prev_sep = sep = NULL;
    for (pos = path; *pos; pos++) {
        if (*pos == '/' || *pos == '\\') {
            if (!prev_was_sep) {
                prev_sep = sep;
            }
            sep = pos;
            prev_was_sep = 1;
        } else {
            prev_was_sep = 0;
        }
    }
    if (!sep) {
        if (path[0] == '\0') {
            buffer[0] = '.';
            buffer[1] = '\0';
        } else {
            SDL_strlcpy(buffer, path, sizeof(buffer));
        }
    } else {
        if (sep[1] == '\0') {
            if (prev_sep) {
                char *s;
                SDL_strlcpy(buffer, prev_sep + 1, sizeof(buffer));
                SDL_strtok_r(buffer, "/\\", &s);
            } else {
                buffer[0] = *sep;
                buffer[1] = '\0';
            }
        } else {
            SDL_strlcpy(buffer, sep + 1, sizeof(buffer));
        }
    }
    return buffer;
}

static void help(void)
{
    SDL_Log("up/down : font size -/+");
    SDL_Log("c/v : next/previous font");
    SDL_Log("o/p : outline   -/+");
    SDL_Log("q/e : wrap size -/+");
    SDL_Log("a : align wrap: left center right");
    SDL_Log("w   : wrap");
    SDL_Log("u   : underline");
    SDL_Log("k   : kerning");
    SDL_Log("s   : strike-through");
    SDL_Log("b   : bold");
    SDL_Log("g/h : hinting -/+ (normal, light, light_subpix, mono, none)");
    SDL_Log("i   : italic");
    SDL_Log("f   : signed distance field");
    SDL_Log("t   : ticks elapsed for 50 rendering");
    SDL_Log("d   : display normal texture, no screen update, stream texture ");
    SDL_Log("r   : start/stop random test");
    SDL_Log("m   : render mode Solid/Blended/Shaded");
    SDL_Log("n   : change direction");
    SDL_Log("9/0 : -/+ alpha color fg");
    SDL_Log("7/8 : -/+ alpha color bg (Shaded only)");
    SDL_Log("6   : invert bg/fg colors");
    SDL_Log("345 : increase r/g/b fg color");
    SDL_Log("2   : background color (Black->R->G->B->W)");
    SDL_Log("<space> : next string/rendering function");
    SDL_Log("<backspace> : previous string/rendering function");
    SDL_Log("F1   : help");
    SDL_Log("F2   : save current rendering to .bmp");
}

#define HAVE_LCD
#define HAVE_SDF

#define HAVE_SET_FONT_SIZE_FUNCTION
// Need patch to set size dynamically
// https://bugzilla.libsdl.org/show_bug.cgi?id=2487

#define HAVE_ALL_WRAPPED_FUNCTIONS
// Need patch to have all wrapped functions
// https://bugzilla.libsdl.org/show_bug.cgi?id=4361

#define HAVE_WRAP_ALIGN



static int rand_n(int n);
static void init_rand(void);

static const char *test_fonts[] = {
    "fonts/NotoColorEmoji.ttf",  // Colored Emoji
    "fonts/FletcherGothicFLF.ttf",
#if 1
    /* CFF/OpenType font (.otf) */
    "fonts/ITCAvantGardeStd-Demi.ttf", // BUG 4266
#endif
    "fonts/DroidSansFallback.ttf", // xstart negative Jap
    "fonts/Папка/Lazy.ttf",
    "fonts/DroidSans.ttf", // no kerning here ?
    "fonts/DejaVuSans.ttf",
    "fonts/digital-7.ttf", // BUG 969
    "fonts/HelveticaNeue-Regular.ttf",// BUG 2622
    "fonts/ttf_amiga/amiga4ever pro2.ttf",
    "fonts/ttf-bitstream-vera-1.10/VeraMoBI.ttf",
    "fonts/ttf-bitstream-vera-1.10/VeraBd.ttf",
    "fonts/ttf-bitstream-vera-1.10/Vera.ttf",
    "fonts/nonscalable/sazanami-gothic.ttf", // BUG 190  crash?.  Both Scalable and Fixed Sizes
    "fonts/drakono.ttf", // BUG 2629 ........ ??????
    "fonts/Lazy.ttf", // BUG 2468
    "fonts/font_bug254.ttf", // BUG 254
    "fonts/OpenSansEmoji.ttf", // BUG 3762
#if 1
    "fonts/nonscalable/pvfixed_20b.pcf.gz",
    "fonts/nonscalable/pvfixed_20r.pcf.gz",
    "fonts/nonscalable/7x13B-ISO8859-15.pcf.gz",
    "fonts/nonscalable/vgasys.fon", // BUG 2574
    "fonts/nonscalable/fixed8.fon", // BUG 2127
#endif
#if 0
# include "fonts/fuzzing_corpora.h"
#endif
#if 0
# include "fonts/android_fonts_others.h"
# include "fonts/android_fonts.h"
#endif
    "fonts/Kaumudi.ttf"
};
static const int test_fonts_count = SDL_arraysize(test_fonts);


static int wrap_size = 137;
static int w_align = 0;
static int outline = 0;
static int font_style = 0;
static int kerning = 1;
static int wrap = 0;
static int sdf = 0;
static int hinting = 0;
static int curr_str = 0;
static int curr_font = 1;
static int curr_size = 50;
static int fg_alpha = 0;
static int bg_alpha = 0;
static int background_color = 0;

static int seed = 0;
static int print_elapsed_ticks = 0;
static int update_screen_mode = 0;
static int save_to_bmp = 0;

/* RENDER_SOLID = 0, RENDER_BLENDED = 1, RENDER_SHADED = 2, RENDER_LCD = 3 } */
static int render_mode = -1;
static int render_mode_overwrite;
static const char *render_mode_desc[] = { "Solid", "Blended", "Shaded", "LCD" };
static const int render_mode_count = SDL_arraysize(render_mode_desc);

static int direction = 0;
static const struct {
    const char *description;
    int value;
} directions[] = {
    { "LTR", TTF_DIRECTION_LTR },
    { "RTL", TTF_DIRECTION_RTL },
    { "TTB", TTF_DIRECTION_TTB },
    { "BTT", TTF_DIRECTION_BTT }
};
static const int direction_count = SDL_arraysize(directions);


//static const char *hinting_desc[] = { "normal", "light", "light_subpix", "lcd_subpix", "mono", "none" };
static const char *hinting_desc[] = { "normal", "light", "light_subpix", "mono", "none" };
static const int hinting_count = SDL_arraysize(hinting_desc);

static int mode_random_test = 0;
static int random_cnt = 0;

static int saved_curr_font = -1;
static int saved_curr_size = -1;

static SDL_Color     boardcol       = { 0, 0, 0, 0 };
static SDL_Color     textcol        = { 255, 255, 255, 0 };


static const char emoji[5] = { 0xF0, 0x9F, 0x98, 0x81, '\0'};

static const char *test_strings[] = {
// TMP BUG
#if 0
"وفي حالة كشف مربع يحتوي على لغم، يخسر اللاعب اللعبة.",
#endif
#if 1
   "纸牌接龙",
    //"空当接龙用一副没有大小王的52张扑克牌来玩，这些扑克牌被摆在8列中。",
#endif
    "ABC",
    "The quick \n brown fox \n jumps over the lazy dog 0123456789",
    "iiiiiiiiiiiiiiiiiiiiiiiiiiiiiii", // test supixel
    emoji,
/* static const char emoji[5] = { 0xF0, 0x9F, 0x98, 0x81, '\0'}; */
#if 1
    "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz "
        "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz "
        "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz "
        ,
#endif
    "aTa kerning iTo",
    "AV VA Te eT Tr rT Td TA LV Vd pV Vq bV", // kerning
    "if fi ifi aif fai aifia substitution",
    "abc \xc4\x80", // String would be both outline and bitmap for Sazami-Gothic
#if 1
    "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz "
        "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz "
        "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz " "abcdefghijklmnopqrstuvwxyz "
        ,
#endif
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "0123456789",
    "=",
    "~",
    "`~!@#$%^&*()_-+={}|[]\\;:'\",<.>/?",
#if 1
    "Å", // ystart negative with DroidSans
    "A\n\b\nB",
    "Tap Å",
    /* xstart negative */
    "Jap",
    "A\315\241B", /* 'A' + U+0361 + 'B' */
    "\315\241BC", /* '.' + U+0361 + 'B' + 'C' */
    ".\315\241BC", /* '.' + U+0361 + 'B' + 'C' */
    ".\315\241BC\naaaaa", /* '.' + U+0361 + 'B' + 'C' */
    ".aaaaa\n.\315\241BC", /* '.' + U+0361 + 'B' + 'C' */

    "aaa\n.\315\241BCaa\n.aaa\n.\315\241BCaaaaa", /* '.' + U+0361 + 'B' + 'C' */
    "Tap Å  Å Å Å Å Å Å Å Å Å  Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å"
        "Tap Å  Å Å Å Å Å Å Å Å Å  Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å"
        "Tap Å  Å Å Å Å Å Å Å Å Å  Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å Å",
#endif
    "+", "2", ")", "|",
    "a\tsdfsad.f..| I V I A AV A V D \n F G H ",
    "é ê è à â ô î ï fi if ñ ç Ã Â",


"Kl1AqvJAUOOKzdjpGTteM775hTYEuqvV b3EU0RZ88JIAMRVS7rMbyKsF1xMg9RTe",
"CGOXraiYCL44XSlvqx825GvZT3HQYXNQcVNjgzrccX1eUar8nG0FZ3MsYdB5caGqiG7l7RJCyNZFRUVQ8rEVnc0TJVrY3ef4v8miQpVvYswTjjZKTZLwwFxLbsHhaDQ3",
"7I8DRXPfbKUIl6SH5V6Twqw6mzEzTlDf",
"JJsSGv1EA MpN9FIctQxi9D iHgEQZe2Wg",
"xqKyuaT8H2xmySzWdbo wgMZ4Sp2BLUGj",
"hUEKUunoouIjw6Cu jdXnPkQzMNGG7U8u",
"4GDEgeCgEXwxoszBpriddAibgbhh36Vz",
"FdkAi hx8B3QvmjFXovy64TzYwaZ0uoW8",
"zAcp1V4PmI5dBv wtnbHqN1ISyrXB7P32",
"lJq5w9GNhpqveQ67kxyQlgAwgQz1AAj3",
"yi0jssXmCMjnFjl2rWs0rNIx51iVNLoI",
"HT1BYxlX3z39XBFEw8JMNwYdWeQrO8h2",
"aQmJI2hUruwhqvJeWL3 gZCGAZ09q67nR",
"rxGWF OPfQdxUR0cYRenCccz5XgSpTDqf",
"4NI1L2KbwybObecSDAAgb0K00oCEkf3E",
"heBlACb AfcK9G85Gl6mBpBUiblc81FSe",
"GwWx6pHvWyvxSu82PbJbZI UNwx9JSATu",
"AFCQd25jnbBJ7LBbaXI3I5IeSwDXqME6",
"lzSqw9t7hJ03AkTjWWUyd3KqrnGAvPJg",
"pxwCKuWxUOrIMvqXGL 5fbibttAPNIJFC",
"Fs6sFtGnRezLttwm8AbSKsuvLGbcqFbZ",
"hEdVvdwWDwsDA23dX0NjDqzYEaURRC7a",
"IBrdiqo3g0tj LkXz1a45DJD52tO8FJSw",
"h5QYsL9S1cfsWzTyyFJ6Npq7vLi1tjEX",
"zhUkVyCPdhWspT9kBQpOmmREmSU6GPW3",
"Ud1ih3stL5nq00PJMRoA1Zd1QJi0mOzj",
"iVppAFr5pGYa4oT8OozDi0oqGvqJg3Eg",
"YC9YUpKq95nIe7D3ckdc6Yy1cEmmuqf1",
"pzmZvs5j77aVaXXnPqzcuU9fW4Kfmiv5",
"04ifd9OyA5XGsLTovRo04MnIKToW9Qu3",
"iDJcbD5si6xPRcPRBEVeEhcJSs7n65SD",
"a4oZMmgJQAa72o6sllH4n0zfJE5H0HlA",
"5eSVmFkJMmPCRKdTZRJeuW71uhprmYHp",
"LcnSHbxguPfqPNhdDR4XDc2qv01Q4Y1Y",
"H3wT59MiQmEBZ7tYGMt1xMUu7wajECo6",
"JNGUtcQz660ApgN54rk2UzpqWimO25Cy",
"ss887dpFwjPaqAg53K6qZnc1NFiXc9WX",
"y9Th4vRk1jSpKlzB5soK66ckzl3USZq6",
"WGWY5YmWhaA5oKCoDmRA3n82XwclqvSP",
"hvlSZ6jE2BU9ImjUjQiY255GA5ASfUUx",
"e9o4tBCA9TCpilOI05UyHzes6s8lP9lQ",
"gJyC26gZsCKR8wSp9kNMYKJRRgA3u45U1DWdzPCWv1SUEKi3Wdo3zNFTWiMcCfcl5A0MzOhbqRee7OP13NruY WP0ufGiB4W9RBWqgSy7umnE6puTyCc9WhPOzdLz168BhQwYetZkADWibObi8jcYajuUv54zxXkXQwC1B8lAi8rIH9lmIy0G10fQ832HKiLx"
};
static const int test_strings_count = SDL_arraysize(test_strings);
static const char *font_path = NULL;

static TTF_Font *font = NULL;
static int iter = 0, sum = 0;

static void quit(const char *msg)
{
    SDL_Log("ERROR: %s", msg);
    SDL_Log("SDL_GetError: %s", SDL_GetError());
    exit(1);
}

static void random_input(void);

static int wait_for_input(void)
{
    int done = 0;
    SDL_Event event;

    while (!done) {
        if (!SDL_WaitEvent(&event)) {
            quit("Event handling");
        }

        switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_F1) {
                    help();
                    done = 1;
                }
                if (event.key.key == SDLK_SPACE) {
                    iter++;
                    sum++;
                    done = 1;
                }
                if (event.key.key == SDLK_BACKSPACE) {
                    iter--;
                    sum++;
                    done = 1;
                }
                if (event.key.key == SDLK_DOWN) {
                    curr_size -= 1;
                    if (curr_size < -1) curr_size = -1;
                    done = 1;
                    SDL_Log("size: %d", curr_size);
                }
                if (event.key.key == SDLK_UP) {
                    curr_size += 1;
                    done = 1;
                    SDL_Log("size: %d", curr_size);
                }
                if (event.key.key == SDLK_C) {
                    curr_font -= 1;
                    if (curr_font < 0) curr_font = 0;
                    done = 1;
                    SDL_Log("Switch to font %s", test_fonts[curr_font]);
                }
                if (event.key.key == SDLK_V) {
                    curr_font += 1;
                    done = 1;
                    if (curr_font >= test_fonts_count) curr_font = test_fonts_count - 1;
                    SDL_Log("Switch to font %s", test_fonts[curr_font]);
                }

                if (event.key.key == SDLK_O) {
                    outline -= 1;
                    if (outline < -1) outline = -1;
                    done = 1;
                    SDL_Log("outline: %d", outline);
                }
                if (event.key.key == SDLK_P) {
                    outline += 1;
                    done = 1;
                    SDL_Log("outline: %d", outline);
                }
                if (event.key.key == SDLK_Q) {
                    wrap_size -= 1;
                    done = 1;
                    SDL_Log("wrap_size: %d", wrap_size);
                }
                if (event.key.key == SDLK_A) {
                    w_align += 1;
                    if (w_align == 3) w_align = 0;
                    done = 1;
                    SDL_Log("wrap_align: %d", w_align);
                }
                if (event.key.key == SDLK_E) {
                    wrap_size += 1;
                    done = 1;
                    SDL_Log("wrap_size: %d", wrap_size);
                }
                if (event.key.key == SDLK_I) {
                    int s = TTF_STYLE_ITALIC;
                    SDL_Log("italic %s", (font_style & s) ? "removed" : "added");
                    font_style ^= s;
                    done = 1;
                }
                if (event.key.key == SDLK_B) {
                    int s = TTF_STYLE_BOLD;
                    SDL_Log("bold %s", (font_style & s) ? "removed" : "added");
                    font_style ^= s;
                    done = 1;
                }
                if (event.key.key == SDLK_U) {
                    int s = TTF_STYLE_UNDERLINE;
                    SDL_Log("underline %s", (font_style & s) ? "removed" : "added");
                    font_style ^= s;
                    done = 1;
                }
                if (event.key.key == SDLK_S) {
                    int s = TTF_STYLE_STRIKETHROUGH;
                    SDL_Log("strike-through %s", (font_style & s) ? "removed" : "added");
                    font_style ^= s;
                    done = 1;
                }
                if (event.key.key == SDLK_K) {
                    done = 1;
                    kerning ^= 1;
                    if (kerning) {
                        SDL_Log("kerning allowed");
                    } else {
                        SDL_Log("kerning removed");
                    }
                }
                if (event.key.key == SDLK_W) {
                    done = 1;
                    wrap ^= 1;
                    if (wrap) {
                        SDL_Log("wrap allowed");
                    } else {
                        SDL_Log("wrap removed");
                    }
                }
                if (event.key.key == SDLK_F) {
                    done = 1;
                    sdf ^= 1;
                    if (sdf) {
                        SDL_Log("SDF allowed");
                    } else {
                        SDL_Log("SDF removed");
                    }
                }
                if (event.key.key == SDLK_T) {
                    done = 1;
                    print_elapsed_ticks ^= 1;
                    if (print_elapsed_ticks) {
                        SDL_Log("print_elapsed_ticks displayed");
                    } else {
                        SDL_Log("print_elapsed_ticks hidden");
                    }
                }
                if (event.key.key == SDLK_D) {
                    done = 1;
                    update_screen_mode += 1;
                    update_screen_mode %= 2;

                    if (update_screen_mode == 0) {
                        SDL_Log("texture displayed");
                    } else if (update_screen_mode == 1) {
                        SDL_Log("texture not displayed");
                    }
                }
                if (event.key.key == SDLK_G) {
                    done = 1;
                    hinting -= 1;
                    if (hinting < 0) hinting = 0;
                    SDL_Log("hinting: %s", hinting_desc[hinting]);
                }
                if (event.key.key == SDLK_H) {
                    done = 1;
                    hinting += 1;
                    hinting %= hinting_count;
                    SDL_Log("hinting: %s", hinting_desc[hinting]);
                }
                if (event.key.key == SDLK_R) {
                    done = 1;
                    mode_random_test = 1;
                    random_cnt = 0;
                    SDL_Log("start random test");
                }
                if (event.key.key == SDLK_M) {
                    done = 1;
                    render_mode += 1;
                    render_mode %= render_mode_count;
                    render_mode_overwrite = render_mode;
                    SDL_Log("render mode: %s", render_mode_desc[render_mode]);
                }
                if (event.key.key == SDLK_N) {
                    done = 1;
                    direction += 1;
                    direction %= direction_count;
                    SDL_Log("direction: %s", directions[direction].description);
                }
                if (event.key.key == SDLK_F2) {
                    done = 1;
                    save_to_bmp = 1;
                }
                if (event.key.key == SDLK_9) {
                    done = 1;
                    fg_alpha -= 1;
                    if (fg_alpha < 0) fg_alpha = 0;
                    SDL_Log("color fg alpha = %d", fg_alpha);
                }
                if (event.key.key == SDLK_0) {
                    done = 1;
                    fg_alpha += 1;
                    if (fg_alpha > 255) fg_alpha = 255;
                    SDL_Log("color fg alpha = %d", fg_alpha);
                }
                if (event.key.key == SDLK_7) {
                    done = 1;
                    bg_alpha -= 1;
                    if (bg_alpha < 0) bg_alpha = 0;
                    SDL_Log("color bg alpha = %d", bg_alpha);
                }
                if (event.key.key == SDLK_8) {
                    done = 1;
                    bg_alpha += 1;
                    if (bg_alpha > 255) bg_alpha = 255;
                    SDL_Log("color bg alpha = %d", bg_alpha);
                }
                if (event.key.key == SDLK_6) {
                    SDL_Color tmp = textcol;
                    textcol = boardcol;
                    boardcol = tmp;
                    textcol.a = 0;
                    boardcol.a = 0;
                    SDL_Log("Invert BG / FG color");
                    done = 1;
                }
                if (event.key.key == SDLK_3) {
                    done = 1;
                    textcol.r += 1;
                    SDL_Log("color fg: r=%d g=%d b=%d alpha=%d", textcol.r, textcol.g, textcol.b, textcol.a);
                }
                if (event.key.key == SDLK_4) {
                    done = 1;
                    textcol.g += 1;
                    SDL_Log("color fg: r=%d g=%d b=%d alpha=%d", textcol.r, textcol.g, textcol.b, textcol.a);
                }
                if (event.key.key == SDLK_5) {
                    done = 1;
                    textcol.b += 1;
                    SDL_Log("color fg: r=%d g=%d b=%d alpha=%d", textcol.r, textcol.g, textcol.b, textcol.a);
                }
                if (event.key.key == SDLK_2) {
                    const char *str[6] = {"Black", "Red", "Green", "Blue", "White", "Gray" };
                    background_color += 1;
                    if (background_color == 6) background_color = 0;
                    SDL_Log("Background color '%s", str[background_color]);
                    done = 1;
                }

                if (event.key.key == SDLK_ESCAPE) {
                    SDL_Log("ESC");
                    return 1;
                }
                break;
            case SDL_EVENT_QUIT:
                return 1;
            default:
                break;
        }
    }
    return 0;
}

int main(void)
{
    SDL_Window   *window         = NULL;
    SDL_Texture  *text_texture   = NULL;
    SDL_Surface  *text_surface   = NULL;
    SDL_Renderer *renderer       = NULL;
    const int     windoww        = 640;
    const int     windowh        = 480;
    const char   *text;
    char          filename[256];
    char          infos[256];
    int           replay = 0;
    int           t1 = 0, t2 = 0, t_sum;
    Uint64        T1 = 0, T2 = 0, T_sum, T_min;
    int           count, count_init;

#define INIT_VARS           \
    t_sum = 0;              \
    T_sum = 0;              \
    T_min = 999999999999ULL;\
    count = count_init;     \

#define START_MEASURE                                                                   \
    t1 = SDL_GetTicks();                                                                \
    T1 = SDL_GetPerformanceCounter();                                                   \


#define END_MEASURE                                                                     \
    T2 = SDL_GetPerformanceCounter();                                                   \
    t2 = SDL_GetTicks();                                                                \
    \
    t_sum += (t2 - t1);                                                                 \
    T_sum += (T2 - T1);                                                                 \
    T_min  = SDL_min(T_min, T2 - T1);                                                   \


    if (!SDL_Init(SDL_INIT_VIDEO)) {
       quit("SDL init failed");
    }

    if (!TTF_Init()) {
       SDL_Quit();
       quit("SDL_ttf init failed");
    }

    window = SDL_CreateWindow("", windoww, windowh, 0);
    if (window == NULL) {
       quit("SDL windowdow setup failed");
    }

    // SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    // SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
    renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL) {
       quit("SDL renderer setup failed");
    }

    /* Display help */
    help();


#if 0
    /*****************************************/
    /* Copy a log here to replay a rendering */
    /*********************************** *****/

    /* FreeType issue with FletcherGothicFLF, size=10 and outline at 9 -> error in FT_Glyph_To_Bitmap(). glyph '=' */
    // replay=1; font_style=0; kerning=0; wrap=0; wrap_size=0; outline=8; curr_size=10; render_mode=0; curr_str=5; curr_font=0;
    // do outline +1 (key p)
    // --> SDL_GetError: Couldn't find glyph
    //

    /* FreeType issue with font/ITCAvantGardeStd-Demi.ttf glyph='Q' */
    // replay=1; font_style=0; kerning=0; wrap=0; wrap_size=0; outline=0; curr_size=4; render_mode=0; curr_str=27; curr_font=1; hinting=3;
    // do outline +1 (key p)
    // --> SDL_GetError: Couldn't find glyph

#endif

    // color
    // replay=1; font_style=0; kerning=0; wrap=0; wrap_size=354; outline=0; curr_size=7; render_mode=1; curr_str=63; curr_font=1522; hinting=0; fg_alpha=255; // light Blended

    //replay=1; font_style=15; kerning=0; wrap=1; wrap_size=56; outline=0; curr_size=5; render_mode=1; curr_str=68; curr_font=3007; hinting=0; fg_alpha=128; // mono Blended

#if 0
    seed=1641805930; replay=1; font_style=9; kerning=1; sdf=1; wrap=0; wrap_size=661; w_align=2; outline=0; curr_size=20; render_mode=3; curr_str=14; curr_font=1777; hinting=1; fg_alpha=65; // light LCD
#endif

    //seed=1673390190; replay=1; font_style=12; kerning=0; sdf=1; wrap=0; wrap_size=91; w_align=0; outline=0; curr_size=16; render_mode=0; curr_str=14; curr_font=1288; hinting=0; fg_alpha=77; // none Solid

    //seed=1673390190; replay=1; font_style=12; kerning=0; sdf=0; wrap=0; wrap_size=91; w_align=0; outline=0; curr_size=30; render_mode=1; curr_str=14; curr_font=1288; hinting=0; fg_alpha=77; // none Solid

    //seed=1673390190; replay=1; font_style=9; kerning=1; sdf=0; wrap=1; wrap_size=94; w_align=2; outline=7; curr_size=42; render_mode=1; curr_str=75; curr_font=1997; hinting=1; fg_alpha=90; // light Blended

    if (replay) {
       SDL_Log("Replay with string _%s_", test_strings[curr_str]);
    }

    while (1)
    {
       /* Normal mode <space>, try all combination {strings}x{render_mode} and exit
          This updates "render_mode" and "curr_str" */
       if (!mode_random_test && !replay) {
          if (iter <= 0) {
             iter = 0;
          }

          if (iter >= render_mode_count * test_strings_count) {
             SDL_Log("End of rendering!");
             break;
          } else {
             render_mode = iter / test_strings_count;
             curr_str = iter - test_strings_count * render_mode;
          }
       }

       if (render_mode_overwrite) {
          render_mode = render_mode_overwrite;
       }


       textcol.a = fg_alpha;
       boardcol.a = bg_alpha;

       /* Set all parameters coming from random or user input */
#if 0
       /* Force a parameter */
       curr_font = 1250;
       curr_size = 1;
       curr_size &=0x3;
#endif

#if 0
       /* From a specific seed, jump to a specific iteration */
       {
          static int skip = 1046;
          if (skip) {
             skip--;
             goto next_loop;
          }
       }
#endif

       if (saved_curr_font != curr_font) {
          if (font) {
             TTF_CloseFont(font);
          }
          saved_curr_font = curr_font;
          saved_curr_size = curr_size;
          font_path = test_fonts[curr_font];
          SDL_Log("TTF_OpenFont: %s", font_path);
          font = TTF_OpenFont(font_path, curr_size);
          if (!font) {
#if 1
             quit("Font load failed");
#else
             SDL_Log("Font load failed");
             saved_curr_font = -1;
             goto next_loop;
#endif
          }
       }

       if (saved_curr_size != curr_size) {
#if defined(HAVE_SET_FONT_SIZE_FUNCTION)
          TTF_SetFontSize(font, curr_size);
#else
          if (font) {
             TTF_CloseFont(font);
          }
          font_path = test_fonts[curr_font];
          SDL_Log("Re-TTF_OpenFont: %s to change size from %d to %d", font_path, saved_curr_size, curr_size);
          font = TTF_OpenFont(font_path, curr_size);
          if (!font) {
#if 1
             quit("Font load failed (2)");
#else
             SDL_Log("Font load failed (2)");
             saved_curr_font = -1;
             goto next_loop;
#endif

          }
#endif
          saved_curr_size = curr_size;
       }

       TTF_SetFontDirection(font, directions[direction].value);

       {
          int tmp;
          tmp = TTF_GetFontOutline(font);
          if (tmp != outline) {
             TTF_SetFontOutline(font, outline);
          }
          tmp = TTF_GetFontStyle(font);
          if (tmp != font_style) {
             TTF_SetFontStyle(font, font_style);
          }
          tmp = TTF_GetFontKerning(font);
          if (tmp != kerning) {
             TTF_SetFontKerning(font, kerning);
          }
#if defined(HAVE_SDF)
          tmp = TTF_GetFontSDF(font);
          if (tmp != sdf) {
             TTF_SetFontSDF(font, sdf);
          }
#endif

          {
             // static const char *hinting_desc[] = { "normal", "light", "light_subpix", "mono", "none" };
             // static const char *hinting_desc[] = { "normal", "light", "light_subpix", "lcd_subpix", "mono", "none" };
             int h = 0;
             if (hinting == 0) h = TTF_HINTING_NORMAL;
             if (hinting == 1) h = TTF_HINTING_LIGHT;
             if (hinting == 2) h = 4; // TTF_HINTING_LIGHT_SUBPIXEL;
                                      //    if (hinting == 3) h = 5; // TTF_HINTING_LCD_SUBPIXEL;
             if (hinting == 3) h = TTF_HINTING_MONO;
             if (hinting == 4) h = TTF_HINTING_NONE;
             tmp = TTF_GetFontHinting(font);
             if (tmp != h) {
                TTF_SetFontHinting(font, h);
             }
          }
       }

       /* Get some console output out in case we crash next... */
       if (! mode_random_test) {
          char title[1024];
          SDL_snprintf(title, sizeof(title) - 1, "%s Sz=%d outline=%d Hinting=%s %s",
                render_mode_desc[render_mode],
                curr_size,
                outline,
                hinting_desc[hinting],
                testapp_basename(font_path));

          if (save_to_bmp) {
             SDL_snprintf(filename, sizeof(filename) - 1 ,
                   "Render=%s_size=%d_outline=%d_wrap=%d_wrap_size=%d_kerning=%d_sdf=%d_italic=%d_bold=%d_underline=%d_strikethrough=%d_hinting=%s_%s__%" SDL_PRIs64 ".bmp",
                   render_mode_desc[render_mode],
                   curr_size,
                   outline,
                   wrap,
                   wrap_size,
                   kerning,
                   sdf,
                   !!(font_style & TTF_STYLE_ITALIC),
                   !!(font_style & TTF_STYLE_BOLD),
                   !!(font_style & TTF_STYLE_UNDERLINE),
                   !!(font_style & TTF_STYLE_STRIKETHROUGH),
                   hinting_desc[hinting],
                   testapp_basename(font_path),
                   (Sint64)time(NULL)
                   );
          }

          SDL_snprintf(infos, sizeof(infos) - 1 ,
                "Render=%s size=%d outline=%d wrap=%d wrap_size=%d kerning=%d sdf=%d italic=%d bold=%d underline=%d strikethrough=%d hinting=%s %s",
                render_mode_desc[render_mode],
                curr_size,
                outline,
                wrap,
                wrap_size,
                kerning,
                sdf,
                !!(font_style & TTF_STYLE_ITALIC),
                !!(font_style & TTF_STYLE_BOLD),
                !!(font_style & TTF_STYLE_UNDERLINE),
                !!(font_style & TTF_STYLE_STRIKETHROUGH),
                hinting_desc[hinting],
                font_path
                );
          SDL_SetWindowTitle(window, title);
          SDL_Log("%s", infos);
#if 0
          SDL_Log("font_style=%d; kerning=%d; wrap=%d; wrap_size=%d; outline=%d; curr_size=%d; render_mode=%d; curr_str=%d; curr_font=%d; hinting=%d;",
                font_style, kerning, wrap, wrap_size, outline, curr_size, render_mode, curr_str, curr_font, hinting);
#endif
       }

       /* pick a string */
       text = test_strings[curr_str];

       /* Skip some string with no chars (all index == 0), zero advance and so zero width at low size
        * That would fail to render because of size 0.
        * */
       {
          int w = 0, h = 0;
#if defined(HAVE_SDF)
          /* SDF change the str size. if we dont render with it (eg not blended mode).
           * then set it to 0.
           * otherwse it reports a valid size.
           * but the texture isn't renderered.
           */
          if (render_mode != 1) {
             int tmp = TTF_GetFontSDF(font);
             if (tmp != 0) {

                TTF_SetFontSDF(font, 0);
             }
          }
#endif
          if (!TTF_SizeUTF8(font, text, &w, &h)) {
             SDL_Log("size failed");
          }
          if (w == 0) {
             SDL_Log("skip size == 0");
             goto next_loop;
          }
       }

       count_init = print_elapsed_ticks ? 500 : 1;
       INIT_VARS

          /* render */
          if (wrap == 0) {
             while (count--) {
                if (text_surface) {
                   SDL_DestroySurface(text_surface);
                }

                switch (render_mode)
                {
                   case 1:
                      text_surface = TTF_RenderUTF8_Blended(font, text, textcol);
                      break;
                   case 2:
                      text_surface = TTF_RenderUTF8_Shaded(font, text, textcol, boardcol);
                      break;
                   case 3:
#if defined(HAVE_LCD)
                      text_surface = TTF_RenderUTF8_LCD(font, text, textcol, boardcol);
#else
                      text_surface = TTF_RenderUTF8_Shaded(font, text, textcol, boardcol);
#endif
                      break;
                   case 0:
                      text_surface = TTF_RenderUTF8_Solid(font, text, textcol);
                      break;
                }

                if (text_surface) {
                   SDL_DestroySurface(text_surface);
                }

                if (print_elapsed_ticks) {
                   START_MEASURE
                }

                switch (render_mode)
                {
                   case 1:
                      text_surface = TTF_RenderUTF8_Blended(font, text, textcol);
                      break;
                   case 2:
                      text_surface = TTF_RenderUTF8_Shaded(font, text, textcol, boardcol);
                      break;
                   case 3:
#if defined(HAVE_LCD)
                      text_surface = TTF_RenderUTF8_LCD(font, text, textcol, boardcol);
#else
                      text_surface = TTF_RenderUTF8_Shaded(font, text, textcol, boardcol);
#endif
                      break;
                   case 0:
                      text_surface = TTF_RenderUTF8_Solid(font, text, textcol);
                      break;
                }

                if (print_elapsed_ticks) {
                   END_MEASURE
                }
             }
          } else {

#if defined(HAVE_WRAP_ALIGN)
             TTF_SetFontWrappedAlign(font, w_align);
#endif


             while (count--) {
                if (text_surface) {
                   SDL_DestroySurface(text_surface);
                }

                if (print_elapsed_ticks) {
                   START_MEASURE
                }
#if defined(HAVE_ALL_WRAPPED_FUNCTIONS)
                switch (render_mode)
                {
                   case 1:
                      text_surface = TTF_RenderUTF8_Blended_Wrapped(font, text, textcol, wrap_size);
                      break;
                   case 2:
                      text_surface = TTF_RenderUTF8_Shaded_Wrapped(font, text, textcol, boardcol, wrap_size);
                      break;
                   case 3:
#if defined(HAVE_LCD)
                      text_surface = TTF_RenderUTF8_LCD_Wrapped(font, text, textcol, boardcol, wrap_size);
#else
                      text_surface = TTF_RenderUTF8_Shaded_Wrapped(font, text, textcol, boardcol, wrap_size);
#endif
                      break;
                   case 0:
                      text_surface = TTF_RenderUTF8_Solid_Wrapped(font, text, textcol, wrap_size);
                      break;
                }
#else
                text_surface = TTF_RenderUTF8_Blended_Wrapped(font, text, textcol, wrap_size);
#endif
                if (print_elapsed_ticks) {
                   END_MEASURE
                }
             }
          }

       if (print_elapsed_ticks) {
          SDL_Log("Avg: %7lg ms  Avg Perf: %7" SDL_PRIu64 " (min=%7" SDL_PRIu64 ")", (double)t_sum / (double)count_init, T_sum / count_init, T_min);
       }

       if (text_surface == NULL && render_mode == 1 && sdf == 1) {
          // sometimes SDF has glyph not found ?? FT issue ?
          SDL_Log("BLENDED/SDF not rendered--> %s", SDL_GetError());
          goto next_loop;
       } else if (text_surface == NULL && render_mode == 3) {
          SDL_Log("LCD not rendered--> %s", SDL_GetError());
          goto next_loop;
       } else if (text_surface == NULL) {
          goto finish;
       }

       /* update_screen */
       if (save_to_bmp) {
          save_to_bmp = 0;
          SDL_Log("Saving to %s", filename);
          SDL_SaveBMP(text_surface, filename);
       }

       if (update_screen_mode == 1) {
          /* No display */
       } else if (update_screen_mode == 0) {
          /* Normal texture */

          if (background_color == 0) SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
          if (background_color == 1) SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
          if (background_color == 2) SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
          if (background_color == 3) SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
          if (background_color == 4) SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
          if (background_color == 5) SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);

          SDL_RenderClear(renderer);

          text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
          if (text_texture == NULL) {
             SDL_Log("Cannot create texture from surface(w=%d h=%d): %s", text_surface->w, text_surface->h, SDL_GetError());
          } else {
             SDL_Rect dstrect;
             dstrect.x = windoww/2 - text_surface->w/2;
             dstrect.y = windowh/2 - text_surface->h/2;
             dstrect.w = text_surface->w;
             dstrect.h = text_surface->h;
             {
                SDL_FRect d;
                d.x = (float)dstrect.x;
                d.y = (float)dstrect.y;
                d.w = (float)dstrect.w;
                d.h = (float)dstrect.h;
                SDL_RenderTexture(renderer, text_texture, NULL, &d);
             }
             SDL_RenderPresent(renderer);

             SDL_DestroyTexture(text_texture);
          }
       }
       SDL_DestroySurface(text_surface);
       text_surface = NULL;

next_loop:

       if (mode_random_test == 0) {
          if (wait_for_input() > 0) {
             break;
          }
       } else {
          random_input();
       }
    }

finish:

    SDL_Log("SDL_GetError: %s", SDL_GetError());
    SDL_Log("cleanup");
    if (font) {
       TTF_CloseFont(font);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Log("SDL_GetError: %s", SDL_GetError());
    TTF_Quit();
    SDL_Quit();
    SDL_Log("SDL_GetError: %s", SDL_GetError());

    return 0;
}

/* random in [0; n - 1] */
static int rand_n(int n) {
    return (rand() % n);
}

static void init_rand(void)
{
    static int once = 0;
    if (once == 0) {
       once = 1;
       /* Let's display the seed if we need to play the test again */
       if (seed == 0) {
          seed = time(NULL);
          SDL_Log("seed %d", seed);
       } else {
          SDL_Log("re-use seed %d", seed);
       }
       srand(seed);
    }
}

static void random_input(void)
{
    SDL_Event event;
    int r;
    int r0, r1, r2, r3, r4, r5, r6, r7, r8, r9;
    int r10, r11, r12, r13, r14, r15, r16, r17, r18, r19;
    int r20;

    init_rand();

    while (SDL_PollEvent(&event) == 1) {
       if (event.type == SDL_EVENT_KEY_DOWN) {
          mode_random_test = 0;
          SDL_Log("stop random test");
          return;
       }
    }

    random_cnt++;

    font_style = 0;

    r = rand();
    r0 = !!(r & (1<<0));
    r1 = !!(r & (1<<1));
    r2 = !!(r & (1<<2));
    r3 = !!(r & (1<<3));
    r4 = !!(r & (1<<4));
    r5 = !!(r & (1<<5));
    r6 = !!(r & (1<<6));
    r7 = !!(r & (1<<7));
    r8 = !!(r & (1<<8));
    r9 = !!(r & (1<<9));
    r10 = !!(r & (1<<10));
    r11 = !!(r & (1<<11));
    r12 = !!(r & (1<<12));
    r13 = !!(r & (1<<13));
    r14 = !!(r & (1<<14));
    r15 = !!(r & (1<<15));
    r16 = !!(r & (1<<16));
    r17 = !!(r & (1<<17));
    r18 = !!(r & (1<<18));
    r19 = !!(r & (1<<19));
    r20 = !!(r & (1<<20));

    if (r0) font_style |= TTF_STYLE_UNDERLINE;
    if (r1) font_style |= TTF_STYLE_STRIKETHROUGH;
    if (r2) font_style |= TTF_STYLE_BOLD;
    if (r3) font_style |= TTF_STYLE_ITALIC;

    kerning = r4;

    wrap = r5;

    /* wrap size: change less often, arbitrary distribution */
    if (wrap) {
       if (r6) {
          wrap_size = rand_n(200);
       } else if (r7) {
          wrap_size = rand_n(400);
       } else if (r8) {
          wrap_size = rand_n(800);
       } else {
          wrap_size = rand_n(2000);
       }

       w_align = rand_n(3);
    }

    /* Current Outline: change less often, arbitrary distribution */
    if (r9) {
       outline = 0;
    } else {
       if (r10) {
          outline = rand_n(10);
       } else {
          outline = rand_n(150);
       }
    }

    /* Current Size: change less often, arbitrary distribution */
    if (r11 && r12 && r13) {
       if (r14) {
          curr_size = rand_n(14);
       } else {
          if (r15) {
             curr_size = rand_n(30);
          } else if (r16) {
             curr_size = rand_n(60);
          } else {
             curr_size = rand_n(200);
          }
       }
    }

    /* Current Font: change less often */
    if (r17 && r18 && r19) {
       curr_font = rand_n(test_fonts_count);
    }

    sdf = r20;
    /* using SDF is too slow  */
    if (g_force_no_SDF) {
       sdf = 0;
    } else if (sdf) {
       /* also slow ? */
       if (outline >= 4) {
          outline = 4;
       }
    }

    render_mode = rand_n(render_mode_count);

    hinting = rand_n(hinting_count);

    curr_str = rand_n(test_strings_count);

    fg_alpha = rand_n(256);

    SDL_Log("%6d] seed=%d; replay=1; font_style=%d; kerning=%d; sdf=%d; wrap=%d; wrap_size=%d; w_align=%d; outline=%d; curr_size=%d; render_mode=%d; curr_str=%d; curr_font=%d; hinting=%d; fg_alpha=%d; // %s %s",
          random_cnt, seed,
          font_style, kerning, sdf,
          wrap, wrap_size, w_align,
          outline, curr_size, render_mode, curr_str, curr_font, hinting,
          fg_alpha,
          hinting_desc[hinting],
          render_mode_desc[render_mode]
          );

    return;
}
