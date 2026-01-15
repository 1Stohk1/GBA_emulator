#include "../include/common.h"
#include "../include/cpu.h"
#include "../include/memory.h"
#include "../include/ppu.h"
#include "../include/common.h"
#include "../include/cpu.h"
#include "../include/memory.h"
#include "../include/ppu.h"
#include <stdio.h>

#ifdef USE_SDL
#include <SDL.h>
#else
// Dummy SDL stubs for headless mode
typedef void SDL_Window;
typedef void SDL_Renderer;
typedef void SDL_Texture;
typedef struct { int type; } SDL_Event;
typedef struct { int sym; } SDL_Keysym;
typedef struct { SDL_Keysym keysym; } SDL_KeyboardEvent;
#define SDL_INIT_VIDEO 0
#define SDL_WINDOWPOS_UNDEFINED 0
#define SDL_WINDOW_SHOWN 0
#define SDL_RENDERER_ACCELERATED 0
#define SDL_PIXELFORMAT_ARGB8888 0
#define SDL_TEXTUREACCESS_STREAMING 0
#define SDL_QUIT 0x100
#define SDL_KEYDOWN 0x200
#define SDL_KEYUP 0x300
#define SDLK_x 0
#define SDLK_z 0
#define SDLK_BACKSPACE 0
#define SDLK_RETURN 0
#define SDLK_RIGHT 0
#define SDLK_LEFT 0
#define SDLK_UP 0
#define SDLK_DOWN 0
#define SDLK_a 0
#define SDLK_s 0
#endif

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);
#ifdef USE_SDL
  printf("Starting GBA Emulator (SDL)\n");
#else
  printf("Starting GBA Emulator (Headless)\n");
#endif

#ifdef USE_SDL
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
#else
  SDL_Window *window = NULL;
  SDL_Renderer *renderer = NULL;
  SDL_Texture *texture = NULL;
#endif

  // Hardware Initialization
  ARM7TDMI cpu;
  cpu_init(&cpu);
  memory_init();
  ppu_init(renderer, texture);

  char *rom_filename = "test.gba";
  if (argc > 1) {
    rom_filename = argv[1];
  }

  if (!memory_load_rom(rom_filename)) {
    printf("Failed to load %s. Exiting.\n", rom_filename);
    return 1;
  }

  // Direct Boot Setup
  cpu.r[REG_PC] = 0x08000000;
  cpu.cpsr = 0x1F; // System Mode
  cpu.r[REG_SP] = 0x03007F00; // Stack Pointer
  printf("Direct Boot: PC=%08X, CPSR=%08X, SP=%08X\n", cpu.r[REG_PC], cpu.cpsr,
         cpu.r[REG_SP]);

  bool quit = false;
#ifdef USE_SDL
  SDL_Event e;
#endif

  // Headless loop limit
  int max_cycles = 50000000;
#ifdef USE_SDL
  (void)max_cycles; // Unused in SDL mode (runs until quit)
#endif
  int total_cycles = 0;

  while (!quit) {
#ifdef USE_SDL
    // Input Handling (SDL)
    static u16 key_state = 0x03FF;
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) {
        quit = true;
      } else if (e.type == SDL_KEYDOWN) {
         // ... (Key mapping)
         // Simplified for brevity in replacement check
      }
    }
    memory_set_key_state(key_state);
#else
    // Headless Input (Mock)
    // Could simulate key presses here
    if (total_cycles > max_cycles) quit = true;
#endif

    // Emulation Loop
    int cycles_per_frame = 280896;
    int cycles_run = 0;

    while (cycles_run < cycles_per_frame) {
      int cycles = cpu_step(&cpu);
      ppu_update(cycles);
      timer_step(cycles);
      cycles_run += cycles;
      total_cycles += cycles;
#ifndef USE_SDL
      if (total_cycles > max_cycles) break;
#endif
    }

#ifdef USE_SDL
    // FPS Calculation
    static u32 start_time = 0;
    static int frames = 0;
    if (start_time == 0) start_time = SDL_GetTicks();
    frames++;
    
    if (SDL_GetTicks() - start_time >= 1000) {
        char title[128];
        sprintf(title, "GBA Emulator - FPS: %d - PC: %08X", frames, cpu.r[REG_PC]);
        SDL_SetWindowTitle(window, title);
        frames = 0;
        start_time = SDL_GetTicks();
    }
    
    // Render
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    ppu_update_texture(texture);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    SDL_Delay(16);
#else
    // Headless Render (Mock)
    ppu_update_texture(NULL); 
    // printf("Frame executed. PC: %08X\n", cpu.r[REG_PC]);
#endif
  }

#ifdef USE_SDL
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
#endif
  
#ifndef USE_SDL
  ppu_save_screenshot("screenshot.ppm");
#endif
  
  printf("Emulation finished (Headless limit reached or Quit).\n");
  return 0;
}
