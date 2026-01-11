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
    
    // Pipeline simulation or internal state could go here
    bool pipeline_flushed;
} ARM7TDMI;

// Function prototypes
void cpu_init(ARM7TDMI *cpu);
void cpu_step(ARM7TDMI *cpu);

// Helper to access named registers more easily
#define REG_SP 13
#define REG_LR 14
#define REG_PC 15

#endif // CPU_H
