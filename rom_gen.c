#include <stdio.h>

// Simple GBA ROM Generator
// Generates a minimal ROM that sets Mode 3 and fills the screen with RED.

unsigned char thumb_code[] = {
    // 1. Setup Mode 3 (0x403) in DISPCNT (0x04000000)
    // R0 = 0x04000000 (IO Base)
    0x04, 0x20, 0x00, 0x06, // MOV R0, #4; LSL R0, R0, #24 -> 0x04000000
    // Actually simpler: 
    // MOV R0, #0x400
    // LSL R0, R0, #16
    
    // Hand-assembled for:
    // MOV R0, #128   (0x80)
    // LSL R0, R0, #19 (0x04000000)
    // But let's stick to simple immediate loads or PC-relative.
    
    // R0 = 0x04000000
    0x80, 0x20, // MOV R0, #128
    0xC0, 0x04, // LSL R0, R0, #19 -> 0x04000000
    
    // R1 = 0x403 (Mode 3 + BG2)
    0x04, 0x21, // MOV R1, #4
    0x08, 0x02, // LSL R1, R1, #8 -> 0x400
    0x03, 0x31, // ADD R1, #3 -> 0x403
    
    // STRH R1, [R0]
    0x01, 0x80, 
    
    // 2. Setup VRAM Pointers
    // R0 = 0x06000000 (Start)
    0x06, 0x20,
    0x00, 0x06, // LSL R0, R0, #24
    
    // R1 = 0x06012C00 (End) (240 * 160 * 2 = 0x12C00 bytes)
    // R1 = R0
    0x01, 0x1C, // MOV R1, R0
    // R2 = 0x12C00
    // 0x9600 * 2 = 0x12C00. 
    // Let's just use a counter or simpler end check.
    // 240*160 = 38400 pixels.
    // R3 = 38400 (0x9600)
    0x96, 0x23, // MOV R3, #150 (approx 0x96) -> No, immediate 8-bit.
    // Load via PC relative? Too complex.
    // Let's just loop forever filling screen.
    
    // R2 = Color RED (0x001F)
    0x1F, 0x22, 
    
    // LOOP:
    // STRH R2, [R0]
    0x02, 0x80,
    // ADD R0, #2
    0x02, 0x30,
    // B LOOP (Offset -4 bytes -> 0xFE)
    0xFE, 0xE7
};

int main() {
  FILE *f = fopen("test.gba", "wb");
  if (!f) return 1;

  fwrite(thumb_code, sizeof(thumb_code), 1, f);

  // Pad to 4KB
  for (int i = 0; i < 4096; i++) fputc(0, f);

  fclose(f);
  printf("test.gba (Red Screen) created.\n");
  return 0;
}
