/*
  Copyright 1997-2022 Sam Lantinga
  Copyright 2022 Collabora Ltd.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include "SDL_ttf.h"
#include "SDL_test.h"

#if TTF_USE_HARFBUZZ
#include <hb.h>
#endif

static SDLTest_CommonState *state;
static const char *fontPath = NULL;

static int
TestDirection(void *arg)
{
    static const char hello[] = "Hello, world!";
    TTF_Font *font = NULL;
    const char *error;
    int ltr_w, ltr_h;
#if TTF_USE_HARFBUZZ
    TTF_Font *font2 = NULL;
    int ttb_w, ttb_h;
    int w, h;
#endif

    error = (TTF_Init() == 0) ? NULL : TTF_GetError();
    if (!SDLTest_AssertCheck(error == NULL,
                             "Init: %s", error ? error : "successful")) {
        goto out;
    }

    font = TTF_OpenFont(fontPath, 12);
    error = (font != NULL) ? "successful" : TTF_GetError();
    if (!SDLTest_AssertCheck(font != NULL,
                             "Load font %s: %s", fontPath, error)) {
        goto out;
    }

    TTF_SizeUTF8(font, hello, &ltr_w, &ltr_h);
    SDLTest_AssertCheck(ltr_w > ltr_h,
                        "Default text direction is horizontal: %d > %d",
                        ltr_w, ltr_h);

#if TTF_USE_HARFBUZZ
    SDLTest_AssertCheck(TTF_SetDirection(HB_DIRECTION_TTB) == 0,
                        "Set global direction");
    TTF_SizeUTF8(font, hello, &ttb_w, &ttb_h);
    SDLTest_AssertCheck(ttb_w < ttb_h,
                        "Changing global direction works: %d < %d",
                        ttb_w, ttb_h);
    SDLTest_AssertCheck(TTF_SetDirection(HB_DIRECTION_LTR) == 0,
                        "Set global direction");
    TTF_SizeUTF8(font, hello, &w, &h);
    SDLTest_AssertCheck(w == ltr_w && h == ltr_h,
                        "Changing global direction back works: %dx%d",
                        w, h);

    SDLTest_AssertCheck(TTF_SetFontDirection(font, TTF_DIRECTION_TTB) == 0,
                        "Set font direction");
    TTF_SizeUTF8(font, hello, &w, &h);
    SDLTest_AssertCheck(w == ttb_w && h == ttb_h,
                        "Can change per-font direction: %dx%d", w, h);

    SDLTest_AssertCheck(TTF_SetFontDirection(font, TTF_DIRECTION_LTR) == 0,
                        "Set font direction");
    SDLTest_AssertCheck(TTF_SetDirection(HB_DIRECTION_TTB) == 0,
                        "Set global direction");
    TTF_SizeUTF8(font, hello, &w, &h);
    SDLTest_AssertCheck(w == ltr_w && h == ltr_h,
                        "Changing font direction works: %dx%d", w, h);

    font2 = TTF_OpenFont(fontPath, 12);
    error = (font2 != NULL) ? "successful" : TTF_GetError();
    if (!SDLTest_AssertCheck(font2 != NULL,
                             "Load font %s: %s", fontPath, error)) {
        goto out;
    }

    TTF_SizeUTF8(font2, hello, &w, &h);
    SDLTest_AssertCheck(w == ttb_w && h == ttb_h,
                        "Global direction is used for new font: %dx%d",
                        w, h);
#endif

out:
    if (font != NULL) {
        TTF_CloseFont(font);
    }
    if (TTF_WasInit()) {
        TTF_Quit();
    }
    return TEST_COMPLETED;
}

static const SDLTest_TestCaseReference directionTestCase = {
    TestDirection, "Direction", "Render text directionally", TEST_ENABLED
};

static const SDLTest_TestCaseReference *testCases[] =  {
    &directionTestCase,
    NULL
};
static SDLTest_TestSuiteReference testSuite = {
    "ttf",
    NULL,
    testCases,
    NULL
};
static SDLTest_TestSuiteReference *testSuites[] =  {
    &testSuite,
    NULL
};

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDLTest_CommonQuit(state);
    exit(rc);
}

int
main(int argc, char *argv[])
{
    int result;
    int testIterations = 1;
    Uint64 userExecKey = 0;
    char *userRunSeed = NULL;
    char *filter = NULL;
    int i, done;
    SDL_Event event;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            consumed = -1;
            if (SDL_strcasecmp(argv[i], "--iterations") == 0) {
                if (argv[i + 1]) {
                    testIterations = SDL_atoi(argv[i + 1]);
                    if (testIterations < 1) testIterations = 1;
                    consumed = 2;
                }
            }
            else if (SDL_strcasecmp(argv[i], "--execKey") == 0) {
                if (argv[i + 1]) {
                    SDL_sscanf(argv[i + 1], "%"SDL_PRIu64, &userExecKey);
                    consumed = 2;
                }
            }
            else if (SDL_strcasecmp(argv[i], "--seed") == 0) {
                if (argv[i + 1]) {
                    userRunSeed = SDL_strdup(argv[i + 1]);
                    consumed = 2;
                }
            }
            else if (SDL_strcasecmp(argv[i], "--filter") == 0) {
                if (argv[i + 1]) {
                    filter = SDL_strdup(argv[i + 1]);
                    consumed = 2;
                }
            }
            else if (argv[i][0] != '-') {
                if (fontPath == NULL) {
                    fontPath = argv[i];
                    consumed = 1;
                }
            }
        }
        if (consumed < 0) {
            static const char *options[] = { "[--iterations #]", "[--execKey #]", "[--seed string]", "[--filter suite_name|test_name] font_path", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            quit(1);
        }

        i += consumed;
    }

    if (fontPath == NULL) {
        SDLTest_LogError("A font is required");
        quit(1);
    }

    /* Initialize common state */
    if (!SDLTest_CommonInit(state)) {
        quit(2);
    }

    /* Create the windows, initialize the renderers */
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderClear(renderer);
    }

    /* Call Harness */
    result = SDLTest_RunSuites(testSuites, (const char *)userRunSeed, userExecKey, (const char *)filter, testIterations);

    /* Empty event queue */
    done = 0;
    for (i=0; i<100; i++)  {
      while (SDL_PollEvent(&event)) {
        SDLTest_CommonEvent(state, &event, &done);
      }
      SDL_Delay(10);
    }

    /* Clean up */
    SDL_free(userRunSeed);
    SDL_free(filter);

    /* Shutdown everything */
    quit(result);
    return(result);
}

/* vi: set ts=4 sw=4 expandtab: */
