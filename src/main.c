#include "../include/common.h"
#include "../include/cpu.h"
#include "../include/memory.h"
#include "../include/ppu.h"
#include <SDL2/SDL.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);
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

  char *rom_filename = "test.gba";
  if (argc > 1) {
    rom_filename = argv[1];
  }

  if (!memory_load_rom(rom_filename)) {
    printf("Failed to load %s. Exiting.\n", rom_filename);
    // Clean up SDL?
    return 1;
  }

  // Direct Boot Setup
  cpu.r[REG_PC] = 0x08000000;
  // Set Thumb Mode (Bit 5 = 1) and Mode to System (0x1F) or User (0x10)?
  // User Prompt said: 0x0000003F (0x1F System Mode + 0x20 Thumb Bit)
  cpu.cpsr = 0x3F;
  cpu.r[REG_SP] = 0x03007F00; // Stack Pointer
  printf("Direct Boot: PC=%08X, CPSR=%08X, SP=%08X\n", cpu.r[REG_PC], cpu.cpsr,
         cpu.r[REG_SP]);

  bool quit = false;
  SDL_Event e;

  while (!quit) {
    // Input Handling
    // GBA Keys: A, B, Select, Start, Right, Left, Up, Down, R, L
    // Bitmask:  0  1  2       3      4      5     6   7     8  9
    // Active LOW (0=Pressed)
    // Initialize Key State (set all released: 1)
    // We maintain a persistent state outside the poll loop to handle multiple
    // keys
    static u16 key_state = 0x03FF;

    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) {
        quit = true;
      } else if (e.type == SDL_KEYDOWN) {
        switch (e.key.keysym.sym) {
        case SDLK_x:
          key_state &= ~(1 << 0);
          break; // A
        case SDLK_z:
          key_state &= ~(1 << 1);
          break; // B
        case SDLK_BACKSPACE:
          key_state &= ~(1 << 2);
          break; // Select
        case SDLK_RETURN:
          key_state &= ~(1 << 3);
          break; // Start
        case SDLK_RIGHT:
          key_state &= ~(1 << 4);
          break; // Right
        case SDLK_LEFT:
          key_state &= ~(1 << 5);
          break; // Left
        case SDLK_UP:
          key_state &= ~(1 << 6);
          break; // Up
        case SDLK_DOWN:
          key_state &= ~(1 << 7);
          break; // Down
        case SDLK_a:
          key_state &= ~(1 << 8);
          break; // R
        case SDLK_s:
          key_state &= ~(1 << 9);
          break; // L
        }
        // printf("[INPUT] KeyDown: %04X\n", key_state);
      } else if (e.type == SDL_KEYUP) {
        switch (e.key.keysym.sym) {
        case SDLK_x:
          key_state |= (1 << 0);
          break;
        case SDLK_z:
          key_state |= (1 << 1);
          break;
        case SDLK_BACKSPACE:
          key_state |= (1 << 2);
          break;
        case SDLK_RETURN:
          key_state |= (1 << 3);
          break;
        case SDLK_RIGHT:
          key_state |= (1 << 4);
          break;
        case SDLK_LEFT:
          key_state |= (1 << 5);
          break;
        case SDLK_UP:
          key_state |= (1 << 6);
          break;
        case SDLK_DOWN:
          key_state |= (1 << 7);
          break;
        case SDLK_a:
          key_state |= (1 << 8);
          break;
        case SDLK_s:
          key_state |= (1 << 9);
          break;
        }
      }
    }

    memory_set_key_state(key_state);

    // Emulation Loop
    // GBA is 16.78MHz. Cycles per frame = 1232 * 228 = 280896.
    int cycles_per_frame = 280896;
    int cycles_run = 0;

    // printf("Starting Frame Loop\n"); // Reduced logging
    while (cycles_run < cycles_per_frame) {
      int cycles = cpu_step(&cpu);
      ppu_update(cycles);
      cycles_run += cycles;
    }

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
