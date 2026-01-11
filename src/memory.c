#include "../include/memory.h"
#include <stdio.h>

#include <string.h>

// Memory map placeholders
static u8 bios[0x4000];
static u8 wram_on_board[0x40000];
static u8 wram_on_chip[0x8000];

void memory_init(void) {
  memset(bios, 0, sizeof(bios));
  memset(wram_on_board, 0, sizeof(wram_on_board));
  memset(wram_on_chip, 0, sizeof(wram_on_chip));
  printf("Memory System Initialized.\n");
}

u8 mmu_read8(u32 addr) {
  // Stub implementation
  return 0;
}

u16 mmu_read16(u32 addr) { return 0; }

u32 mmu_read32(u32 addr) { return 0; }

void mmu_write8(u32 addr, u8 value) {
  // Stub
}

void mmu_write16(u32 addr, u16 value) {
  // Stub
}

void mmu_write32(u32 addr, u32 value) {
  // Stub
}
