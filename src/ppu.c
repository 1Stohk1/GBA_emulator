#include "../include/ppu.h"
#include <stdio.h>


// Internal PPU State
// static u16 vram[0x18000];
// static u16 palette[0x200];
// static u16 oam[0x400];

void ppu_init(SDL_Renderer *renderer, SDL_Texture *texture) {
  printf("PPU Initialized.\n");
}

void ppu_step(void) {
  // Scanline rendering logic would go here
}

void ppu_update_texture(SDL_Texture *texture) {
  // In a real emulator, we would unlock the texture, copy the framebuffer, and
  // lock it again. For this stub, we might just clear it or leave it as is.
}
