#include <stdio.h>

// Simple GBA ROM Generator
// Generates a minimal ROM that sets Mode 3 and fills the screen with RED.

#include <stdio.h>

// ARM Code for Red Screen Fill
unsigned char arm_code[] = {
    // 1. Setup DISPCNT (0x04000000) = 0x0403 (Mode 3, BG2)
    // MOV R0, #0x04000000
    // (Value 1, ROR 6 -> 1 << 26 = 0x4000000? No. 1 ROR 8 = 01000000? No.)
    // 0x04000000 = 1 << 26?
    // 0x4000000  = 1 << 26.
    // 0x04000000 = 1 << 26.
    // 1 ROR 6 = (1 << 26). (32-6=26).
    // Opcode: E3A00301 (Rot=3 -> 6).
    0x01, 0x03, 0xA0, 0xE3, 

    // MOV R1, #0x403
    // MOV R1, #0x400
    // 4 ROR 24 = 4 << 8 = 0x400. Rot=12 (0xC).
    0x04, 0x1C, 0xA0, 0xE3,
    // ORR R1, R1, #3
    0x03, 0x10, 0x81, 0xE3,

    // STR R1, [R0]
    0x00, 0x10, 0x80, 0xE5,

    // 2. Setup VRAM Fill
    // MOV R0, #0x06000000
    // 6 ROR 8 = 6 << 24. Rot=4.
    0x06, 0x04, 0xA0, 0xE3,

    // MOV R2, #0x001F (Red)
    0x1F, 0x20, 0xA0, 0xE3,
    // ORR R2, R2, R2, LSL #16 (Make it 0x001F001F)
    0x02, 0x28, 0x82, 0xE1,

    // LOOP:
    // STR R2, [R0], #4 (Post-increment)
    0x04, 0x20, 0x80, 0xE4,
    // B LOOP
    // Offset calculation:
    // PC is here + 8.
    // Loop is at STR (-4 bytes).
    // Target = PC + 8 + (off*4).
    // -4 = +8 + off*4.
    // -12 = off*4 -> off = -3. (0xFFFFFD)
    0xFD, 0xFF, 0xFF, 0xEA
};

int main() {
  FILE *f = fopen("test.gba", "wb");
  if (!f) return 1;

  fwrite(arm_code, sizeof(arm_code), 1, f);
  for (int i = 0; i < 4096; i++) fputc(0, f); 
  fclose(f);
  printf("test.gba (ARM Red) created.\n");
  return 0;
}
