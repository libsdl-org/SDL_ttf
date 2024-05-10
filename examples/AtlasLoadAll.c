#include <SDL3/SDL_rect.h>
#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

SDL_bool useDark, forceDark = 1;
TTF_AtlasInfo atlas;
SDL_Texture*fontTexture;
SDL_Renderer*renderer;
int illegalCharIndex, baseline, lineHeight;

void DrawString(const char*text, int x, int y);
void DrawWindow(void);

int main(int argc, char *argv[])
{
	char buffer[4096];
	const char*filename=0;
	for(int i=1; i<argc; i++) {
		if (argv[i][0] == '-') {
			//Ignore flags
		} else {
			filename = argv[i];
			break;
		}
	}
#ifdef __WIN32__
	if (!filename) {
		#error "TODO"
		filename="some path.ttf";
	}
#else
	if (!filename) {
		buffer[0] = 0;
		FILE*f = popen("fc-match -f %{file}", "r");
		if (f) {
			int len = fread(buffer, 1, sizeof(buffer), f);
			pclose(f);
			buffer[len] = 0;
			filename = buffer;
		}
	}
#endif

	TTF_Init();

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "SDL init failed\n");
		return 1;
	}

	useDark = SDL_GetSystemTheme() == SDL_SYSTEM_THEME_DARK;
	useDark |= forceDark;
	SDL_Window*window = SDL_CreateWindow("Atlas LoadAll Test", 1280, 720, SDL_WINDOW_RESIZABLE);
	if (window == 0) {
		fprintf(stderr, "SDL_CreateWindow failed\n");
		return 1;
	}

	renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_PRESENTVSYNC);

	fontTexture = TTF_AtlasLoadAll(filename, 16, renderer, &atlas);
	if (!fontTexture) {
		fprintf(stderr, "Failed to create font texture. Error code %d\n", atlas.errorCode);
		return 1;
	}

	//We choose the baseline and line height here
	//E is a tall common letter, q is a common letter with pixels under the baseline (descent)
	//Some letters might be taller than E or go further down than q but that's fine
	//We choose the baseline as the ascent of E plus some padding from the top of the line
	//Typically the descent is a negative value because it's below the baseline
	//We choose lineHeight by using baseline (which has padding) subtract the (negative) descent value giving us a larger positive value
	{
		int foundCount = 0, chosenAscent=0, chosenDescent=0;
		//The C++ version has a nicer example using a hashtable
		for(int i=0; i<atlas.entryLength && foundCount < 3; i++) {
			if (atlas.ids[i] == 'E')
				chosenAscent = atlas.entries[i].ascent;
			else if (atlas.ids[i] == 'q') {
				chosenDescent = atlas.entries[i].ascent - atlas.entries[i].height; //descent is typically a negative value
			} else if (atlas.ids[i] == 0xFFFD) { // Unicode value of �
				illegalCharIndex = i;
				foundCount++;
			}
		}
		baseline = chosenAscent + 2; //Some chars may be taller than 'E' and we would want pixels between lines
		lineHeight = baseline - chosenDescent; //descent is typically a negative value
	}

	DrawWindow();
	
	SDL_bool wantQuit = 0;
	SDL_Event event;
	while (!wantQuit) {
		if (!SDL_WaitEvent(&event)) { wantQuit = 1; break; }
		switch (event.type) {
			case SDL_EVENT_QUIT: { wantQuit = 1; } break;
			case SDL_EVENT_KEY_DOWN: {
				if (event.key.keysym.sym == SDLK_ESCAPE) { wantQuit = 1; break; }
			} break;
		}
		DrawWindow();
	}
	
	SDL_DestroyTexture(fontTexture); //Perhaps destory atlas also handle this?
	TTF_DestroyAtlas(&atlas);
	TTF_Quit();
	return 0;
}

void DrawWindow(void) {
	if (useDark) {
		SDL_SetRenderDrawColor(renderer, 0x10, 0x10, 0x10, 255);
		SDL_SetTextureColorMod(fontTexture, 0xE0, 0xE0, 0xE0);
	} else {
		SDL_SetRenderDrawColor(renderer, 0xF0, 0xF0, 0xF0, 255);
		SDL_SetTextureColorMod(fontTexture, 0x10, 0x10, 0x10);
	}

	SDL_RenderClear(renderer);
	
	SDL_FRect r = {0, 0, atlas.width, atlas.height};
	if (useDark) SDL_SetRenderDrawColor(renderer, 0, 0, 0x40, 255); else { SDL_SetRenderDrawColor(renderer, 0xC0, 0xC0, 0xFF, 255); }
	SDL_RenderFillRect(renderer, &r);
	SDL_RenderTexture(renderer, fontTexture, &r, &r);
	
	// To test missing characters we replace ~ and ` in our draw function
	DrawString("Test string pq ~ `\nYou may choose the line\nheight however you like\n\nThe quick brown fox\njumped over the lazy dog", 1024+4, lineHeight);
	SDL_RenderPresent(renderer);
}

//The C++ example uses a hashtable
TTF_AtlasEntry*LookupGlyph(int c) {
	for(int i=0; i<atlas.entryLength; i++) {
		if ((int)atlas.ids[i] == c)
			return  &atlas.entries[i];
	}
	return &atlas.entries[illegalCharIndex]; //Usually index of FFFD
}

void DrawString(const char*text, int x, int y) {
	if (!text) return;
	const char*p = text;
	int startX = x;
	while (*p) {
		Sint32 glyph = *p++;
		
		if (glyph == '~') { glyph = 0x2603; } //2603 is snowman ☃ which is likely missing in the choosen font
		if (glyph == '`') { glyph = 0xEC08; } //Private font

		if (glyph == ' ') {
			x += atlas.spaceXAdv;
			continue;
		} else if (glyph == '\n') {
			x = startX;
			y += lineHeight;
			continue;
		}
		TTF_AtlasEntry*e = LookupGlyph(glyph);
		SDL_FRect srcRect = {e->canvasX, e->canvasY, e->width, e->height};
		SDL_FRect dstRect = {x + e->left, y + baseline - e->ascent, e->width, e->height};
		x += e->xAdv;
		SDL_RenderTexture(renderer, fontTexture, &srcRect, &dstRect);
	}
}
