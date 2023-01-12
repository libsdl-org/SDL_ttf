#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_ttf.h>

int main(int argc, char *argv[])
{
    if (SDL_Init(0) < 0) {
        SDL_Log("SDL_Init: could not initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() == -1) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
    }
    TTF_Quit();
    SDL_Quit();
    return 0;
}
