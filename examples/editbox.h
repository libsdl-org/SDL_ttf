/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* This is an example of using SDL_ttf to create a multi-line editbox
 * with full IME support.
 */

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

/* Define this if you want to test the surface text engine */
#define TEST_SURFACE_ENGINE

typedef struct EditBox {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    TTF_Text *text;
    SDL_FRect rect;
    bool has_focus;

    /* Cursor support */
    int cursor;
    int cursor_length;
    bool cursor_visible;
    Uint64 last_cursor_change;
    SDL_FRect cursor_rect;

    /* Highlight support */
    bool highlighting;
    int highlight1;
    int highlight2;

    /* IME composition */
    int composition_start;
    int composition_length;
    int composition_cursor;
    int composition_cursor_length;

    /* IME candidates */
    TTF_Text *candidates;
    int selected_candidate_start;
    int selected_candidate_length;

#ifdef TEST_SURFACE_ENGINE
    SDL_Surface *window_surface;
#endif
} EditBox;


extern EditBox *EditBox_Create(SDL_Window *window, SDL_Renderer *renderer, TTF_TextEngine *engine, TTF_Font *font, const SDL_FRect *rect);
extern void EditBox_Destroy(EditBox *edit);
extern void EditBox_SetFocus(EditBox *edit, bool focus);
extern void EditBox_Draw(EditBox *edit);
extern void EditBox_MoveCursorLeft(EditBox *edit);
extern void EditBox_MoveCursorRight(EditBox *edit);
extern void EditBox_MoveCursorUp(EditBox *edit);
extern void EditBox_MoveCursorDown(EditBox *edit);
extern void EditBox_MoveCursorBeginningOfLine(EditBox *edit);
extern void EditBox_MoveCursorEndOfLine(EditBox *edit);
extern void EditBox_MoveCursorBeginning(EditBox *edit);
extern void EditBox_MoveCursorEnd(EditBox *edit);
extern void EditBox_Backspace(EditBox *edit);
extern void EditBox_BackspaceToBeginning(EditBox *edit);
extern void EditBox_DeleteToEnd(EditBox *edit);
extern void EditBox_Delete(EditBox *edit);
extern void EditBox_SelectAll(EditBox *edit);
extern bool EditBox_DeleteHighlight(EditBox *edit);
extern void EditBox_Copy(EditBox *edit);
extern void EditBox_Cut(EditBox *edit);
extern void EditBox_Paste(EditBox *edit);
extern void EditBox_Insert(EditBox *edit, const char *text);
extern bool EditBox_HandleEvent(EditBox *edit, SDL_Event *event);

