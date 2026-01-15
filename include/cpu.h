#ifndef CPU_H
#define CPU_H

#include "common.h"

// ARM7TDMI Registers
// R0-R12: General purpose
// R13: SP (Stack Pointer)
// R14: LR (Link Register)
// R15: PC (Program Counter)
typedef struct {
  u32 r[16];
  u32 cpsr; // Current Program Status Register
  u32 spsr; // Saved Program Status Register

  // Banked Registers
  // 0: User/System, 1: FIQ, 2: IRQ, 3: SVC, 4: ABT, 5: UND
  u32 r13_bank[6];
  u32 r14_bank[6];
  u32 spsr_bank[6];

  // Pipeline simulation or internal state could go here
  bool pipeline_flushed;
  bool halted; // Halt state (SWI 0x05 / 0x02)
} ARM7TDMI;

// Function prototypes
void cpu_init(ARM7TDMI *cpu);
int cpu_step(ARM7TDMI *cpu);

// Helper to access named registers more easily
#define REG_SP 13
#define REG_LR 14
#define REG_PC 15

#define FLAG_N 0x80000000
#define FLAG_Z 0x40000000
#define FLAG_C 0x20000000
#define FLAG_V 0x10000000
#define FLAG_T 0x00000020

#endif // CPU_H
