#include "../include/ppu.h"
#include "../include/memory.h"
#include <stdio.h>

// Internal PPU State
// static u16 vram[0x18000];
// static u16 palette[0x200];
// static u16 oam[0x400];

void ppu_init(SDL_Renderer *renderer, SDL_Texture *texture) {
  printf("PPU Initialized.\n");
}

// Scanline rendering logic

static int cycle_bucket = 0;
static int vcount = 0;

void ppu_update(int cycles) {
  cycle_bucket += cycles;

  while (cycle_bucket >= 1232) {
    cycle_bucket -= 1232;
    vcount++;

    if (vcount > 227) {
      vcount = 0;
    }

    // Update VCOUNT (0x04000006)
    u8 *io = memory_get_io();
    *(u16 *)&io[6] = vcount;

    // Update DISPSTAT (0x04000004) VBlank Flag (Bit 0)
    // VBlank is lines 160-227
    u16 *dispstat = (u16 *)&io[4];
    if (vcount >= 160 && vcount <= 227) {
      *dispstat |= 1; // Set VBlank
    } else {
      *dispstat &= ~1; // Clear VBlank
    }
  }
}

void ppu_update_texture(SDL_Texture *texture) {
  u16 *vram = (u16 *)memory_get_vram();
  u8 *io = memory_get_io();
  u16 *pal = (u16 *)memory_get_pal();

  // Read DISPCNT (0x04000000). Bit 0-2 is Video Mode.
  u16 dispcnt = *(u16 *)&io[0];
  u8 mode = dispcnt & 7;

  // Default to Mode 3 if 0 (since our test ROM doesn't set it yet, but
  // strictness is asked). Actually, let's allow Mode 0 to render nothing (or
  // default behavior) but force Mode 3 logic if valid.

  void *pixels;
  int pitch;
  if (SDL_LockTexture(texture, NULL, &pixels, &pitch) == 0) {
    u32 *dst = (u32 *)pixels;

    if (mode == 3) {
      // Mode 3: 240x160 15-bit Bitmap
      for (int i = 0; i < GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT; i++) {
        u16 color = vram[i];
        u8 r = (color & 0x1F) << 3;
        u8 g = ((color >> 5) & 0x1F) << 3;
        u8 b = ((color >> 10) & 0x1F) << 3;
        dst[i] = (255 << 24) | (r << 16) | (g << 8) | b;
      }
    } else if (mode == 4) {
      // Mode 4: 240x160 8-bit Bitmap (Page flipping logic needed, but
      // simplifying to Page 0) VRAM Page 0: 0x06000000 VRAM Page 1:
      // 0x0600A000 Read Frame Select bit (Bit 4 of DISPCNT)
      u8 *page_ptr = (u8 *)vram;
      if (dispcnt & 0x10)
        page_ptr += 0xA000;

      for (int i = 0; i < GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT; i++) {
        u8 index = page_ptr[i];
        u16 color = pal[index]; // Read from Palette RAM
        u8 r = (color & 0x1F) << 3;
        u8 g = ((color >> 5) & 0x1F) << 3;
        u8 b = ((color >> 10) & 0x1F) << 3;
        dst[i] = (255 << 24) | (r << 16) | (g << 8) | b;
      }
    } else {
      // Fallback / Mode 0 (Tile) - Not implemented, clear screen
      // But for "test.gba" compatibility which doesn't set DISPCNT,
      // if mode is 0, let's just clear for now OR assume Mode 3 if we want to
      // be lax. Let's be strict: if mode is 0, render black. (Wait, user just
      // saw Red pixel with Mode 0 implied? Ah, previous code forced Mode 3
      // logic regardless of DISPCNT). I will default to Mode 3 just for
      // "test.gba" compatibility until updated. Or better: update test.gba.
      // Let's implement Black screen for non-3/4.
      for (int i = 0; i < GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT; i++) {
        dst[i] = 0xFF000000; // Black
      }
    }
    SDL_UnlockTexture(texture);
  }
}
