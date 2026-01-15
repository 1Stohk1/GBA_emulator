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
    // Update Accumulator
    cycle_bucket += cycles;
    
    // Define Constants
    const int CYCLES_PER_LINE = 1232;
    const int CYCLES_HDRAW = 960;
    
    // Check DISPSTAT Status *Current*
    u8 *io = memory_get_io();
    u16 old_stat = *(u16 *)&io[4]; 
    u16 new_stat = old_stat;
    
    // HBlank Status (Bit 1)
    // 0..960 = Drawing (Flag 0), 960..1232 = HBlank (Flag 1)
    if (cycle_bucket >= CYCLES_HDRAW) {
        if (!(old_stat & 2)) { // Rising Edge HBlank
           new_stat |= 2;
           if (new_stat & 0x10) *(u16 *)&io[0x202] |= 2; // IRQ
        }
    } else {
        new_stat &= ~2; // Clear HBlank
    }
    
    while (cycle_bucket >= CYCLES_PER_LINE) {
        cycle_bucket -= CYCLES_PER_LINE;
        vcount++;
        if (vcount > 227) vcount = 0;
        
        *(u16 *)&io[6] = vcount; // Update VCount IO
        
        // Check VBlank Transition (Line 160)
        if (vcount == 160) {
            new_stat |= 1; // Set VBlank
            if (new_stat & 0x08) *(u16 *)&io[0x202] |= 1; // IRQ
            memory_check_dma_vblank();
        } 
        else if (vcount == 0) { // End of VBlank
             new_stat &= ~1; // Clear VBlank
        }
        
        // V-Counter Match (Bit 2)
        u8 vcount_setting = (new_stat >> 8) & 0xFF;
        if (vcount == vcount_setting) {
             new_stat |= 4; // Set Match
             if (new_stat & 0x20) *(u16 *)&io[0x202] |= 4; // IRQ
        } else {
             new_stat &= ~4;
        }
    }
    
    *(u16 *)&io[4] = new_stat;
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

    // Clear buffer (Transparent / Backdrop - usually Pal 0)
    for(int x=0; x<GBA_SCREEN_WIDTH; x++) scanline_buffer[x] = 0; // Black/Transparent
    
    // Render BGs by Priority (3 -> 0)
    for (int prio = 3; prio >= 0; prio--) {
        // Check all 4 BGs
        for (int bg=0; bg<4; bg++) {
            // Check Enable (Bits 8,9,10,11)
            bool enabled = (dispcnt >> (8+bg)) & 1;
            if (!enabled) continue;
            
            u16 bgcnt = *(u16 *)&io[0x08 + bg*2];
            int bg_prio = bgcnt & 3;
            if (bg_prio != prio) continue;
            
            // Render this BG
            int char_base_block = (bgcnt >> 2) & 3;
            int mosaic = (bgcnt >> 6) & 1;
            int color_mode = (bgcnt >> 7) & 1; // 0=16/16, 1=256/1
            int screen_base_block = (bgcnt >> 8) & 0x1F;
            int size = (bgcnt >> 14) & 3;
            
            // Unused params supress
            (void)mosaic;
            (void)size; // TODO: Handle large maps logic (Size 1,2,3)
            // Current simplification: Treat all as Size 0 (256x256)
            
            // Scroll Registers
            u16 hofs = *(u16 *)&io[0x10 + bg*4];
            u16 vofs = *(u16 *)&io[0x12 + bg*4];
            
            int map_base = screen_base_block * 2048; // 2KB steps
            int tile_base = char_base_block * 16384; // 16KB steps
            
            for (int x = 0; x < GBA_SCREEN_WIDTH; x++) {
                int scx = (x + hofs) & 0x1FF; // Wrap 512
                int scy = (line + vofs) & 0x1FF;
                
                // Map coordinates
                int map_x = (scx / 8) & 0x1F;
                int map_y = (scy / 8) & 0x1F;
                
                // Size Logic Override for Demo (Assume Size 0)
                // TODO: Correct Size logic selection for map_addr
                
                // Read Tile Entry
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
                } else {
                     if (line == 0 && x < 4) {
                        printf("L0 X%d: Transparent. Byte=%02X\n", x, (color_mode==0) ? vram[tile_base + (tile_idx * 32) + (tile_pixel_y * 4) + (tile_pixel_x / 2)] : 0);
                     }
                }
            }
        }
    }
}

void ppu_render_oam(u32 *scanline_buffer, int line) {
    u8 *io = memory_get_io();
    u16 dispcnt = *(u16 *)&io[0];
    
    // Check if OBJ (Bit 12) is enabled
    if (!(dispcnt & 0x1000)) return;

    u8 *oam = memory_get_oam();
    u8 *vram = memory_get_vram(); // OBJ Tiles are at 0x06010000 (Offset 0x10000 in VRAM)
    u8 *obj_vram = vram + 0x10000;
    u16 *pal = (u16 *)memory_get_pal(); // OBJ Palette is at 0x05000200 (Offset 0x200 in PAL RAM?)
    // Actually memory_get_pal returns base of 0x05000000. OBJ Pal starts at +0x200.
    u16 *obj_pal = pal + 0x100; // 0x200 bytes / 2 = 0x100 shorts

    // Iterate 128 sprites
    for (int i = 0; i < 128; i++) {
        u16 attr0 = *(u16 *)&oam[i * 8 + 0];
        u16 attr1 = *(u16 *)&oam[i * 8 + 2];
        u16 attr2 = *(u16 *)&oam[i * 8 + 4];

        // Attribute 0: Y Coordinate, Shape, Color Mode, Mosaic, Double Size, Disable
        int y = attr0 & 0xFF;
        int rot_scale_flag = (attr0 >> 8) & 1;
        int double_size = (attr0 >> 9) & 1; // Only if Rot/Scale on
        int mode = (attr0 >> 10) & 3; // 0=Normal, 1=Semi-Trans, 2=Obj Window
        int mosaic = (attr0 >> 12) & 1;
        int color_mode = (attr0 >> 13) & 1; // 0=16/16 (4bpp), 1=256/1 (8bpp)
        int shape = (attr0 >> 14) & 3; // 0=Square, 1=Wide, 2=Tall

        // Attribute 1: X Coordinate, Rot/Scale Param / Flip
        int x = attr1 & 0x1FF;
        int flip_h = 0;
        int flip_v = 0;
        
        if (!rot_scale_flag) {
             flip_h = (attr1 >> 12) & 1;
             flip_v = (attr1 >> 13) & 1;
        }

        // Attribute 2: Tile Index, Priority, Palette Bank
        int tile_index = attr2 & 0x3FF;
        int priority = (attr2 >> 10) & 3;
        int pal_bank = (attr2 >> 12) & 0xF;
        
        (void)priority;
        (void)mosaic;
        (void)double_size;

        // Simplify: Only standard sprites, no rot/scale yet.
        if (mode == 2) continue; // Skip OBJ Window
        if (rot_scale_flag) continue; // Skip Rot/Scale for now (too complex for step 1)

        // Calculate size (Width/Height) based on Shape & Size
        // Size bits in Attr1 (14-15)
        int size = (attr1 >> 14) & 3;
        int width = 8, height = 8;
        
        // Lookup table logic (simplified):
        // Shape 0 (Square): 8, 16, 32, 64
        // Shape 1 (Wide): 16x8, 32x8, 32x16, 64x32
        // Shape 2 (Tall): 8x16, 8x32, 16x32, 32x64
        if (shape == 0) {
            int dim = 8 << size; width = dim; height = dim;
        } else if (shape == 1) {
            if (size==0) { width=16; height=8; }
            else if (size==1) { width=32; height=8; }
            else if (size==2) { width=32; height=16; }
            else { width=64; height=32; }
        } else if (shape == 2) {
            if (size==0) { width=8; height=16; }
            else if (size==1) { width=8; height=32; }
            else if (size==2) { width=16; height=32; }
            else { width=32; height=64; }
        }
        
        // Wrap Y
        if (y >= 160) y -= 256; 
        
        // Check visibility on current line
        if (line >= y && line < (y + height)) {
            // Visible on this line
            int sprite_y = line - y;
            if (flip_v) sprite_y = height - 1 - sprite_y;

            // Render loop logic for row
            for (int sx = 0; sx < width; sx++) {
                int screen_x = x + sx;
                if (screen_x >= 512) screen_x -= 512; // Wrap X? usually 240
                if (screen_x < 0 || screen_x >= GBA_SCREEN_WIDTH) continue;

                int sprite_x = sx;
                if (flip_h) sprite_x = width - 1 - sx;

                // Fetch Pixel
                // 4bpp: 32 bytes per tile. 8x8 pixels per tile.
                // 8bpp: 64 bytes per tile.
                // Tile Mapping: 1D or 2D. Default 1D? Bit 6 of DISPCNT is 1D/2D.
                // Assuming 1D mapping for simplicity.
                int tile_y = sprite_y / 8;
                int tile_x_offset = sprite_x / 8;
                int local_y = sprite_y % 8;
                int local_x = sprite_x % 8;
                
                int current_tile = tile_index; 
                // In 1D mode, tiles are linear.
                // In 4bpp, each "tile increment" is 32 bytes.
                // Width in tiles depends on shape.
                // Stride logic is complex.
                // Let's assume basic linear check:
                // Tile ID = Base + TileY * Stride + TileX
                // For 1D: TileID simply increments.
                
                // Simplified 1D Mapping:
                int stride = width / 8;
                if (color_mode == 0) { // 4bpp
                    current_tile += (tile_y * stride) + tile_x_offset; 
                } else { // 8bpp
                   current_tile += ((tile_y * stride) + tile_x_offset) * 2;
                }

                u32 tile_addr = current_tile * 32;
                if (color_mode == 0) { // 4bpp
                     u8 input_byte = obj_vram[tile_addr + (local_y * 4) + (local_x / 2)];
                     u8 index = (local_x & 1) ? (input_byte >> 4) : (input_byte & 0xF);
                     if (index != 0) { // Transparent
                         u16 color = obj_pal[pal_bank * 16 + index];
                         u8 r = (color & 0x1F) << 3;
                         u8 g = ((color >> 5) & 0x1F) << 3;
                         u8 b = ((color >> 10) & 0x1F) << 3;
                         scanline_buffer[screen_x] = (255 << 24) | (r << 16) | (g << 8) | b;
                     }
                }
                // 8bpp skipped for brevity
            }
        }
    }
}

// Previous ppu_update_texture ...

// Internal Framebuffer for Headless/Screenshot
static u32 internal_framebuffer[GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT];

void ppu_save_screenshot(const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        printf("Failed to create screenshot: %s\n", filename);
        return;
    }
    // P6 = Binary PPM, 240x160, 255 max val
    fprintf(f, "P6\n%d %d\n255\n", GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT);
    
    for (int i = 0; i < GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT; i++) {
        u32 color = internal_framebuffer[i];
        // Saved as ARGB8888 in buffer (from our render logic)
        // PPM needs RGB
        u8 r = (color >> 16) & 0xFF;
        u8 g = (color >> 8) & 0xFF;
        u8 b = color & 0xFF;
        fputc(r, f);
        fputc(g, f);
        fputc(b, f);
    }
    fclose(f);
    printf("Screenshot saved to %s\n", filename);
}

void ppu_update_texture(SDL_Texture *texture) {
  u16 *vram = (u16 *)memory_get_vram();
  u8 *io = memory_get_io();
  u16 *pal = (u16 *)memory_get_pal();

  // Read DISPCNT (0x04000000). Bit 0-2 is Video Mode.
  u16 dispcnt = *(u16 *)&io[0];
  u8 mode = dispcnt & 7;

  // Determine destination buffer
  u32 *dst = internal_framebuffer;
#ifdef USE_SDL
  void *pixels = NULL;
  int pitch = 0;
  if (texture) {
      if (SDL_LockTexture(texture, NULL, &pixels, &pitch) < 0) return;
      dst = (u32 *)pixels;
  }
#endif

  // Render Frame
    if (mode == 0) {
        for (int y=0; y<GBA_SCREEN_HEIGHT; y++) {
             ppu_render_scanline_mode0(&dst[y * GBA_SCREEN_WIDTH], y);
             ppu_render_oam(&dst[y * GBA_SCREEN_WIDTH], y); 
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
      u8 *page_ptr = (u8 *)vram;
      if (dispcnt & 0x10) page_ptr += 0xA000;

      for (int i = 0; i < GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT; i++) {
        u8 index = page_ptr[i];
        u16 color = pal[index]; 
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
  
#ifdef USE_SDL
  if (texture) SDL_UnlockTexture(texture);
#endif
}
