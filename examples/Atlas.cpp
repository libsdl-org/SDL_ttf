#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <stdio.h>
#include <SDL3/SDL.h>
#include "SDL_ttf_atlas.h"
#include <unordered_map>

SDL_bool useDark, forceDark = 1;
int renderStyle = 0;
SDL_Texture*fontTexture;
SDL_Renderer*renderer;
std::unordered_map<Uint64, TTF_AtlasEntry*> table; //It's more readable to use TTF_AtlasEntry* than mapping to an index

int spaceXAdv16, baseline16, lineHeight16;
int spaceXAdv20, baseline20, lineHeight20;

static void DrawWindow(int textureWidth, int textureHeight);
static void DrawString(const char*text, int x, int y, int spaceXAdv, int baseline, int lineHeight, Uint64 xorID);

static Uint64 AtlasPred(void*user, int glyph) { return glyph ^ (Uint64)user; }

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
	SDL_Window*window = SDL_CreateWindow("Atlas Test", 1280, 720, SDL_WINDOW_RESIZABLE);
	if (window == 0) {
		fprintf(stderr, "SDL_CreateWindow failed\n");
		return 1;
	}

	renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_PRESENTVSYNC);

	TTF_AtlasInfo atlas;

	Uint64 tag16 = 16ULL << 32;
	Uint64 tag20 = 20ULL << 32;
	auto atlasState = TTF_AtlasInit(&atlas, 0, 0, 0);
	//spaceXAdv16 = TTF_AtlasLoadPredicate(filename, 16, atlasState, AtlasPred, (void*)tag16); //This works too
	spaceXAdv16 = TTF_AtlasLoad(filename, 16, atlasState, 0, 0, 0, tag16); //null range and null list means load everything
	
	int range20[]={'a', 'z'};
	//Y is intentionally missing to show we didn't load everything
	spaceXAdv20 = TTF_AtlasLoad(filename, 20, atlasState, range20, 2, "TE\uFFFD", tag20);

	/*
	auto pixels = TTF_AtlasDeinitWithPixels(atlasState);
	SDL_Surface*canvas = SDL_CreateSurfaceFrom(pixels, atlas.width, atlas.height, atlas.width*4, SDL_PIXELFORMAT_ARGB8888);
	fontTexture = SDL_CreateTextureFromSurface(renderer, canvas);
	SDL_DestroySurface(canvas); //*/

	/*
	TTF_AtlasDeinitWithSurface(atlasState);
	fontTexture = SDL_CreateTextureFromSurface(renderer, atlas.surface); //*/
	
	fontTexture = TTF_AtlasDeinitWithTexture(atlasState, renderer);
	if (!fontTexture) {
		fprintf(stderr, "Failed to create font texture. Error code %d\n", atlas.errorCode);
		return 1;
	}

	for(int i = 0; i < atlas.entryLength; i++) {
		//We should use a hash function since sequential keys are not terrific
		table.insert({atlas.ids[i], &atlas.entries[i]});
	}

	//We choose the baseline and line height here
	//E is a tall common letter, q is a common letter with pixels under the baseline (descent)
	//Some letters might be taller than E or go further down than q but that's fine
	//We choose the baseline as the ascent of E plus some padding from the top of the line
	//Typically the descent is a negative value because it's below the baseline
	//We choose lineHeight by using baseline (which has padding) subtract the (negative) descent value giving us a larger positive value
	{
		baseline16 = table['E' | tag16]->ascent + 2;
		auto q = table['q' | tag16];
		lineHeight16 = baseline16 - (q->ascent - q->height);

		baseline20 = table['E' | tag20]->ascent + 2;
		auto q20 = table['q' | tag20];
		lineHeight20 = baseline20 - (q20->ascent - q20->height);
	}
	

	DrawWindow(atlas.width, atlas.height);
	
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
		DrawWindow(atlas.width, atlas.height);
	}
	
	SDL_DestroyTexture(fontTexture); //Perhaps destory atlas also handle this?
	TTF_DestroyAtlas(&atlas);
	TTF_Quit();
	return 0;
}

static void DrawWindow(int textureWidth, int textureHeight) {
	//Choose background and text color of canvas, canvas BG is later
	if (useDark) {
		SDL_SetRenderDrawColor(renderer, 0x10, 0x10, 0x10, 255);
		SDL_SetTextureColorMod(fontTexture, 0xE0, 0xE0, 0xE0);
	} else {
		SDL_SetRenderDrawColor(renderer, 0xF0, 0xF0, 0xF0, 255);
		SDL_SetTextureColorMod(fontTexture, 0x10, 0x10, 0x10);
	}

	SDL_RenderClear(renderer);

	//Choose canvas background and draw our texture on top
	SDL_FRect r = {0, 0, textureWidth, textureHeight};
	if (useDark) SDL_SetRenderDrawColor(renderer, 0, 0, 0x40, 255); else { SDL_SetRenderDrawColor(renderer, 0xC0, 0xC0, 0xFF, 255); }
	SDL_RenderFillRect(renderer, &r);
	SDL_RenderTexture(renderer, fontTexture, &r, &r);
	
	// To test missing characters we replace ~ and ` in our draw function
	const char *testString = "Test string pq ~ `\nYou may choose the line\nheight however you like\n\nThe quick brown fox\njumped over the lazy dog";
	DrawString(testString, 1024+4,  20, spaceXAdv16, baseline16, lineHeight16, 16ULL<<32);

	SDL_SetTextureColorMod(fontTexture, 0x40, 0x80, 0x40);
	DrawString(testString, 1024+4, 240, spaceXAdv20, baseline20, lineHeight20, 20ULL<<32); //Y is intentionally missing to show we didn't load everything
	SDL_RenderPresent(renderer);
}
static void DrawString(const char*text, int x, int y, int spaceXAdv, int baseline, int lineHeight, Uint64 xorID) {
	if (!text) return;
	const char*p = text;
	int startX = x;
	while (*p) {
		Sint32 glyph = *p++;
		
		if (glyph == '~') { glyph = 0x2603; } //2603 is snowman ☃ which is likely missing in the choosen font
		if (glyph == '`') { glyph = 0xEC08; } //Private font

		if (glyph == ' ') {
			x += spaceXAdv;
			continue;
		} else if (glyph == '\n') {
			x = startX;
			y += lineHeight;
			continue;
		}
		
		TTF_AtlasEntry*e = table[ (table.contains(glyph ^ xorID) ? glyph : 0xFFFD) ^ xorID ];
		SDL_FRect srcRect = {e->canvasX, e->canvasY, e->width, e->height};
		SDL_FRect dstRect = {x + e->left, y + baseline - e->ascent, e->width, e->height};
		x += e->xAdv;
		SDL_RenderTexture(renderer, fontTexture, &srcRect, &dstRect);
	}
}
