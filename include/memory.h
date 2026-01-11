#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"

// Initialize memory subsystem
void memory_init(void);

// Memory Access
u8  mmu_read8(u32 addr);
u16 mmu_read16(u32 addr);
u32 mmu_read32(u32 addr);

void mmu_write8(u32 addr, u8 value);
void mmu_write16(u32 addr, u16 value);
void mmu_write32(u32 addr, u32 value);

#endif // MEMORY_H
