#include "../include/cpu.h"
#include <stdio.h>
#include <string.h>


void cpu_init(ARM7TDMI *cpu) {
  memset(cpu, 0, sizeof(ARM7TDMI));
  cpu->cpsr = 0x13;            // Supervisor mode
  cpu->r[REG_PC] = 0x08000000; // Reset vector (ROM start)
  printf("CPU Initialized. PC at 0x%08X\n", cpu->r[REG_PC]);
}

void cpu_step(ARM7TDMI *cpu) {
  // 1. Fetch
  // u32 instruction = mmu_read32(cpu->r[REG_PC]);

  // 2. Decode & Execute (Placeholder)
  // printf("Executing instruction at 0x%08X\n", cpu->r[REG_PC]);

  // 3. Advance PC
  cpu->r[REG_PC] += 4; // ARM mode assumed for now
}
