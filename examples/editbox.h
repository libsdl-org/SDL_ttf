/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>


typedef struct EditBox {
    TTF_Font *font;
    TTF_Text *text;
    SDL_FRect rect;
    int cursor;
    bool cursor_visible;
    Uint64 last_cursor_change;
    bool highlighting;
    int highlight1, highlight2;

    // Used for testing the software rendering implementation
    SDL_Surface *window_surface;
} EditBox;


extern EditBox *EditBox_Create(TTF_Text *text, const SDL_FRect *rect);
extern void EditBox_Destroy(EditBox *edit);
extern void EditBox_Draw(EditBox *edit, SDL_Renderer *renderer);
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

