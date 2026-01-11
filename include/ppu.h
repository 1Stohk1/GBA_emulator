#ifndef PPU_H
#define PPU_H

#include "common.h"
#include <SDL2/SDL.h>

// PPU Constants
#define GBA_SCREEN_WIDTH  240
#define GBA_SCREEN_HEIGHT 160

// Initialize PPU
void ppu_init(SDL_Renderer *renderer, SDL_Texture *texture);

// Execute one PPU cycle/scanline
void ppu_step(void);

// Update texture with frame buffer
void ppu_update_texture(SDL_Texture *texture);

#endif // PPU_H
