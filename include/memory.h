#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"

// Initialize memory subsystem
void memory_init(void);

// Memory Access
u8 mmu_read8(u32 addr);
u16 mmu_read16(u32 addr);
extern u8 *rom_memory; // Buffer globale per la ROM
bool memory_load_rom(const char *filename);

u8 bus_read8(u32 addr);
u16 bus_read16(u32 addr);
u32 bus_read32(u32 addr);
void bus_write8(u32 addr, u8 value);
void bus_write16(u32 addr, u16 value);
void bus_write32(u32 addr, u32 value);

u8 *memory_get_vram(void);
u8 *memory_get_io(void);
u8 *memory_get_pal(void);
u8 *memory_get_oam(void);
void memory_set_key_state(u16 key_mask);

void mmu_write8(u32 addr, u8 value);
void mmu_write16(u32 addr, u16 value);
void mmu_write32(u32 addr, u32 value);

#endif // MEMORY_H
