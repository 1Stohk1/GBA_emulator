#include "../include/ppu.h"
#include "../include/memory.h"
#include <stdio.h>
#include <string.h>

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

// Helper: Read palette color
u16 ppu_read_palette(int index) {
    u8 *pal = memory_get_pal();
    return *(u16 *)&pal[index * 2];
}

void ppu_render_scanline_mode0(u32 *scanline_buffer, int line) {
    u8 *io = memory_get_io();
    u8 *vram = memory_get_vram();
    u16 dispcnt = *(u16 *)&io[0];

    // For each BG (0-3)
    // Simplified: Just render BG0 for now or loop all enabled
    // Bit 8=BG0, 9=BG1, 10=BG2, 11=BG3
    
    // Clear scanline (Transparent / Backdrop?)
    // Using Palette 0 color 0 usually (Transparent)
    for(int x=0; x<GBA_SCREEN_WIDTH; x++) scanline_buffer[x] = 0; // Black/Transparent

    // Render BGs in order 3 -> 0 (Priority TODO)
    // Logic: Iterate BGs with priority. For now, simple loop 3 down to 0? 
    // Usually hardware sorts by priority registers.
    // Let's just implement BG0 for starters as it's common.
    
    if (dispcnt & 0x0100) { // BG0 Enabled
        u16 bg0cnt = *(u16 *)&io[0x08];
        int priority = bg0cnt & 3;
        int char_base_block = (bg0cnt >> 2) & 3;
        int mosaic = (bg0cnt >> 6) & 1;
        int color_mode = (bg0cnt >> 7) & 1; // 0=16/16, 1=256/1
        int screen_base_block = (bg0cnt >> 8) & 0x1F;
        int size = (bg0cnt >> 14) & 3; 

        // Screen Size
        // 0: 256x256, 1: 512x256, 2: 256x512, 3: 512x512
        
        // Scroll
        u16 hofs = *(u16 *)&io[0x10];
        u16 vofs = *(u16 *)&io[0x12];
        
        int map_base = screen_base_block * 2048; // 2KB steps
        int tile_base = char_base_block * 16384; // 16KB steps
        
        for (int x = 0; x < GBA_SCREEN_WIDTH; x++) {
            int scx = (x + hofs) & 0x1FF; // Mask for 512 (max width)? Depends on size
            int scy = (line + vofs) & 0x1FF;
            
            // Map coordinates
            // Assuming Size 0 (256x256) for simplicity first
            // Tile Map is 32x32 tiles (2 bytes each)
            int map_x = (scx / 8) & 0x1F;
            int map_y = (scy / 8) & 0x1F;
            
            // Read Tile Entry
            // Map is at vram + map_base
            // Offset = (map_y * 32 + map_x) * 2
            int entry_idx = (map_y * 32 + map_x) * 2;
            u16 tile_entry = *(u16 *)&vram[map_base + entry_idx];
            
            int tile_idx = tile_entry & 0x3FF;
            int h_flip = (tile_entry >> 10) & 1;
            int v_flip = (tile_entry >> 11) & 1;
            int pal_bank = (tile_entry >> 12) & 0xF;
            
            // Tile Pixel
            int tile_pixel_x = scx % 8;
            int tile_pixel_y = scy % 8;
            
            if (h_flip) tile_pixel_x = 7 - tile_pixel_x;
            if (v_flip) tile_pixel_y = 7 - tile_pixel_y;
            
            // Read Pixel Data
            // 4bpp (16 colors): 32 bytes per tile (8x8x0.5)
            // 8bpp (256 colors): 64 bytes per tile
            
            u8 color_idx = 0;
            if (color_mode == 0) { // 4bpp
               int offset = tile_base + (tile_idx * 32) + (tile_pixel_y * 4) + (tile_pixel_x / 2);
               u8 byte = vram[offset];
               if (tile_pixel_x & 1) color_idx = byte >> 4;
               else color_idx = byte & 0xF;
            } else { // 8bpp
               int offset = tile_base + (tile_idx * 64) + (tile_pixel_y * 8) + tile_pixel_x;
               color_idx = vram[offset];
            }
            
            if (color_idx != 0) { // Entry 0 is transparent
                // Fetch color from palette
                u16 color = 0;
                if (color_mode == 0) {
                    color = ppu_read_palette(pal_bank * 16 + color_idx);
                } else {
                    color = ppu_read_palette(color_idx);
                }
                
                // Convert 15-bit to 32-bit ARGB
                u8 r = (color & 0x1F) << 3;
                u8 g = ((color >> 5) & 0x1F) << 3;
                u8 b = ((color >> 10) & 0x1F) << 3;
                scanline_buffer[x] = (255 << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
}

void ppu_update_texture(SDL_Texture *texture) {
#ifdef USE_SDL
  u16 *vram = (u16 *)memory_get_vram();
  u8 *io = memory_get_io();
  u16 *pal = (u16 *)memory_get_pal();

  // Read DISPCNT (0x04000000). Bit 0-2 is Video Mode.
  u16 dispcnt = *(u16 *)&io[0];
  u8 mode = dispcnt & 7;

  void *pixels;
  int pitch;
  
  if (!texture) return; // Headless guard
  
  if (SDL_LockTexture(texture, NULL, &pixels, &pitch) == 0) {
    u32 *dst = (u32 *)pixels;

    if (mode == 0) {
        // Line by Line (Headless/PPU Test style, but doing full frame here)
        // Note: Real PPU renders line by line during HBlank.
        // We'll emulate "drawing whole frame" here for the texture update.
        for (int y=0; y<GBA_SCREEN_HEIGHT; y++) {
             ppu_render_scanline_mode0(&dst[y * GBA_SCREEN_WIDTH], y);
        }
    }
    else if (mode == 3) {
      // Mode 3: 240x160 15-bit Bitmap
      for (int i = 0; i < GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT; i++) {
        u16 color = vram[i];
        u8 r = (color & 0x1F) << 3;
        u8 g = ((color >> 5) & 0x1F) << 3;
        u8 b = ((color >> 10) & 0x1F) << 3;
        dst[i] = (255 << 24) | (r << 16) | (g << 8) | b;
      }
    } else if (mode == 4) {
      // Mode 4...
      // (Existing logic preserved)
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
        // Black
      for (int i = 0; i < GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT; i++) {
        dst[i] = 0xFF000000;
      }
    }
    SDL_UnlockTexture(texture);
  }
#endif
}
