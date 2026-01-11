#include "../include/common.h"
#include "../include/cpu.h"
#include "../include/memory.h"
#include "../include/ppu.h"
#include <SDL2/SDL.h>
#include <stdio.h>


int main(int argc, char *argv[]) {
  printf("Starting GBA Emulator...\n");

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow(
      "GBA Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      GBA_SCREEN_WIDTH * 2, GBA_SCREEN_HEIGHT * 2, SDL_WINDOW_SHOWN);
  if (!window) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                           SDL_TEXTUREACCESS_STREAMING,
                                           GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT);

  // Hardware Initialization
  ARM7TDMI cpu;
  cpu_init(&cpu);
  memory_init();
  ppu_init(renderer, texture);

  bool quit = false;
  SDL_Event e;

  while (!quit) {
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) {
        quit = true;
      }
    }

    // Emulation Loop (Simplified)
    // In reality, you'd loop this many times per frame (approx 280k cycles)
    cpu_step(&cpu);
    ppu_step();

    // Render
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    ppu_update_texture(texture);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    // Cap FPS (Simple delay for now)
    SDL_Delay(16);
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
