#include <stdio.h>

// Codice Assembly Thumb pre-compilato:
// 1. Setup VRAM address (0x06000000)
// 2. Setup Red Color (0x001F)
// 3. Write Color to VRAM
// 4. Infinite Loop
// Interactive Test Code
// Loop:
// 1. Read VCOUNT (0x04000006)
// 2. Check if VCOUNT > 100
// 3. If VCOUNT > 100 -> Fill Green
// 4. Else -> Fill Red
// 5. Repeat
// Assembly Thumb equivalente alla logica sopra
unsigned char thumb_code[] = {
    // R0 = VRAM Address (0x06000000)
    0x06, 0x20, 0x00, 0x06,
    // R1 = IO Registers Base (0x04000000)
    0x04, 0x21, 0x09, 0x06,

    // NEW: Set DISPCNT (0x04000000) to 0x0403 (Mode 3, BG2 Enable)
    // MOV R2, #3
    0x03, 0x22,
    // MOV R3, #4
    0x04, 0x23,
    // LSL R3, R3, #8 (R3 = 0x400)
    0xDB, 0x02,
    // ORR R2, R3 (R2 = 0x403)
    0x1A, 0x43,
    // STRH R2, [R1] (Offset 0 from 0x04000000)
    0x0A, 0x80,

    // LABEL_LOOP:
    // R1 points to 0x04000000. VCOUNT is at offset 0x006.

    // LABEL_READ_VCOUNT:
    // ADD R1, #6 -> R1 = 0x04000006 (VCOUNT address)
    0x06, 0x31, // ADD R1, #6
    0x0A, 0x88, // LDRH R2, [R1] (Read VCOUNT)
    0x06, 0x39, // SUB R1, #6 -> Restore R1 to 0x04000000

    // Check VCOUNT > 100
    // CMP R2, #100
    0x64, 0x2B, // CMP R2, #100
    0x01, 0xD3, // BCC Label_Red (If Carry Clear -> R2 < 100) - Unsigned Lower
    // Wait. CMP R2, 100.
    // If R2 >= 100, C is Set.
    // If R2 < 100, C is Clear.
    // So BCC (Branch if Carry Clear) goes to Red logic if VCOUNT < 100.

    // Label_Green Check: if VCOUNT > 100 -> Green
    // (Actually >= 100)
    // MOV R2, #0x03E0 (Green)
    0xE0, 0x22, 0x01, 0xE0, // B Label_Draw

    // Label_Red:
    0x1F, 0x22, // MOV R2, #0x001F (Red) (Also for < 100)

    // Label_Draw:
    0x02, 0x80, // STRH R2, [R0] (Write to VRAM 0x06000000)

    // Jump Back to Start
    // We are approximately 14 bytes in loop?
    // Instructions:
    // ADD(2) + LDRH(2) + SUB(2) + CMP(2) + BCC(2) + MOV(2) + B(2) + MOV(2) +
    // STRH(2) = 18 bytes.
    // Target is start (ADD).
    // Current PC is after STRH.
    // Offset -18 bytes -> -9 dec -> 0xF7?
    // Target = PC + 4 + (offset * 2).
    // PC = Current.
    // Target = Current - 18? (Roughly)
    // Current instruction is B. PC is (address of B) + 4.
    // Target is (address of ADD R1, #6).
    // (address of B) + 4 + (offset * 2) = (address of ADD R1, #6)
    // Let's count bytes from ADD R1, #6 to B instruction:
    // ADD(2) + LDRH(2) + SUB(2) + CMP(2) + BCC(2) + MOV(2) + B(2) + MOV(2) +
    // STRH(2) = 18 bytes.
    // So, B is at offset 16 from ADD.
    // (address of ADD) + 16 + 4 + (offset * 2) = (address of ADD)
    // 20 + (offset * 2) = 0
    // offset * 2 = -20
    // offset = -10.
    // -10 in 11-bit signed is 0b11111110110 = 0x7F6.
    // B instruction format: 11101 (offset_11).
    // So 0xF6, 0xE7.
    0xF6, 0xE7, // B LOOP_START (Back to Reading VCOUNT)

    // Padding
    0x00, 0x00};

int main() {
  FILE *f = fopen("test.gba", "wb");
  if (!f)
    return 1;

  // Scriviamo il codice all'inizio del file (simuliamo entry point diretto)
  fwrite(thumb_code, sizeof(thumb_code), 1, f);

  // Riempiamo un po' il file per sicurezza
  for (int i = 0; i < 100; i++)
    fputc(0, f);

  fclose(f);
  printf("test.gba generato con successo!\n");
  return 0;
}
