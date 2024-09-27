
# Migrating to SDL_ttf 3.0

This guide provides useful information for migrating applications from SDL_ttf 2.0 to SDL_ttf 3.0.

Functions that previously returned a negative error code now return bool.

Code that used to look like this:
```c
    if (TTF_Function() < 0 || TTF_Function() == -1) {
        /* Failure... */
    }
```
or
```c
    if (TTF_Function() == 0) {
        /* Success... */
    }
```
or
```c
    if (!TTF_Function()) {
        /* Success... */
    }
```
should be changed to:
```c
    if (TTF_Function()) {
        /* Success... */
    } else {
        /* Failure... */
    }
```

Several functions have been renamed. We have provided a handy semantic patch to migrate more easily to SDL3: [SDL_migration.cocci](https://github.com/libsdl-org/SDL_ttf/blob/main/build-scripts/SDL_migration.cocci)

In general we have switched to using UTF8 in the API. Functions which had 3 variants, for Latin-1, UTF-8, and UCS2, now accept UTF-8 text. In addition, those functions now have an optional length parameter which allows you to render substrings.

The following functions have been renamed:
* TTF_GlyphIsProvided32() => TTF_GlyphIsProvided()
* TTF_GlyphMetrics32() => TTF_GlyphMetrics()
* TTF_MeasureUTF8() => TTF_MeasureText()
* TTF_RenderGlyph32_Blended() => TTF_RenderGlyph_Blended()
* TTF_RenderGlyph32_LCD() => TTF_RenderGlyph_LCD()
* TTF_RenderGlyph32_Shaded() => TTF_RenderGlyph_Shaded()
* TTF_RenderGlyph32_Solid() => TTF_RenderGlyph_Solid()
* TTF_RenderUTF8_Blended() => TTF_RenderText_Blended()
* TTF_RenderUTF8_Blended_Wrapped() => TTF_RenderText_Blended_Wrapped()
* TTF_RenderUTF8_LCD() => TTF_RenderText_LCD()
* TTF_RenderUTF8_LCD_Wrapped() => TTF_RenderText_LCD_Wrapped()
* TTF_RenderUTF8_Shaded() => TTF_RenderText_Shaded()
* TTF_RenderUTF8_Shaded_Wrapped() => TTF_RenderText_Shaded_Wrapped()
* TTF_RenderUTF8_Solid() => SDL_RenderText_Solid()
* TTF_RenderUTF8_Solid_Wrapped() => TTF_RenderText_Solid_Wrapped()
* TTF_SizeUTF8() => TTF_SizeText()

The following functions have been removed:
* TTF_ByteSwappedUNICODE()
* TTF_GetFontKerningSizeGlyphs() - replaced with TTF_GetGlyphKerning()
* TTF_GetFontKerningSizeGlyphs32() - replaced with TTF_GetGlyphKerning()
* TTF_MeasureUNICODE()
* TTF_OpenFontDPI() - replaced with TTF_OpenFontWithProperties()
* TTF_OpenFontDPIIO() - replaced with TTF_OpenFontWithProperties()
* TTF_OpenFontIndex() - replaced with TTF_OpenFontWithProperties()
* TTF_OpenFontIndexDPI() - replaced with TTF_OpenFontWithProperties()
* TTF_OpenFontIndexDPIIO() - replaced with TTF_OpenFontWithProperties()
* TTF_OpenFontIndexIO() - replaced with TTF_OpenFontWithProperties()
* TTF_RenderUNICODE_Blended()
* TTF_RenderUNICODE_Blended_Wrapped()
* TTF_RenderUNICODE_LCD()
* TTF_RenderUNICODE_LCD_Wrapped()
* TTF_RenderUNICODE_Shaded()
* TTF_RenderUNICODE_Shaded_Wrapped()
* TTF_RenderUNICODE_Solid()
* TTF_RenderUNICODE_Solid_Wrapped()
* TTF_SizeUNICODE()
