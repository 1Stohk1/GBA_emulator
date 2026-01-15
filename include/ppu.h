#ifndef PPU_H
#define PPU_H

#include "common.h"

#ifdef USE_SDL
#include <SDL.h>
#else
// Dummy types for headless mode
typedef void SDL_Renderer;
typedef void SDL_Texture;
#endif

// PPU Constants
#define GBA_SCREEN_WIDTH 240
#define GBA_SCREEN_HEIGHT 160

// Initialize PPU
void ppu_init(SDL_Renderer *renderer, SDL_Texture *texture);

// Execute one PPU cycle/scanline
// Execute one PPU cycle/scanline
void ppu_step(void);

// Update PPU state based on CPU cycles
void ppu_update(int cycles);

// Update texture with frame buffer
void ppu_update_texture(SDL_Texture *texture);

// Render one scanline in Mode 0 (Headless/Test)
void ppu_render_scanline_mode0(u32 *scanline_buffer, int line);

// Save screenshot to PPM file (Headless Debug)
void ppu_save_screenshot(const char *filename);

#endif // PPU_H
