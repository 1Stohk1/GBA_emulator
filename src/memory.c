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

// Timer State
static u16 timer_counter[4] = {0};
static u16 timer_reload[4] = {0};
static double timer_accumulator[4] = {0}; 

// Helper to get prescaler shift: 0=1, 1=64, 2=256, 3=1024
static int get_prescaler_shift(int setting) {
    switch (setting) {
        case 0: return 0; // x1
        case 1: return 6; // x64
        case 2: return 8; // x256
        case 3: return 10; // x1024
    }
    return 0;
}

void timer_step(int cycles) {
    for (int i=0; i<4; i++) {
        // TMxCNT_H is at offset 0x102 + i*4
        u16 cnt_h = *(u16 *)&io_regs[0x102 + i*4];
        
        bool start = (cnt_h >> 7) & 1;
        if (!start) continue;
        
        // Cascade not implemented yet
        bool cascade = (cnt_h >> 2) & 1;
        if (cascade) continue; 
        
        int prescaler_setting = cnt_h & 3;
        int shift = get_prescaler_shift(prescaler_setting);
        
        timer_accumulator[i] += cycles;
        
        double ticks_needed = (double)(1 << shift);
        while (timer_accumulator[i] >= ticks_needed) {
            timer_accumulator[i] -= ticks_needed;
            timer_counter[i]++;
            
            if (timer_counter[i] == 0) { // Overflow
                timer_counter[i] = timer_reload[i];
                
                // IRQ
                if ((cnt_h >> 6) & 1) {
                    u16 *if_reg = (u16 *)&io_regs[0x202];
                    *if_reg |= (1 << (3 + i));
                }
                
                // Audio Channels often use Timer Overflows (DMA Sound)
                // TODO: Trigger DMA 1/2 sound FIFO if configured?
            }
        }
        
        // Sync to IO regs for readout
        *(u16 *)&io_regs[0x100 + i*4] = timer_counter[i];
    }
}

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
      /*
      // Debug IO Writes
      u32 offset = addr - 0x04000000;
      // Log DISPCNT (0), BGCNT (8, A, C, E)
      if (offset == 0 || offset == 8 || offset == 0xA || offset == 0xC || offset == 0xE) {
          printf("[IO] Write16: [%08X] = %04X\n", addr, value);
      }
      */
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
    
    // Interrupt Control Registers
    u32 offset = addr - 0x04000000;
    if (offset == 0x202) { // IF (Interrupt Request) - Write 1 to acknowledge/clear
        u16 current_if = *(u16 *)&io_regs[0x202];
        *(u16 *)&io_regs[0x202] = current_if & ~value;
        // printf("[IO] IF Write: %04X -> Val %04X (Ack)\n", value, *(u16 *)&io_regs[0x202]);
        return;
    }
    
    // Timer Write Logic
    if (offset >= 0x100 && offset <= 0x10E) {
        int timer_idx = (offset - 0x100) / 4;
        int is_cnt_h = (offset & 2); // 0=L (0, 4..), 2=H (2, 6..)
        
        if (!is_cnt_h) { // CNT_L (Reload Value)
            timer_reload[timer_idx] = value;
        } else { // CNT_H (Control)
            u16 old_val = *(u16 *)&io_regs[offset];
            bool old_start = (old_val >> 7) & 1;
            bool new_start = (value >> 7) & 1;
            
            // If transitioning from Stop to Start, reload counter
            if (!old_start && new_start) {
                timer_counter[timer_idx] = timer_reload[timer_idx];
                // printf("[Timer%d] Started. Reload=%04X\n", timer_idx, timer_reload[timer_idx]);
            }
        }
    }
    
    *(u16 *)&io_regs[offset] = value;
    
    // Check for DMA Control Write (Offset 0xBA, 0xC6, 0xD2, 0xDE)
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
static void perform_dma(int channel) {
    // u32 base = 0x040000B0 + (channel * 12); // Unused
    // Determine IO offset correctly
    // Global io_regs are at base 0 from pointer view?
    // memory_get_io() returns pointer to 0x04000000 base.
    // io_regs used in perform_dma assumes io_regs is a global array? 
    // Wait, perform_dma in my previous paste used `io_regs` which is defined as `static u8 io_regs[0x400]` in memory.c?
    // Let's check definition of io_regs. usually it's `io_regs` or `io`.
    // In check_dma (line 283) it gets `u8 *io = memory_get_io();`
    // My perform_dma code used `io_regs` which might be invalid if not in scope or named differently.
    // Let's use memory_get_io().
    
    u8 *io = memory_get_io();
    u32 offset = 0xB0 + (channel * 12);
    
    u32 sad = *(u32 *)&io[offset];
    u32 dad = *(u32 *)&io[offset + 4];
    u16 cnt_l = *(u16 *)&io[offset + 8];
    u16 control_val = *(u16 *)&io[offset + 10];

    bool is_32 = (control_val >> 10) & 1;
    int count = cnt_l;
    if (count == 0) count = (channel == 3) ? 0x10000 : 0x4000;
    
    int dest_adj = (control_val >> 5) & 3; 
    int src_adj = (control_val >> 7) & 3;
    
    u32 src = sad;
    u32 dst = dad;
    int step = is_32 ? 4 : 2;
    
    for (int i=0; i<count; i++) {
        if (is_32) {
            u32 val = bus_read32(src);
            bus_write32(dst, val);
        } else {
            u16 val = bus_read16(src);
            bus_write16(dst, val);
        }
        
        if (src_adj == 0) src += step;
        else if (src_adj == 1) src -= step;
        
        if (dest_adj == 0) dst += step;
        else if (dest_adj == 1) dst -= step;
    }
    
    if (!((control_val >> 9) & 1)) { // Repeat Bit 9
        u16 new_ctrl = control_val & ~(1 << 15);
        *(u16 *)&io[offset + 10] = new_ctrl;
    } else {
        *(u32 *)&io[offset] = src;
        *(u32 *)&io[offset + 4] = dst;
    }
}

void check_dma(int channel, u16 control_val) {
    // This signature matches existing call sites?
    // Wait, original signature was `void check_dma(int channel, u16 control_val)`?
    // Line 280 in previous View: `void check_dma(int channel, u16 control_val)`
    // My previous replacement replaced `check_dma` with `static void perform_dma` inside it? No.
    // I need to restore check_dma signature if it's called with 2 args.
    // grep search for check_dma call sites?
    // It's called from `bus_write16`.
    
    // u16 control_val is passed as argument.
    // We can use it directly.
    
    // Debug print
    // printf("DMA Check Ch%d Ctrl=%04X\n", channel, control_val);
    
    bool enable = (control_val >> 15) & 1;
    int timing = (control_val >> 12) & 3;
    
    if (enable && timing == 0) {
        perform_dma(channel);
    }
}

void memory_check_dma_vblank(void) {
    for (int i=0; i<4; i++) {
        u32 base = 0x040000B0 + (i * 12);
        int io_offset = base - 0x04000000;
        u16 control_val = *(u16 *)&io_regs[io_offset + 10];
        
        bool enable = (control_val >> 15) & 1;
        int timing = (control_val >> 12) & 3;
        
        // Timing 1 = VBlank
        if (enable && timing == 1) {
            // printf("[DMA] VBlank Trigger Channel %d\n", i);
            perform_dma(i);
        }
    }
}

void mmu_write32(u32 addr, u32 value) {
  // Stub
}
