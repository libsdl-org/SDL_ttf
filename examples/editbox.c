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


static void DrawText(EditBox *edit, TTF_Text *text, float x, float y)
{
#ifdef TEST_SURFACE_ENGINE
    if (edit->window_surface) {
        /* Flush the renderer so we can draw directly to the window surface */
        SDL_FlushRenderer(edit->renderer);
        TTF_DrawSurfaceText(text, (int)SDL_roundf(x), (int)SDL_roundf(y), edit->window_surface);
        return;
    }
#endif /* TEST_SURFACE_ENGINE */

    TTF_DrawRendererText(text, x, y);
}

static bool GetHighlightExtents(EditBox *edit, int *marker, int *length)
{
    if (edit->highlight1 >= 0 && edit->highlight2 >= 0) {
        int marker1 = SDL_min(edit->highlight1, edit->highlight2);
        int marker2 = SDL_max(edit->highlight1, edit->highlight2);
        if (marker2 > marker1) {
            *marker = marker1;
            *length = marker2 - marker1;
            return true;
        }
    }
    return false;
}

static void ResetComposition(EditBox *edit)
{
    edit->composition_start = 0;
    edit->composition_length = 0;
    edit->composition_cursor = 0;
    edit->composition_cursor_length = 0;
}

static int UTF8ByteLength(const char *text, int num_codepoints)
{
    const char *start = text;
    while (num_codepoints > 0) {
        Uint32 ch = SDL_StepUTF8(&text, NULL);
        if (ch == 0) {
            break;
        }
        --num_codepoints;
    }
    return (int)(uintptr_t)(text - start);
}

static void HandleComposition(EditBox *edit, const SDL_TextEditingEvent *event)
{
    EditBox_DeleteHighlight(edit);

    if (edit->composition_length > 0) {
        TTF_DeleteTextString(edit->text, edit->composition_start, edit->composition_length);
        ResetComposition(edit);
    }

    int length = (int)SDL_strlen(event->text);
    if (length > 0) {
        edit->composition_start = edit->cursor;
        edit->composition_length = length;
        TTF_InsertTextString(edit->text, edit->composition_start, event->text, edit->composition_length);
        if (event->start > 0 || event->length > 0) {
            edit->composition_cursor = UTF8ByteLength(&edit->text->text[edit->composition_start], event->start);
            edit->composition_cursor_length = UTF8ByteLength(&edit->text->text[edit->composition_start + edit->composition_cursor], event->length);
        } else {
            edit->composition_cursor = length;
            edit->composition_cursor_length = 0;
        }
    }
}

static void CancelComposition(EditBox *edit)
{
    ResetComposition(edit);

    SDL_ClearComposition(edit->window);
}

static void DrawComposition(EditBox *edit)
{
    /* Draw an underline under the composed text */
    SDL_Renderer *renderer = edit->renderer;
    int font_height = TTF_GetFontHeight(edit->font);
    TTF_SubString **substrings = TTF_GetTextSubStringsForRange(edit->text, edit->composition_start, edit->composition_length, NULL);
    if (substrings) {
        for (int i = 0; substrings[i]; ++i) {
            SDL_FRect rect;
            SDL_RectToFRect(&substrings[i]->rect, &rect);
            rect.x += edit->rect.x;
            rect.y += (edit->rect.y + font_height);
            rect.h = 1.0f;
            SDL_RenderFillRect(renderer, &rect);
        }
        SDL_free(substrings);
    }

    /* Thicken the underline under the active clause in the composed text */
    if (edit->composition_cursor_length > 0) {
        substrings = TTF_GetTextSubStringsForRange(edit->text, edit->composition_start + edit->composition_cursor, edit->composition_cursor_length, NULL);
        if (substrings) {
            for (int i = 0; substrings[i]; ++i) {
                SDL_FRect rect;
                SDL_RectToFRect(&substrings[i]->rect, &rect);
                rect.x += edit->rect.x;
                rect.y += (edit->rect.y + font_height) - 1;
                rect.h = 1.0f;
                SDL_RenderFillRect(renderer, &rect);
            }
            SDL_free(substrings);
        }
    }
}

static void DrawCompositionCursor(EditBox *edit)
{
    SDL_Renderer *renderer = edit->renderer;
    if (edit->composition_cursor_length == 0) {
        TTF_SubString cursor;
        if (TTF_GetTextSubString(edit->text, edit->composition_start + edit->composition_cursor, &cursor)) {
            SDL_FRect rect;

            SDL_RectToFRect(&cursor.rect, &rect);
            if (TTF_GetFontDirection(edit->font) == TTF_DIRECTION_RTL) {
                rect.x += cursor.rect.w;
            }
            rect.x += edit->rect.x;
            rect.y += edit->rect.y;
            rect.w = 1.0f;

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
            SDL_RenderFillRect(renderer, &rect);
        }
    }
}

static void ClearCandidates(EditBox *edit)
{
    if (edit->candidates) {
        TTF_DestroyText(edit->candidates);
        edit->candidates = NULL;
    }
    edit->selected_candidate_start = 0;
    edit->selected_candidate_length = 0;
}

static void SaveCandidates(EditBox *edit, const SDL_Event *event)
{
    int i;

    ClearCandidates(edit);

    bool horizontal = event->edit_candidates.horizontal;
    int num_candidates = event->edit_candidates.num_candidates;
    int selected_candidate = event->edit_candidates.selected_candidate;

    /* Calculate the length of the candidates text */
    size_t length = 0;
    for (i = 0; i < num_candidates; ++i) {
        if (horizontal) {
            if (i > 0) {
                ++length;
            }
        }

        length += SDL_strlen(event->edit_candidates.candidates[i]);

        if (!horizontal) {
            length += 1;
        }
    }
    if (length == 0) {
        return;
    }
    ++length; /* For null terminator */

    char *candidate_text = (char *)SDL_malloc(length);
    if (!candidate_text) {
        return;
    }

    char *dst = candidate_text;
    for (i = 0; i < num_candidates; ++i) {
        if (horizontal) {
            if (i > 0) {
                *dst++ = ' ';
            }
        }

        int length = (int)SDL_strlen(event->edit_candidates.candidates[i]);
        if (i == selected_candidate) {
            edit->selected_candidate_start = (int)(uintptr_t)(dst - candidate_text);
            edit->selected_candidate_length = length;
        }
        SDL_memcpy(dst, event->edit_candidates.candidates[i], length);
        dst += length;

        if (!horizontal) {
            *dst++ = '\n';
        }
    }
    *dst = '\0';

    edit->candidates = TTF_CreateText(TTF_GetTextEngine(edit->text), edit->font, candidate_text, 0);
    SDL_free(candidate_text);
    if (edit->candidates) {
        float r, g, b, a;
        TTF_GetTextColorFloat(edit->text, &r, &g, &b, &a);
        TTF_SetTextColorFloat(edit->candidates, r, g, b, a);
    } else {
        ClearCandidates(edit);
    }
}

static void DrawCandidates(EditBox *edit)
{
    SDL_Renderer *renderer = edit->renderer;
    SDL_Rect safe_rect;
    SDL_FRect candidates_rect;
    int candidates_w;
    int candidates_h;
    float x, y;

    /* Position the candidate window */
    TTF_SubString cursor;
    int offset = edit->composition_start;
    if (edit->composition_cursor_length > 0) {
        // Place the candidates at the active clause
        offset += edit->composition_cursor;
    }
    if (!TTF_GetTextSubString(edit->text, offset, &cursor)) {
        return;
    }

    SDL_GetRenderSafeArea(renderer, &safe_rect);
    TTF_GetTextSize(edit->candidates, &candidates_w, &candidates_h);
    candidates_rect.x = edit->rect.x + cursor.rect.x;
    candidates_rect.y = edit->rect.y + cursor.rect.y + cursor.rect.h + 2.0f;
    candidates_rect.w = 1.0f + 2.0f + candidates_w + 2.0f + 1.0f;
    candidates_rect.h = 1.0f + 2.0f + candidates_h + 2.0f + 1.0f;
    if ((candidates_rect.x + candidates_rect.w) > safe_rect.w) {
        candidates_rect.x = (safe_rect.w - candidates_rect.w);
        if (candidates_rect.x < 0.0f) {
            candidates_rect.x = 0.0f;
        }
    }

    /* Draw the candidate background */
    SDL_SetRenderDrawColor(renderer, 0xAA, 0xAA, 0xAA, 0xFF);
    SDL_RenderFillRect(renderer, &candidates_rect);
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderRect(renderer, &candidates_rect);

    /* Draw the candidates */
    x = candidates_rect.x + 3.0f;
    y = candidates_rect.y + 3.0f;
    DrawText(edit, edit->candidates, x, y);

    /* Underline the selected candidate */
    if (edit->selected_candidate_length > 0) {
        int font_height = TTF_GetFontHeight(edit->font);
        TTF_SubString **substrings = TTF_GetTextSubStringsForRange(edit->candidates, edit->selected_candidate_start, edit->selected_candidate_length, NULL);
        if (substrings) {
            for (int i = 0; substrings[i]; ++i) {
                SDL_FRect rect;
                SDL_RectToFRect(&substrings[i]->rect, &rect);
                rect.x += x;
                rect.y += (y + font_height);
                rect.h = 1.0f;
                SDL_RenderFillRect(renderer, &rect);
            }
            SDL_free(substrings);
        }
    }
}

static void UpdateTextInputArea(EditBox *edit)
{
    /* Convert the text input area and cursor into window coordinates */
    SDL_Renderer *renderer = edit->renderer;
    SDL_FPoint window_edit_rect_min;
    SDL_FPoint window_edit_rect_max;
    SDL_FPoint window_cursor;
    if (!SDL_RenderCoordinatesToWindow(renderer, edit->rect.x, edit->rect.y, &window_edit_rect_min.x, &window_edit_rect_min.y) ||
        !SDL_RenderCoordinatesToWindow(renderer, edit->rect.x + edit->rect.w, edit->rect.y + edit->rect.h, &window_edit_rect_max.x, &window_edit_rect_max.y) ||
        !SDL_RenderCoordinatesToWindow(renderer, edit->cursor_rect.x, edit->cursor_rect.y, &window_cursor.x, &window_cursor.y)) {
        return;
    }

    SDL_Rect rect;
    rect.x = (int)SDL_roundf(window_edit_rect_min.x);
    rect.y = (int)SDL_roundf(window_edit_rect_min.y);
    rect.w = (int)SDL_roundf(window_edit_rect_max.x - window_edit_rect_min.x);
    rect.h = (int)SDL_roundf(window_edit_rect_max.y - window_edit_rect_min.y);
    int cursor_offset = (int)SDL_roundf(window_cursor.x - window_edit_rect_min.x);
    SDL_SetTextInputArea(edit->window, &rect, cursor_offset);
}

static void DrawCursor(EditBox *edit)
{
    if (edit->composition_length > 0) {
        DrawCompositionCursor(edit);
        return;
    }

    SDL_Renderer *renderer = edit->renderer;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
    SDL_RenderFillRect(renderer, &edit->cursor_rect);
}

EditBox *EditBox_Create(SDL_Window *window, SDL_Renderer *renderer, TTF_TextEngine *engine, TTF_Font *font, const SDL_FRect *rect)
{
    EditBox *edit = (EditBox *)SDL_calloc(1, sizeof(*edit));
    if (!edit) {
        return NULL;
    }

    edit->window = window;
    edit->renderer = renderer;
    edit->font = font;
    edit->text = TTF_CreateText(engine, font, NULL, 0);
    if (!edit->text) {
        EditBox_Destroy(edit);
        return NULL;
    }
    edit->rect = *rect;
    edit->highlight1 = -1;
    edit->highlight2 = -1;

    /* Wrap the editbox text within the editbox area */
    TTF_SetTextWrapWidth(edit->text, (int)SDL_floorf(rect->w));

    /* Show the whitespace when wrapping, so it can be edited */
    TTF_SetTextWrapWhitespaceVisible(edit->text, true);

#ifdef TEST_SURFACE_ENGINE
    /* Grab the window surface if we want to test the surface text engine.
     * This isn't strictly necessary, we can still use the renderer if it's
     * a software renderer targeting an SDL_Surface.
     */
    edit->window_surface = (SDL_Surface *)SDL_GetPointerProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_SURFACE_POINTER, NULL);
#endif

    /* We support rendering the composition and candidates */
    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition,candidates");

    return edit;
}

void EditBox_Destroy(EditBox *edit)
{
    if (!edit) {
        return;
    }

    ClearCandidates(edit);
    TTF_DestroyText(edit->text);
    SDL_free(edit);
}

void EditBox_SetFocus(EditBox *edit, bool focus)
{
    if (!edit) {
        return;
    }

    if (edit->has_focus == focus) {
        return;
    }

    edit->has_focus = focus;

    if (edit->has_focus) {
        SDL_StartTextInput(edit->window);
    } else {
        SDL_StopTextInput(edit->window);
    }
}

void EditBox_Draw(EditBox *edit)
{
    if (!edit) {
        return;
    }

    SDL_Renderer *renderer = edit->renderer;
    float x = edit->rect.x;
    float y = edit->rect.y;

    /* Draw any highlight */
    int marker, length;
    if (GetHighlightExtents(edit, &marker, &length)) {
        TTF_SubString **highlights = TTF_GetTextSubStringsForRange(edit->text, marker, length, NULL);
        if (highlights) {
            int i;
            SDL_SetRenderDrawColor(renderer, 0xEE, 0xEE, 0x00, 0xFF);
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

    DrawText(edit, edit->text, x, y);

    if (edit->has_focus) {
        /* Draw the cursor */
        Uint64 now = SDL_GetTicks();
        if ((now - edit->last_cursor_change) >= CURSOR_BLINK_INTERVAL_MS) {
            edit->cursor_visible = !edit->cursor_visible;
            edit->last_cursor_change = now;
        }

        /* Calculate the cursor rect, used for positioning candidates */
        TTF_SubString cursor;
        if (TTF_GetTextSubString(edit->text, edit->cursor, &cursor)) {
            SDL_FRect cursor_rect;
            SDL_RectToFRect(&cursor.rect, &cursor_rect);
            if (TTF_GetFontDirection(edit->font) == TTF_DIRECTION_RTL) {
                cursor_rect.x += cursor.rect.w;
            }
            cursor_rect.x += edit->rect.x;
            cursor_rect.y += edit->rect.y;
            cursor_rect.w = 1.0f;
            SDL_copyp(&edit->cursor_rect, &cursor_rect);

            UpdateTextInputArea(edit);
        }

        if (edit->composition_length > 0) {
            DrawComposition(edit);
        }

        if (edit->candidates) {
            DrawCandidates(edit);
        }

        if (edit->cursor_visible) {
            DrawCursor(edit);
        }
    }
}

static int GetCursorTextIndex(TTF_Font *font, int x, const TTF_SubString *substring)
{
    if (substring->flags & (TTF_SUBSTRING_LINE_END | TTF_SUBSTRING_TEXT_END)) {
        return substring->offset;
    }

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

static void SetCursorPosition(EditBox *edit, int position)
{
    if (edit->composition_length > 0) {
        /* Don't let the cursor be moved into the composition */
        if (position >= edit->composition_start && position <= (edit->composition_start + edit->composition_length)) {
            return;
        }

        CancelComposition(edit);
    }

    edit->cursor = position;
}

static void MoveCursorIndex(EditBox *edit, int direction)
{
    TTF_SubString substring;

    if (direction < 0) {
        if (TTF_GetTextSubString(edit->text, edit->cursor - 1, &substring)) {
            SetCursorPosition(edit, substring.offset);
        }
    } else {
        if (TTF_GetTextSubString(edit->text, edit->cursor, &substring) &&
            TTF_GetTextSubString(edit->text, substring.offset + SDL_max(substring.length, 1), &substring)) {
            SetCursorPosition(edit, substring.offset);
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
            x = substring.rect.x + substring.rect.w - 1;
        } else {
            x = substring.rect.x;
        }
        y = substring.rect.y - fontHeight / 2;
        if (TTF_GetTextSubStringForPoint(edit->text, x, y, &substring)) {
            SetCursorPosition(edit, GetCursorTextIndex(edit->font, x, &substring));
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
            x = substring.rect.x + substring.rect.w - 1;
        } else {
            x = substring.rect.x;
        }
        y = substring.rect.y + substring.rect.h + fontHeight / 2;
        if (TTF_GetTextSubStringForPoint(edit->text, x, y, &substring)) {
            SetCursorPosition(edit, GetCursorTextIndex(edit->font, x, &substring));
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
        SetCursorPosition(edit, substring.offset);
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
        SetCursorPosition(edit, substring.offset + substring.length);
    }
}

void EditBox_MoveCursorBeginning(EditBox *edit)
{
    if (!edit) {
        return;
    }

    /* Move to the beginning of the text */
    SetCursorPosition(edit, 0);
}

void EditBox_MoveCursorEnd(EditBox *edit)
{
    if (!edit) {
        return;
    }

    /* Move to the end of the text */
    if (edit->text->text) {
        SetCursorPosition(edit, (int)SDL_strlen(edit->text->text));
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

    if (edit->cursor > 0) {
        const char *start = &edit->text->text[edit->cursor];
        const char *next = start;
        SDL_StepBackUTF8(edit->text->text, &next);
        int length = (int)(uintptr_t)(start - next);
        TTF_DeleteTextString(edit->text, edit->cursor - length, length);
        edit->cursor -= length;
    }
}

void EditBox_BackspaceToBeginning(EditBox *edit)
{
    if (!edit) {
        return;
    }

    /* Delete to the beginning of the string */
    TTF_DeleteTextString(edit->text, 0, edit->cursor);
    SetCursorPosition(edit, 0);
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
        if (edit->has_focus) {
            EditBox_SetFocus(edit, false);
            return true;
        }
        return false;
    }

    if (!edit->has_focus) {
        EditBox_SetFocus(edit, true);
    }

    /* Set the cursor position */
    TTF_SubString substring;
    int textX = (int)SDL_roundf(x - edit->rect.x);
    int textY = (int)SDL_roundf(y - edit->rect.y);
    if (!TTF_GetTextSubStringForPoint(edit->text, textX, textY, &substring)) {
        SDL_Log("Couldn't get cursor location: %s\n", SDL_GetError());
        return false;
    }

    SetCursorPosition(edit, GetCursorTextIndex(edit->font, textX, &substring));
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

    SetCursorPosition(edit, GetCursorTextIndex(edit->font, textX, &substring));
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

    int marker, length;
    if (GetHighlightExtents(edit, &marker, &length)) {
        TTF_DeleteTextString(edit->text, marker, length);
        SetCursorPosition(edit, marker);
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

    int marker, length;
    if (GetHighlightExtents(edit, &marker, &length)) {
        char *temp = (char *)SDL_malloc(length + 1);
        if (temp) {
            SDL_memcpy(temp, &edit->text->text[marker], length);
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
    int marker, length;
    if (GetHighlightExtents(edit, &marker, &length)) {
        char *temp = (char *)SDL_malloc(length + 1);
        if (temp) {
            SDL_memcpy(temp, &edit->text->text[marker], length);
            temp[length] = '\0';
            SDL_SetClipboardText(temp);
            SDL_free(temp);
        }
        TTF_DeleteTextString(edit->text, marker, length);
        SetCursorPosition(edit, marker);
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
    EditBox_Insert(edit, text);
}

void EditBox_Insert(EditBox *edit, const char *text)
{
    if (!edit || !text) {
        return;
    }

    EditBox_DeleteHighlight(edit);

    if (edit->composition_length > 0) {
        TTF_DeleteTextString(edit->text, edit->composition_start, edit->composition_length);
        edit->composition_length = 0;
    }

    size_t length = SDL_strlen(text);
    TTF_InsertTextString(edit->text, edit->cursor, text, length);
    SetCursorPosition(edit, (int)(edit->cursor + length));
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
        if (!edit->has_focus) {
            break;
        }

        switch (event->key.key) {
        case SDLK_A:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_SelectAll(edit);
            }
            break;
        case SDLK_C:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_Copy(edit);
            }
            break;

        case SDLK_V:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_Paste(edit);
            }
            break;

        case SDLK_X:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_Cut(edit);
            }
            break;

        case SDLK_LEFT:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_MoveCursorBeginningOfLine(edit);
            } else {
                EditBox_MoveCursorLeft(edit);
            }
            break;

        case SDLK_RIGHT:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_MoveCursorEndOfLine(edit);
            } else {
                EditBox_MoveCursorRight(edit);
            }
            break;

        case SDLK_UP:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_MoveCursorBeginning(edit);
            } else {
                EditBox_MoveCursorUp(edit);
            }
            break;

        case SDLK_DOWN:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_MoveCursorEnd(edit);
            } else {
                EditBox_MoveCursorDown(edit);
            }
            break;

        case SDLK_HOME:
            EditBox_MoveCursorBeginning(edit);
            break;

        case SDLK_END:
            EditBox_MoveCursorEnd(edit);
            break;

        case SDLK_BACKSPACE:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_BackspaceToBeginning(edit);
            } else {
                EditBox_Backspace(edit);
            }
            break;

        case SDLK_DELETE:
            if (event->key.mod & SDL_KMOD_CTRL) {
                EditBox_DeleteToEnd(edit);
            } else {
                EditBox_Delete(edit);
            }
            break;

        case SDLK_RETURN:
            EditBox_Insert(edit, "\n");
            break;

        case SDLK_ESCAPE:
            EditBox_SetFocus(edit, false);
            break;

        default:
            break;
        }
        return true;

    case SDL_EVENT_TEXT_INPUT:
        EditBox_Insert(edit, event->text.text);
        return true;

    case SDL_EVENT_TEXT_EDITING:
        HandleComposition(edit, &event->edit);
        break;

    case SDL_EVENT_TEXT_EDITING_CANDIDATES:
        ClearCandidates(edit);
        SaveCandidates(edit, event);
        break;

    default:
        break;
    }
    return false;
}

