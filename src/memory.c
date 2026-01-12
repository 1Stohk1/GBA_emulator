#include "../include/memory.h"
#include <stdio.h>

#include <string.h>

#include <stdlib.h> // For malloc

// Memory map placeholders
static u8 bios[0x4000];
static u8 wram_on_board[0x40000];
static u8 wram_on_chip[0x8000];
static u8 io_regs[0x400];
static u8 pal_ram[0x400];
static u8 vram[0x18000]; // 96KB VRAM
static u8 oam[0x400];    // 1KB OAM
// static u32 dummy_rom[1024]; // 4KB dummy ROM - Replacing with real ROM buffer
u8 *rom_memory = NULL;

// Forward Declaration
void check_dma(int channel, u16 control_val);

void memory_init(void) {
  memset(bios, 0, sizeof(bios));
  memset(wram_on_board, 0, sizeof(wram_on_board));
  memset(wram_on_chip, 0, sizeof(wram_on_chip));
  memset(wram_on_chip, 0, sizeof(wram_on_chip));
  memset(io_regs, 0, sizeof(io_regs));
  // Initialize KEYINPUT to 0x03FF (All Released)
  *(u16 *)&io_regs[0x130] = 0x03FF;

  memset(pal_ram, 0, sizeof(pal_ram));
  memset(vram, 0, sizeof(vram));
  memset(oam, 0, sizeof(oam));
  printf("Memory System Initialized.\n");
}

bool memory_load_rom(const char *filename) {
  FILE *f = fopen(filename, "rb");
  if (!f) {
    printf("Failed to open ROM: %s\n", filename);
    return false;
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  // Allocazione dinamica (fino a 32MB max per GBA)
  rom_memory = (u8 *)malloc(size);
  if (!rom_memory) {
    fclose(f);
    return false;
  }

  fread(rom_memory, 1, size, f);
  fclose(f);
  printf("ROM Loaded: %ld bytes\n", size);
  return true;
}

u8 mmu_read8(u32 addr) {
  // Stub implementation
  return 0;
}

u16 mmu_read16(u32 addr) { return 0; }

u32 bus_read32(u32 addr) {
  // BIOS
  if (addr < 0x00004000) {
    return *(u32 *)&bios[addr];
  }
  // EWRAM (0x02000000 - 0x0203FFFF)
  if (addr >= 0x02000000 && addr <= 0x0203FFFF) {
    return *(u32 *)&wram_on_board[addr - 0x02000000];
  }
  // IWRAM (0x03000000 - 0x03007FFF)
  if (addr >= 0x03000000 && addr <= 0x03007FFF) {
    return *(u32 *)&wram_on_chip[addr - 0x03000000];
  }
  // IO
  if (addr >= 0x04000000 && addr <= 0x040003FF) {
    if (addr == 0x04000130) { // KEYINPUT
      return *(u16 *)&io_regs[0x130];
    }
    return *(u32 *)&io_regs[addr - 0x04000000];
  }
  // Palette
  if (addr >= 0x05000000 && addr <= 0x050003FF) {
    return *(u32 *)&pal_ram[addr - 0x05000000];
  }
  // VRAM (0x06000000 - 0x06017FFF)
  if (addr >= 0x06000000 && addr <= 0x06017FFF) {
     return *(u32 *)&vram[addr - 0x06000000];
  }
  // ROM
  if (addr >= 0x08000000 && addr <= 0x09FFFFFF) {
    if (rom_memory) {
      u32 offset = addr - 0x08000000;
      return *(u32 *)&rom_memory[offset];
    }
    return 0;
  }
  return 0;
}

u16 bus_read16(u32 addr) {
  // BIOS
  if (addr < 0x00004000) return *(u16 *)&bios[addr];
  // EWRAM
  if (addr >= 0x02000000 && addr <= 0x0203FFFF) return *(u16 *)&wram_on_board[addr - 0x02000000];
  // IWRAM
  if (addr >= 0x03000000 && addr <= 0x03007FFF) return *(u16 *)&wram_on_chip[addr - 0x03000000];

  if (addr >= 0x08000000 && addr <= 0x09FFFFFF) {
    if (rom_memory) {
      u32 offset = addr - 0x08000000;
      return *(u16 *)&rom_memory[offset];
    }
    return 0;
  }
  if (addr >= 0x04000000 && addr <= 0x040003FF) {
    if (addr == 0x04000130) { // KEYINPUT
      u16 val = *(u16 *)&io_regs[0x130];
      // printf("Reading KEYS. Value: %04X\n", val);
      return val;
    }
    return *(u16 *)&io_regs[addr - 0x04000000];
  }
  if (addr >= 0x05000000 && addr <= 0x050003FF) {
    return *(u16 *)&pal_ram[addr - 0x05000000];
  }
  if (addr >= 0x06000000 && addr <= 0x06017FFF) {
    return *(u16 *)&vram[addr - 0x06000000];
  }
  return 0;
}

u8 bus_read8(u32 addr) {
  if (addr < 0x00004000) return bios[addr];
  if (addr >= 0x02000000 && addr <= 0x0203FFFF) return wram_on_board[addr - 0x02000000];
  if (addr >= 0x03000000 && addr <= 0x03007FFF) return wram_on_chip[addr - 0x03000000];

  if (addr >= 0x08000000 && addr <= 0x09FFFFFF) {
    if (rom_memory) {
      return rom_memory[addr - 0x08000000];
    }
  }
  if (addr >= 0x04000000 && addr <= 0x040003FF) {
    return io_regs[addr - 0x04000000];
  }
  if (addr >= 0x05000000 && addr <= 0x050003FF) {
    return pal_ram[addr - 0x05000000];
  }
  if (addr >= 0x06000000 && addr <= 0x06017FFF) {
    return vram[addr - 0x06000000];
  }
  return 0;
}

void bus_write32(u32 addr, u32 value) {
  if (addr >= 0x02000000 && addr <= 0x0203FFFF) {
    *(u32 *)&wram_on_board[addr - 0x02000000] = value;
    return;
  }
  if (addr >= 0x03000000 && addr <= 0x03007FFF) {
    *(u32 *)&wram_on_chip[addr - 0x03000000] = value;
    return;
  }
  if (addr >= 0x04000000 && addr <= 0x040003FF) {
      *(u32 *)&io_regs[addr - 0x04000000] = value;
      return;
  }
  if (addr >= 0x05000000 && addr <= 0x050003FF) {
      *(u32 *)&pal_ram[addr - 0x05000000] = value;
      return;
  }
  if (addr >= 0x06000000 && addr <= 0x06017FFF) {
      *(u32 *)&vram[addr - 0x06000000] = value;
      return;
  }
  // printf("[BUS] Write32: [%08X] = %08X\n", addr, value);
}

void bus_write8(u32 addr, u8 value) {
  if (addr >= 0x02000000 && addr <= 0x0203FFFF) {
    wram_on_board[addr - 0x02000000] = value;
    return;
  }
  if (addr >= 0x03000000 && addr <= 0x03007FFF) {
    wram_on_chip[addr - 0x03000000] = value;
    return;
  }
   if (addr >= 0x04000000 && addr <= 0x040003FF) {
    io_regs[addr - 0x04000000] = value;
    return;
  }
  if (addr >= 0x05000000 && addr <= 0x050003FF) {
    pal_ram[addr - 0x05000000] = value;
    return;
  }
  if (addr >= 0x06000000 && addr <= 0x06017FFF) {
    vram[addr - 0x06000000] = value;
    return;
  }
  // printf("[BUS] Write8: [%08X] = %02X\n", addr, value);
}

void bus_write16(u32 addr, u16 value) {
  if (addr >= 0x02000000 && addr <= 0x0203FFFF) {
      *(u16 *)&wram_on_board[addr - 0x02000000] = value;
      return;
  }
  if (addr >= 0x03000000 && addr <= 0x03007FFF) {
      *(u16 *)&wram_on_chip[addr - 0x03000000] = value;
      return;
  }
  if (addr >= 0x06000000 && addr <= 0x06017FFF) {
    u32 offset = addr - 0x06000000;
    *(u16 *)&vram[offset] = value;
    return;
  }
  if (addr >= 0x04000000 && addr <= 0x040003FF) {
    if (addr == 0x04000000) {
        printf("[IO] DISPCNT Write: %04X (Mode %d)\n", value, value & 7);
    }
    *(u16 *)&io_regs[addr - 0x04000000] = value;
    
    // Check for DMA Control Write (Offset 0xBA, 0xC6, 0xD2, 0xDE)
    // DMA0CNT_H = 0xBA
    // DMA1CNT_H = 0xC6
    // DMA2CNT_H = 0xD2
    // DMA3CNT_H = 0xDE
    u32 offset = addr - 0x04000000;
    if (offset == 0xBA) check_dma(0, value);
    else if (offset == 0xC6) check_dma(1, value);
    else if (offset == 0xD2) check_dma(2, value);
    else if (offset == 0xDE) check_dma(3, value);
    
    return;
  }
  if (addr >= 0x05000000 && addr <= 0x050003FF) {
    *(u16 *)&pal_ram[addr - 0x05000000] = value;
    return;
  }
  if (addr >= 0x07000000 && addr <= 0x070003FF) {
    *(u16 *)&oam[addr - 0x07000000] = value;
    return;
  }
  // printf("[BUS] Write16: [%08X] = %04X\n", addr, value);
}

// Helpers
u8 *memory_get_vram(void) { return vram; }
u8 *memory_get_io(void) { return io_regs; }
u8 *memory_get_pal(void) { return pal_ram; }

// Input Helper
void memory_set_key_state(u16 key_mask) {
  // key_mask: 0=Pressed, 1=Released (Standard GBA)
  // We just write directly to 0x130
  *(u16 *)&io_regs[0x130] = key_mask;
}

void mmu_write8(u32 addr, u8 value) {
  // Stub
}

void mmu_write16(u32 addr, u16 value) {
  // Stub
}
u8 *memory_get_oam() { return oam; }

// DMA Helper (Forward declaration or impl)
void check_dma(int channel, u16 control_val) {
    // DMA Registers base: 0x40000B0 + ch*12
    u32 base = 0xB0 + channel * 12;
    u8 *io = memory_get_io();
    
    // Read SAD, DAD, CNT_L from IO
    u32 sad = *(u32 *)&io[base];
    u32 dad = *(u32 *)&io[base + 4];
    u16 cnt_l = *(u16 *)&io[base + 8];
    // control_val is the new value for CNT_H (base + 10)
    
    // If Enable (Bit 15) and Timing=0 (Immediate)
    // Timing bits: 12-13.
    bool enable = (control_val >> 15) & 1;
    int timing = (control_val >> 12) & 3; // 0=Immediate
    
    if (enable && timing == 0) {
        // execute copy
        // Size: Bit 10 (0=16bit, 1=32bit)
        bool is_32 = (control_val >> 10) & 1;
        int count = cnt_l;
        if (count == 0) count = (channel == 3) ? 0x10000 : 0x4000; 
        
        // Addr Ctrl
        int dest_adj = (control_val >> 5) & 3; // 0=Inc, 1=Dec, 2=Fixed, 3=Reload
        int src_adj = (control_val >> 7) & 3;  // 0=Inc, 1=Dec, 2=Fixed
        
        u32 src = sad;
        u32 dst = dad;
        int step = is_32 ? 4 : 2;
        
        //printf("[DMA%d] Transfer: Src=%08X Dst=%08X Count=%X 32bit=%d\n", channel, src, dst, count, is_32);
        
        for (int i=0; i<count; i++) {
            if (is_32) {
                u32 val = bus_read32(src);
                bus_write32(dst, val);
            } else {
                u16 val = bus_read16(src);
                bus_write16(dst, val);
            }
            
            // Update Addr
            if (src_adj == 0) src += step;
            else if (src_adj == 1) src -= step;
            // fixed ignored
            
            if (dest_adj == 0) dst += step;
            else if (dest_adj == 1) dst -= step;
            // fixed ignored
        }
        
        // Clear Enable bit after immediate transfer? 
        // Docs: "Enable bit is NOT cleared automatically for VBlank/HBlank/VidCap modes... 
        // For Immediate mode, transfer starts immediately... bit remains set?"
        // Actually, Immediate mode stops after transfer. User should clear it? 
        // Or emulator clears it to indicate done?
        // "When the transfer is finished, the Enable bit is cleared (unless Repeat is set)."
        if (!((control_val >> 9) & 1)) { // If Repeat not set
            control_val &= ~(1 << 15); // Clear Enable
            *(u16 *)&io[base + 10] = control_val;
        }
    }
}

void mmu_write32(u32 addr, u32 value) {
  // Stub
}
