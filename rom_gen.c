#include <stdio.h>

// Simple GBA ROM Generator
// Generates a minimal ROM that sets Mode 3 and fills the screen with RED.

#include <stdio.h>

// ARM Code for Mode 0 (Tiled) Green Screen
unsigned char arm_code[] = {
    // 1. Setup DISPCNT (0x04000000) = 0x0100 (Mode 0, BG0)
    // MOV R0, #0x04000000
    0x01, 0x03, 0xA0, 0xE3, 

    // MOV R1, #0x100 (BG0)
    // 1 ROR 24 = 1 << 8 = 0x100
    0x01, 0x1C, 0xA0, 0xE3,
    // STR R1, [R0]
    0x00, 0x10, 0x80, 0xE5,
    
    // 2. Setup BG0CNT (0x04000008) = 0x0000 
    // (Size 0, Colors 16/16, Pri 0, CharBase 0, ScreenBase 0)
    // Default 0 is fine.
    // STRH R1(0), [R0, #8] -> No, R1 is 0x100.
    // MOV R1, #0
    0x00, 0x10, 0xA0, 0xE3,
    // STRH R1, [R0, #8] ... ARM STRH is complex. Use STR (will write 0 to 8-11).
    // STR R1, [R0, #8]
    0x08, 0x10, 0x80, 0xE5,
    
    // 3. Setup Palette (0x05000000)
    // MOV R0, #0x05000000
    0x05, 0x04, 0xA0, 0xE3,
    // MOV R2, #0x03E0 (Green 00000 11111 00000)
    // 0x03E0 = 0x1F << 5.
    // MOV R2, #0x1F
    0x1F, 0x20, 0xA0, 0xE3,
    // MOV R2, R2, LSL #5
    0x82, 0x22, 0xA0, 0xE1,
    
    // Color 0 is transparent. We need Color 1.
    // STRH R2, [R0, #2] (Palette index 1)
    // Store 32-bit to 0x05000000 (Color 0 and 1).
    // R2 = 0000 03E0. 
    // R3 = (R2 << 16) | R2 -> 03E0 03E0 ? No.
    // We want Index 1 to be Green. Index 0 doesn't matter (backdrop).
    // Combine: 0x03E003E0
    0x02, 0x38, 0x82, 0xE1, // ORR R3, R2, R2 LSL 16
    // STR R3, [R0]
    0x00, 0x30, 0x80, 0xE5,
    
    // 4. Setup Tile Data (Char Base 0 -> 0x06000000)
    // We need a tile with pixels = 1.
    // MOV R0, #0x06000000
    0x06, 0x04, 0xA0, 0xE3,
    // Fill first tile (32 bytes) with 0x11111111 (Index 1)
    // MOV R2, #0x11
    0x11, 0x20, 0xA0, 0xE3,
    // ORR R2, R2, R2 LSL 8 (0x1111)
    0x02, 0x24, 0x82, 0xE1,
    // ORR R2, R2, R2 LSL 16 (0x11111111)
    0x02, 0x28, 0x82, 0xE1,
    
    // Write 32 bytes (8 words)
    // STR R2, [R0], #4
    0x04, 0x20, 0x80, 0xE4,
    0x04, 0x20, 0x80, 0xE4,
    0x04, 0x20, 0x80, 0xE4,
    0x04, 0x20, 0x80, 0xE4,
    0x04, 0x20, 0x80, 0xE4,
    0x04, 0x20, 0x80, 0xE4,
    0x04, 0x20, 0x80, 0xE4,
    0x04, 0x20, 0x80, 0xE4,
    
    // 5. Setup Tile Map (Screen Base 0 -> 0x06000000 + ? No, SB is bits 8-12 of CNT)
    // We set BG0CNT to 0 -> Screen Base 0.
    // But Char Base is also 0. They overlap!
    // CharBase 0 = 0x06000000.
    // ScreenBase 0 = 0x06000000.
    // If we write map at 0, we overwrite tiles.
    // Let's set ScreenBase to 1 (0x06000800).
    
    // Re-Setup BG0CNT
    // MOV R0, #0x04000000
    0x01, 0x03, 0xA0, 0xE3,
    // MOV R1, #0x100 (ScreenBase 1 << 8)
    // 1 ROR 24 = 1 << 8 = 0x100
    0x01, 0x1C, 0xA0, 0xE3,
    // STR R1, [R0, #8]
    0x08, 0x10, 0x80, 0xE5,
    
    // ScreenBase 1 starts at 0x06000800.
    // MOV R0, #0x06000000
    0x06, 0x04, 0xA0, 0xE3,
    // ADD R0, R0, #0x800
    // 0x800 = 8 << 8 = 8 ROR 24?
    // 8 ROR 20? 0xE3A00B02?
    // ADD R0, R0, #0x800 (Imm 2048) -> Need rotator.
    // 2 (0x02) ROR 22 (0xB) = 2 << 10 = 2048.
    0x02, 0x0B, 0x80, 0xE2,
    
    // Fill Map with Tile 0 (0x0000)
    // Actually we want Tile 0 to be visible properly?
    // We defined Tile 0 above.
    // Map entries: TileIndex (0) | Pal (0)... = 0x0000.
    // Wait, if map is 0, it uses Tile 0.
    // Tile 0 is filled with 1s.
    // So it should show pixels of Color 1 (Green).
    // Fill entire map with 0.
    // MOV R2, #0
    0x00, 0x20, 0xA0, 0xE3,
    
    // LOOP Fill
    // STR R2, [R0], #4
    0x04, 0x20, 0x80, 0xE4,
    // B LOOP
    0xFD, 0xFF, 0xFF, 0xEA
};

int main() {
  FILE *f = fopen("test_mode0.gba", "wb");
  if (!f) return 1;

  fwrite(arm_code, sizeof(arm_code), 1, f);
  for (int i = 0; i < 4096; i++) fputc(0, f); 
  fclose(f);
  printf("test_mode0.gba (Mode 0 Green) created.\n");
  return 0;
}
