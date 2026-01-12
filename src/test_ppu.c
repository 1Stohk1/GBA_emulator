#include "../include/ppu.h"
#include "../include/memory.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

void test_ppu_mode0_bg0() {
    printf("Testing PPU Mode 0 BG0...\n");
    memory_init();
    
    u8 *io = memory_get_io();
    u8 *vram = memory_get_vram();
    u8 *pal = memory_get_pal();
    
    // 1. Setup DISPCNT (0x00)
    // Mode 0 (0-2 = 000), BG0 Enable (Bit 8 = 1)
    // 0000 0001 0000 0000 -> 0x0100
    *(u16 *)&io[0x00] = 0x0100;
    
    // 2. Setup BG0CNT (0x08)
    // CBB=0, SBB=31 (Top of VRAM 0x3800 for map), 4bpp, Size=0 (256x256)
    // SBB = 31 (0x1F) << 8 = 0x1F00
    *(u16 *)&io[0x08] = 0x1F00;
    
    // 3. Setup Palette
    // Color 1: Red (0x001F)
    *(u16 *)&pal[2] = 0x001F; // Index 1
    
    // 4. Setup Tile Data (Char Base Block 0)
    // Tile 1: All pixels color 1
    // 4bpp: 32 bytes. Each byte = 0x11 (Pixels 0,1 are color 1)
    for (int i=0; i<32; i++) {
        vram[32 + i] = 0x11; // Tile Index 1 starts at offset 32 (Tile 0 is offset 0)
    }
    
    // 5. Setup Tile Map (Screen Base Block 31 -> 0x1F * 2048 = 0xF800)
    u16 *map = (u16 *)&vram[0xF800];
    // Map entry 0 (Top-left): Tile Index 1
    map[0] = 1;
    
    // 6. Render Scanline 0
    u32 buffer[GBA_SCREEN_WIDTH];
    ppu_render_scanline_mode0(buffer, 0);
    
    // Verify Pixel 0 is Red
    // ARGB: Red 0x1F -> R=255, G=0, B=0
    u32 pixel = buffer[0];
    u8 r = (pixel >> 16) & 0xFF;
    u8 g = (pixel >> 8) & 0xFF;
    u8 b = pixel & 0xFF;
    
    printf("Pixel 0: %08X (R=%d, G=%d, B=%d)\n", pixel, r, g, b);
    
    if (r == 248 && g == 0 && b == 0) { // 31 << 3 = 248
         printf("PASS: Pixel 0 is Red\n");
    } else {
         printf("FAIL: Expected Red (00 or F8), got %08X\n", pixel);
    }
    
    // Verify Pixel 8 (Tile 2, should be empty/transparent 0)
    u32 pixel8 = buffer[8];
    if ((pixel8 & 0xFFFFFF) == 0) {
        printf("PASS: Pixel 8 is Transparent/Black\n");
    } else {
        printf("FAIL: Pixel 8 expected 0, got %X\n", pixel8);
    }
}

int main() {
    test_ppu_mode0_bg0();
    return 0;
}
