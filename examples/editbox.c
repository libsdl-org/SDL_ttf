/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include "editbox.h"

#define CURSOR_BLINK_INTERVAL_MS    500


EditBox *EditBox_Create(TTF_Text *text, const SDL_FRect *rect)
{
    EditBox *edit = (EditBox *)SDL_calloc(1, sizeof(*edit));
    if (!edit) {
        return NULL;
    }

    edit->text = text;
    edit->font = TTF_GetTextFont(text);
    edit->rect = *rect;
    edit->highlight1 = -1;
    edit->highlight2 = -1;

    return edit;
}

void EditBox_Destroy(EditBox *edit)
{
    if (!edit) {
        return;
    }

    SDL_free(edit);
}

static bool GetHighlightExtents(EditBox *edit, int *marker1, int *marker2)
{
    if (edit->highlight1 >= 0 && edit->highlight2 >= 0) {
        *marker1 = SDL_min(edit->highlight1, edit->highlight2);
        *marker2 = SDL_max(edit->highlight1, edit->highlight2) - 1;
        if (*marker2 >= *marker1) {
            return true;
        }
    }
    return false;
}

void EditBox_Draw(EditBox *edit, SDL_Renderer *renderer)
{
    if (!edit) {
        return;
    }

    float x = edit->rect.x;
    float y = edit->rect.y;

    /* Draw any highlight */
    int marker1, marker2;
    if (GetHighlightExtents(edit, &marker1, &marker2)) {
        TTF_SubString **highlights = TTF_GetTextSubStringsForRange(edit->text, marker1, marker2, NULL);
        if (highlights) {
            int i;
            SDL_SetRenderDrawColor(renderer, 0xCC, 0xCC, 0x00, 0xFF);
            for (i = 0; highlights[i]; ++i) {
                SDL_FRect rect;
                SDL_RectToFRect(&highlights[i]->rect, &rect);
                rect.x += x;
                rect.y += y;
                SDL_RenderFillRect(renderer, &rect);
            }
            SDL_free(highlights);
        }
    }

    if (edit->window_surface) {
        /* Flush the renderer so we can draw directly to the window surface */
        SDL_FlushRenderer(renderer);
        TTF_DrawSurfaceText(edit->text, (int)x, (int)y, edit->window_surface);
    } else {
        TTF_DrawRendererText(edit->text, x, y);
    }

    /* Draw the cursor */
    Uint64 now = SDL_GetTicks();
    if ((now - edit->last_cursor_change) >= CURSOR_BLINK_INTERVAL_MS) {
        edit->cursor_visible = !edit->cursor_visible;
        edit->last_cursor_change = now;
    }

    TTF_SubString cursor;
    if (edit->cursor_visible && TTF_GetTextSubString(edit->text, edit->cursor, &cursor)) {
        SDL_FRect cursorRect;

        SDL_RectToFRect(&cursor.rect, &cursorRect);
        if (TTF_GetFontDirection(edit->font) == TTF_DIRECTION_RTL) {
            cursorRect.x += cursor.rect.w;
        }
        cursorRect.x += x;
        cursorRect.y += y;
        cursorRect.w = 1.0f;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
        SDL_RenderFillRect(renderer, &cursorRect);
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

static void MoveCursorIndex(EditBox *edit, int direction)
{
    TTF_SubString substring;

    if (direction < 0) {
        if (TTF_GetTextSubString(edit->text, edit->cursor - 1, &substring)) {
            edit->cursor = substring.offset;
        }
    } else {
        if (TTF_GetTextSubString(edit->text, edit->cursor, &substring) &&
            TTF_GetTextSubString(edit->text, substring.offset + SDL_max(substring.length, 1), &substring)) {
            edit->cursor = substring.offset;
        }
    }
}

void EditBox_MoveCursorLeft(EditBox *edit)
{
    if (!edit) {
        return;
    }

    if (TTF_GetFontDirection(edit->font) == TTF_DIRECTION_RTL) {
        MoveCursorIndex(edit, 1);
    } else {
        MoveCursorIndex(edit, -1);
    }
}

void EditBox_MoveCursorRight(EditBox *edit)
{
    if (!edit) {
        return;
    }

    if (TTF_GetFontDirection(edit->font) == TTF_DIRECTION_RTL) {
        MoveCursorIndex(edit, -1);
    } else {
        MoveCursorIndex(edit, 1);
    }
}

void EditBox_MoveCursorUp(EditBox *edit)
{
    if (!edit) {
        return;
    }

    TTF_SubString substring;
    if (TTF_GetTextSubString(edit->text, edit->cursor, &substring)) {
        int fontHeight = TTF_GetFontHeight(edit->font);
        int x, y;
        if (TTF_GetFontDirection(edit->font) == TTF_DIRECTION_RTL) {
            x = substring.rect.x + substring.rect.w;
        } else {
            x = substring.rect.x;
        }
        y = substring.rect.y - fontHeight;
        if (TTF_GetTextSubStringForPoint(edit->text, x, y, &substring)) {
            edit->cursor = GetCursorTextIndex(edit->font, x, &substring);
        }
    }
}

void EditBox_MoveCursorDown(EditBox *edit)
{
    if (!edit) {
        return;
    }

    TTF_SubString substring;
    if (TTF_GetTextSubString(edit->text, edit->cursor, &substring)) {
        int fontHeight = TTF_GetFontHeight(edit->font);
        int x, y;
        if (TTF_GetFontDirection(edit->font) == TTF_DIRECTION_RTL) {
            x = substring.rect.x + substring.rect.w;
        } else {
            x = substring.rect.x;
        }
        y = substring.rect.y + substring.rect.h + fontHeight;
        if (TTF_GetTextSubStringForPoint(edit->text, x, y, &substring)) {
            edit->cursor = GetCursorTextIndex(edit->font, x, &substring);
        }
    }
}

void EditBox_MoveCursorBeginningOfLine(EditBox *edit)
{
    if (!edit) {
        return;
    }

    TTF_SubString substring;
    if (TTF_GetTextSubString(edit->text, edit->cursor, &substring) &&
        TTF_GetTextSubStringForLine(edit->text, substring.line_index, &substring)) {
        edit->cursor = substring.offset;
    }
}

void EditBox_MoveCursorEndOfLine(EditBox *edit)
{
    if (!edit) {
        return;
    }

    TTF_SubString substring;
    if (TTF_GetTextSubString(edit->text, edit->cursor, &substring) &&
        TTF_GetTextSubStringForLine(edit->text, substring.line_index, &substring)) {
        edit->cursor = substring.offset + substring.length;
    }
}

void EditBox_MoveCursorBeginning(EditBox *edit)
{
    if (!edit) {
        return;
    }

    /* Move to the beginning of the text */
    edit->cursor = 0;
}

void EditBox_MoveCursorEnd(EditBox *edit)
{
    if (!edit) {
        return;
    }

    /* Move to the end of the text */
    if (edit->text->text) {
        edit->cursor = (int)SDL_strlen(edit->text->text);
    }
}

void EditBox_Backspace(EditBox *edit)
{
    if (!edit || !edit->text->text) {
        return;
    }

    if (EditBox_DeleteHighlight(edit)) {
        return;
    }

    const char *start = &edit->text->text[edit->cursor];
    const char *current = start;
    /* Step back over the previous UTF-8 character */
    do {
        if (current == edit->text->text) {
            break;
        }
        --current;
    } while ((*current & 0xC0) == 0x80);

    int length = (int)(start - current);
    TTF_DeleteTextString(edit->text, edit->cursor - length, length);
    edit->cursor -= length;
}

void EditBox_BackspaceToBeginning(EditBox *edit)
{
    if (!edit) {
        return;
    }

    /* Delete to the beginning of the string */
    TTF_DeleteTextString(edit->text, 0, edit->cursor);
    edit->cursor = 0;
}

void EditBox_DeleteToEnd(EditBox *edit)
{
    if (!edit) {
        return;
    }

    /* Delete to the end of the string */
    TTF_DeleteTextString(edit->text, edit->cursor, -1);
}

void EditBox_Delete(EditBox *edit)
{
    if (!edit || !edit->text->text) {
        return;
    }

    if (EditBox_DeleteHighlight(edit)) {
        return;
    }

    const char *start = &edit->text->text[edit->cursor];
    const char *next = start;
    size_t length = SDL_strlen(next);
    SDL_StepUTF8(&next, &length);
    length = (next - start);
    TTF_DeleteTextString(edit->text, edit->cursor, (int)length);
}

static bool HandleMouseDown(EditBox *edit, float x, float y)
{
    SDL_FPoint pt = { x, y };
    if (!SDL_PointInRectFloat(&pt, &edit->rect)) {
        return false;
    }

    /* Set the cursor position */
    TTF_SubString substring;
    int textX = (int)SDL_roundf(x - (edit->rect.x + 4.0f));
    int textY = (int)SDL_roundf(y - (edit->rect.y + 4.0f));
    if (!TTF_GetTextSubStringForPoint(edit->text, textX, textY, &substring)) {
        SDL_Log("Couldn't get cursor location: %s\n", SDL_GetError());
        return false;
    }

    edit->cursor = GetCursorTextIndex(edit->font, textX, &substring);
    edit->highlighting = true;
    edit->highlight1 = edit->cursor;
    edit->highlight2 = -1;

    return true;
}

static bool HandleMouseMotion(EditBox *edit, float x, float y)
{
    if (!edit->highlighting) {
        return false;
    }

    /* Set the highlight position */
    TTF_SubString substring;
    int textX = (int)SDL_roundf(x - edit->rect.x);
    int textY = (int)SDL_roundf(y - edit->rect.y);
    if (!TTF_GetTextSubStringForPoint(edit->text, textX, textY, &substring)) {
        SDL_Log("Couldn't get cursor location: %s\n", SDL_GetError());
        return false;
    }

    edit->cursor = GetCursorTextIndex(edit->font, textX, &substring);
    edit->highlight2 = edit->cursor;

    return true;
}

static bool HandleMouseUp(EditBox *edit, float x, float y)
{
    (void)x; (void)y;

    if (!edit->highlighting) {
        return false;
    }

    edit->highlighting = false;
    return true;
}

void EditBox_SelectAll(EditBox *edit)
{
    if (!edit || !edit->text->text) {
        return;
    }

    edit->highlight1 = 0;
    edit->highlight2 = (int)SDL_strlen(edit->text->text);
}

bool EditBox_DeleteHighlight(EditBox *edit)
{
    if (!edit || !edit->text->text) {
        return false;
    }

    int marker1, marker2;
    if (GetHighlightExtents(edit, &marker1, &marker2)) {
        size_t length = marker2 - marker1 + 1;
        TTF_DeleteTextString(edit->text, marker1, (int)length);
        edit->cursor = marker1;
        edit->highlight1 = -1;
        edit->highlight2 = -1;
        return true;
    }
    return false;
}

void EditBox_Copy(EditBox *edit)
{
    if (!edit || !edit->text->text) {
        return;
    }

    int marker1, marker2;
    if (GetHighlightExtents(edit, &marker1, &marker2)) {
        size_t length = marker2 - marker1 + 1;
        char *temp = (char *)SDL_malloc(length + 1);
        if (temp) {
            SDL_memcpy(temp, &edit->text->text[marker1], length);
            temp[length] = '\0';
            SDL_SetClipboardText(temp);
            SDL_free(temp);
        }
    } else {
        SDL_SetClipboardText(edit->text->text);
    }
}

void EditBox_Cut(EditBox *edit)
{
    if (!edit || !edit->text->text) {
        return;
    }

    /* Copy to clipboard and delete text */
    int marker1, marker2;
    if (GetHighlightExtents(edit, &marker1, &marker2)) {
        size_t length = marker2 - marker1 + 1;
        char *temp = (char *)SDL_malloc(length + 1);
        if (temp) {
            SDL_memcpy(temp, &edit->text->text[marker1], length);
            temp[length] = '\0';
            SDL_SetClipboardText(edit->text->text);
            SDL_free(temp);
        }
        TTF_DeleteTextString(edit->text, marker1, (int)length);
        edit->cursor = marker1;
        edit->highlight1 = -1;
        edit->highlight2 = -1;
    } else {
        SDL_SetClipboardText(edit->text->text);
        TTF_DeleteTextString(edit->text, 0, -1);
    }
}

void EditBox_Paste(EditBox *edit)
{
    if (!edit) {
        return;
    }

    const char *text = SDL_GetClipboardText();
    size_t length = SDL_strlen(text);
    TTF_InsertTextString(edit->text, edit->cursor, text, length);
    edit->cursor = (int)(edit->cursor + length);
}

void EditBox_Insert(EditBox *edit, const char *text)
{
    if (!edit || !text) {
        return;
    }

    size_t length = SDL_strlen(text);
    TTF_InsertTextString(edit->text, edit->cursor, text, length);
    edit->cursor = (int)(edit->cursor + length);
}

bool EditBox_HandleEvent(EditBox *edit, SDL_Event *event)
{
    if (!edit || !event) {
        return false;
    }

    switch (event->type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        return HandleMouseDown(edit, event->button.x, event->button.y);

    case SDL_EVENT_MOUSE_MOTION:
        return HandleMouseMotion(edit, event->motion.x, event->motion.y);

    case SDL_EVENT_MOUSE_BUTTON_UP:
        return HandleMouseUp(edit, event->button.x, event->button.y);

    case SDL_EVENT_KEY_DOWN:
        switch (event->key.key) {
        case SDLK_A:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_SelectAll(edit);
                return true;
            }
            break;
        case SDLK_C:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_Copy(edit);
                return true;
            }
            break;

        case SDLK_V:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_Paste(edit);
                return true;
            }
            break;

        case SDLK_X:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_Cut(edit);
                return true;
            }
            break;

        case SDLK_LEFT:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_MoveCursorBeginningOfLine(edit);
            } else {
                EditBox_MoveCursorLeft(edit);
            }
            return true;

        case SDLK_RIGHT:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_MoveCursorEndOfLine(edit);
            } else {
                EditBox_MoveCursorRight(edit);
            }
            return true;

        case SDLK_UP:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_MoveCursorBeginning(edit);
            } else {
                EditBox_MoveCursorUp(edit);
            }
            return true;

        case SDLK_DOWN:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_MoveCursorEnd(edit);
            } else {
                EditBox_MoveCursorDown(edit);
            }
            return true;

        case SDLK_HOME:
            EditBox_MoveCursorBeginning(edit);
            return true;

        case SDLK_END:
            EditBox_MoveCursorEnd(edit);
            return true;

        case SDLK_BACKSPACE:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_BackspaceToBeginning(edit);
            } else {
                EditBox_Backspace(edit);
            }
            return true;

        case SDLK_DELETE:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_DeleteToEnd(edit);
            } else {
                EditBox_Delete(edit);
            }
            return true;

        default:
            break;
        }
        break;

    case SDL_EVENT_TEXT_INPUT:
        EditBox_Insert(edit, event->text.text);
        return true;

    default:
        break;
    }
    return false;
}

