#include "../include/cpu.h"
#include "../include/memory.h"
#include <stdio.h>
#include <string.h>

void cpu_init(ARM7TDMI *cpu) {
  memset(cpu, 0, sizeof(ARM7TDMI));
  cpu->cpsr = 0x13;            // Supervisor mode
  cpu->r[REG_PC] = 0x08000000; // Reset vector (ROM start)
  // printf("CPU Initialized. PC at 0x%08X\n", cpu->r[REG_PC]);
}

// Check instruction condition
bool check_condition(u32 cond, u32 cpsr) {
  switch (cond) {
  case 0x0:
    return (cpsr & FLAG_Z); // EQ
  case 0x1:
    return !(cpsr & FLAG_Z); // NE
  case 0x2:
    return (cpsr & FLAG_C); // CS / HS
  case 0x3:
    return !(cpsr & FLAG_C); // CC / LO
  case 0x4:
    return (cpsr & FLAG_N); // MI
  case 0x5:
    return !(cpsr & FLAG_N); // PL
  case 0x6:
    return (cpsr & FLAG_V); // VS
  case 0x7:
    return !(cpsr & FLAG_V); // VC
  case 0x8:
    return (cpsr & FLAG_C) && !(cpsr & FLAG_Z); // HI
  case 0x9:
    return !(cpsr & FLAG_C) || (cpsr & FLAG_Z); // LS
  case 0xA:
    return (!!(cpsr & FLAG_N) == !!(cpsr & FLAG_V)); // GE
  case 0xB:
    return (!!(cpsr & FLAG_N) != !!(cpsr & FLAG_V)); // LT
  case 0xC:
    return !(cpsr & FLAG_Z) && (!!(cpsr & FLAG_N) == !!(cpsr & FLAG_V)); // GT
  case 0xD:
    return (cpsr & FLAG_Z) || (!!(cpsr & FLAG_N) != !!(cpsr & FLAG_V)); // LE
  case 0xE:
    return true; // AL
  default:
    return true; // Undefined
  }
}

// Barrel Shifter Helper
// Returns the shifted value and updates carry_out if applicable
u32 barrel_shift(u32 val, u8 shift_type, u8 amount, u32 *carry_out) {
  // shift_type: 0=LSL, 1=LSR, 2=ASR, 3=ROR
  u32 result = val;

  switch (shift_type) {
  case 0: // LSL
    if (amount == 0) {
      // No shift, carry unaffected (handled by caller generally)
    } else if (amount < 32) {
      *carry_out = (val >> (32 - amount)) & 1;
      result = val << amount;
    } else if (amount == 32) {
      *carry_out = val & 1;
      result = 0;
    } else {
      *carry_out = 0;
      result = 0;
    }
    break;

  case 1: // LSR
    // Note: Immediate LSR #0 is usually decoded as LSR #32 by caller
    if (amount == 0) {
      // LSL #0 (No shift) equivalent
    } else if (amount < 32) {
      *carry_out = (val >> (amount - 1)) & 1;
      result = val >> amount;
    } else if (amount == 32) {
      *carry_out = (val >> 31) & 1;
      result = 0;
    } else {
      *carry_out = 0;
      result = 0;
    }
    break;

  case 2: // ASR
    // Note: Immediate ASR #0 is usually decoded as ASR #32 by caller
    if (amount == 0) {
      // LSL #0
    } else if (amount < 32) {
      *carry_out = (val >> (amount - 1)) & 1;
      result = (int32_t)val >> amount;
    } else {
      *carry_out = (val >> 31) & 1;
      result = (val & 0x80000000) ? 0xFFFFFFFF : 0;
    }
    break;

  case 3: // ROR
    if (amount == 0) {
      // RRX Handling (Rotate Right Extended)
      // Needs C flag input. If not provided/handled, behaves like No Shift?
      // Standard: ROR #0 is RRX. Register ROR by 0 is No Shift.
      // This helper assumes amount is the raw shift count.
    } else {
      amount = amount & 31;
      if (amount == 0) { // ROR #32
        *carry_out = (val >> 31) & 1;
        result = val;
      } else {
        *carry_out = (val >> (amount - 1)) & 1;
        result = (val >> amount) | (val << (32 - amount));
      }
    }
    break;
  }
  return result;
}

// Forward declarations
int cpu_step_arm(ARM7TDMI *cpu);
int cpu_step_thumb(ARM7TDMI *cpu);

int cpu_step(ARM7TDMI *cpu) {
  //  // printf("Step. Thumb? %d\n", (cpu->cpsr & FLAG_T) ? 1 : 0);
  if (cpu->cpsr & FLAG_T) {
    return cpu_step_thumb(cpu);
  } else {
    return cpu_step_arm(cpu);
  }
}

int cpu_step_thumb(ARM7TDMI *cpu) {
  u32 pc = cpu->r[REG_PC];
  u16 instruction = bus_read16(pc);

  // Thumb execution logic
  // Update PC (Thumb PC is PC+4 for execution, but step is +2)
  // Fetch is at PC, Exec is effectively at PC+4 (pipeline) but for sim we just
  // fetch AT PC. Increment PC by 2
  cpu->r[REG_PC] += 2;

  // Format 1: Move Initial Shifted Register (Opcode 000...)
  if ((instruction & 0xE000) == 0x0000) {
    if ((instruction & 0x1800) != 0x1800) { // Not Add/Sub
      u32 op = (instruction >> 11) & 3;
      u32 offset5 = (instruction >> 6) & 0x1F;
      u32 rs = (instruction >> 3) & 7;
      u32 rd = instruction & 7;

      u32 val = cpu->r[rs];
      u32 carry = (cpu->cpsr & FLAG_C) ? 1 : 0;
      u32 result = 0;

      // Using existing barrel helper
      // Op: 0=LSL, 1=LSR, 2=ASR
      result = barrel_shift(val, op, offset5, &carry);

      cpu->r[rd] = result;

      // Update Flags
      if (result == 0)
        cpu->cpsr |= FLAG_Z;
      else
        cpu->cpsr &= ~FLAG_Z;
      if (result & 0x80000000)
        cpu->cpsr |= FLAG_N;
      else
        cpu->cpsr &= ~FLAG_N;
      if (carry)
        cpu->cpsr |= FLAG_C;
      else
        cpu->cpsr &= ~FLAG_C;

      // printf("  [Thumb] Shift R%d, R%d, #%d\n", rd, rs, offset5);
      return 1;
    }
  }

  // Format 2: Add/Subtract
  if ((instruction & 0xF800) == 0x1800) {
    bool I = (instruction >> 10) & 1;  // 0=Reg, 1=Imm3
    bool sub = (instruction >> 9) & 1; // 0=Add, 1=Sub
    u32 rn = (instruction >> 3) & 7;   // Source Register
    u32 rd = instruction & 7;          // Destination Register
    u32 val_n = cpu->r[rn];
    u32 val_m = 0;

    if (I) {
      val_m = (instruction >> 6) & 7; // Imm3
    } else {
      u32 rm = (instruction >> 6) & 7; // Rm
      val_m = cpu->r[rm];
    }

    u32 result = 0;
    if (sub) {
      result = val_n - val_m;
      // Flags for SUB
      if (result == 0)
        cpu->cpsr |= FLAG_Z;
      else
        cpu->cpsr &= ~FLAG_Z;
      if (result & 0x80000000)
        cpu->cpsr |= FLAG_N;
      else
        cpu->cpsr &= ~FLAG_N;
      if (val_n >= val_m)
        cpu->cpsr |= FLAG_C;
      else
        cpu->cpsr &= ~FLAG_C; // Not Borrow
    } else {
      result = val_n + val_m;
      // Flags for ADD
      if (result == 0)
        cpu->cpsr |= FLAG_Z;
      else
        cpu->cpsr &= ~FLAG_Z;
      if (result & 0x80000000)
        cpu->cpsr |= FLAG_N;
      else
        cpu->cpsr &= ~FLAG_N;
      {
        u64 sum = (u64)val_n + val_m;
        if (sum >> 32)
          cpu->cpsr |= FLAG_C;
        else
          cpu->cpsr &= ~FLAG_C;
      }
    }

    cpu->r[rd] = result;
    // printf("  [Thumb] %s R%d, R%d, %s%X\n", sub ? "SUB" : "ADD", rd, rn,
    // I ? "#" : "R", I ? val_m : (instruction >> 6) & 7);
    return 1;
  }

  // Format 4: ALU Operations
  // Format: 0100 00 Op(4 bits) Rs Rd
  if ((instruction & 0xFC00) == 0x4000) {
    u32 op = (instruction >> 6) & 0xF;
    u32 rs = (instruction >> 3) & 7;
    u32 rd = instruction & 7;

    u32 val_d = cpu->r[rd];
    u32 val_s = cpu->r[rs];
    u32 result = 0;
    u32 carry = (cpu->cpsr & FLAG_C) ? 1 : 0; // For ADC/SBC/Shifts
    u32 shifter_carry = carry;                // Default

    switch (op) {
    case 0: // AND Rd, Rs
      result = val_d & val_s;
      cpu->r[rd] = result;
      break;
    case 1: // EOR Rd, Rs
      result = val_d ^ val_s;
      cpu->r[rd] = result;
      break;
    case 2: // LSL Rd, Rs
      // Check Rs for shift amount
      if ((val_s & 0xFF) == 0) {
        result = val_d;
        // C flag unaffected
      } else {
        result = barrel_shift(val_d, 0, val_s & 0xFF, &shifter_carry);
        if (val_s & 0xFF) { // Only update C if shift happens
          if (shifter_carry)
            cpu->cpsr |= FLAG_C;
          else
            cpu->cpsr &= ~FLAG_C;
        }
      }
      cpu->r[rd] = result;
      break;
    case 3: // LSR Rd, Rs
      if ((val_s & 0xFF) == 0) {
        result = val_d;
      } else {
        result = barrel_shift(val_d, 1, val_s & 0xFF, &shifter_carry);
        if (val_s & 0xFF) {
          if (shifter_carry)
            cpu->cpsr |= FLAG_C;
          else
            cpu->cpsr &= ~FLAG_C;
        }
      }
      cpu->r[rd] = result;
      break;
    case 4: // ASR Rd, Rs
      if ((val_s & 0xFF) == 0) {
        result = val_d;
      } else {
        result = barrel_shift(val_d, 2, val_s & 0xFF, &shifter_carry);
        if (val_s & 0xFF) {
          if (shifter_carry)
            cpu->cpsr |= FLAG_C;
          else
            cpu->cpsr &= ~FLAG_C;
        }
      }
      cpu->r[rd] = result;
      break;
    case 5: // ADC Rd, Rs
      result = val_d + val_s + carry;
      // Flags...
      if (((u64)val_d + val_s + carry) >> 32)
        cpu->cpsr |= FLAG_C;
      else
        cpu->cpsr &= ~FLAG_C;
      cpu->r[rd] = result;
      break;
    case 6: // SBC Rd, Rs
      result = val_d - val_s - (!carry);
      // If borrow (val_d < val_s + !carry), C=0. Else C=1.
      if ((u64)val_d >= (u64)val_s + (!carry))
        cpu->cpsr |= FLAG_C;
      else
        cpu->cpsr &= ~FLAG_C;
      cpu->r[rd] = result;
      break;
    case 7: // ROR Rd, Rs
      if ((val_s & 0xFF) == 0) {
        result = val_d;
      } else {
        result = barrel_shift(val_d, 3, val_s & 0xFF, &shifter_carry);
        if (val_s & 0xFF) {
          if (shifter_carry)
            cpu->cpsr |= FLAG_C;
          else
            cpu->cpsr &= ~FLAG_C;
        }
      }

      cpu->r[rd] = result;
      break;
    case 8: // TST Rd, Rs
      result = val_d & val_s;
      // Flags only
      break;
    case 9: // NEG Rd, Rs (Rd = 0 - Rs)
      result = 0 - val_s;
      cpu->r[rd] = result;
      if (0 >= val_s)
        cpu->cpsr |= FLAG_C;
      else
        cpu->cpsr &= ~FLAG_C; // 0-val_s, C=1 if no borrow (0>=val_s). Correct?
      // NEG is RSB Rd, Rs, #0.
      break;
    case 10: // CMP Rd, Rs
      result = val_d - val_s;
      if (val_d >= val_s)
        cpu->cpsr |= FLAG_C;
      else
        cpu->cpsr &= ~FLAG_C;
      // Overflow...
      break;
    case 11: // CMN Rd, Rs (CMP Rd, -Rs) -> Add
      result = val_d + val_s;
      if (((u64)val_d + val_s) >> 32)
        cpu->cpsr |= FLAG_C;
      else
        cpu->cpsr &= ~FLAG_C;
      break;
    case 12: // ORR Rd, Rs
      result = val_d | val_s;
      cpu->r[rd] = result;
      break;
    case 13: // MUL Rd, Rs
      result = val_d * val_s;
      cpu->r[rd] = result;
      // C flag senseless?
      break;
    case 14: // BIC Rd, Rs (Rd &= ~Rs)
      result = val_d & (~val_s);
      cpu->r[rd] = result;
      break;
    case 15: // MVN Rd, Rs
      result = ~val_s;
      cpu->r[rd] = result;
      break;
    }

    // Update N/Z for all ALU Format 4
    if (result == 0)
      cpu->cpsr |= FLAG_Z;
    else
      cpu->cpsr &= ~FLAG_Z;
    if (result & 0x80000000)
      cpu->cpsr |= FLAG_N;
    else
      cpu->cpsr &= ~FLAG_N;

    // printf("  [Thumb] ALU Op %d Rd %d, Rs %d\n", op, rd, rs);
    return 1;
  }

  // Format 3: Move/Compare/Add/Sub Immediate (Op 001)
  // 001 Op(12-11) Rd(10-8) Offset8(7-0)
  if ((instruction & 0xE000) == 0x2000) {
    u32 op = (instruction >> 11) & 3;
    u32 rd = (instruction >> 8) & 7;
    u32 offset8 = instruction & 0xFF;

    u32 result = 0;
    if (op == 0) { // MOV
      result = offset8;
      cpu->r[rd] = result;
      // Flags
      if (result == 0)
        cpu->cpsr |= FLAG_Z;
      else
        cpu->cpsr &= ~FLAG_Z;
      if (result & 0x80000000)
        cpu->cpsr |= FLAG_N;
      else
        cpu->cpsr &= ~FLAG_N;
      // printf("  [Thumb] MOV R%d, #%d\n", rd, offset8);
    } else if (op == 1) { // CMP
      u32 val_n = cpu->r[rd];
      u32 val_m = offset8;
      u32 result = val_n - val_m;
      if (result == 0)
        cpu->cpsr |= FLAG_Z;
      else
        cpu->cpsr &= ~FLAG_Z;
      if (result & 0x80000000)
        cpu->cpsr |= FLAG_N;
      else
        cpu->cpsr &= ~FLAG_N;
      if (val_n >= val_m)
        cpu->cpsr |= FLAG_C;
      else
        cpu->cpsr &= ~FLAG_C;
      // printf("  [Thumb] CMP R%d, #%d\n", rd, offset8);
    } else if (op == 2) { // ADD
      u32 val_n = cpu->r[rd];
      u32 val_m = offset8;
      u32 result = val_n + val_m;
      // Flags (Simplified for now)
      if (result == 0)
        cpu->cpsr |= FLAG_Z;
      else
        cpu->cpsr &= ~FLAG_Z;
      if (result & 0x80000000)
        cpu->cpsr |= FLAG_N;
      else
        cpu->cpsr &= ~FLAG_N;
      cpu->r[rd] = result;
      // printf("  [Thumb] ADD R%d, #%d\n", rd, offset8);
    } else if (op == 3) { // SUB
      u32 val_n = cpu->r[rd];
      u32 val_m = offset8;
      u32 result = val_n - val_m;
      if (result == 0)
        cpu->cpsr |= FLAG_Z;
      else
        cpu->cpsr &= ~FLAG_Z;
      if (result & 0x80000000)
        cpu->cpsr |= FLAG_N;
      else
        cpu->cpsr &= ~FLAG_N;
      cpu->r[rd] = result;
      // printf("  [Thumb] SUB R%d, #%d\n", rd, offset8);
    }
    return 1;
  }

  // Format 16: Conditional Branch
  // Format: 1101 Cond Offset8
  if ((instruction & 0xF000) == 0xD000) {
    if ((instruction & 0x0F00) != 0x0F00) { // Exclude SWI (DF00)
      u32 cond = (instruction >> 8) & 0xF;
      int8_t offset = (int8_t)(instruction & 0xFF); // Signed 8-bit

      if (check_condition(cond, cpu->cpsr)) {
        cpu->r[REG_PC] += 2 + (offset << 1);
        // Target = PC_now + 2 + (offset * 2).
        // Standard: Target = PC_start + 4 + (offset * 2).
        // PC_now = PC_start + 2.
        // So Target = (PC_now - 2) + 4 + (offset * 2) = PC_now + 2 + (offset *
        // 2). printf("  [Thumb] B%X Taken. Target %08X\n", cond,
        // cpu->r[REG_PC]);
      } else {
        // printf("  [Thumb] B%X Not Taken.\n", cond);
      }
      return 1; // 1S + 1N if taken? Assume 1 for simplicity.
    }
  }

  // Format 17: Software Interrupt (SWI)
  if ((instruction & 0xFF00) == 0xDF00) {
      u8 swi_comment = instruction & 0xFF;
      printf("[CPU] SWI (Thumb) #%02X at PC=%08X\n", swi_comment, cpu->r[REG_PC]-2);
      return 1; 
  }

  return 1; // Default Data Processing

  // Format 9: Load/Store with Expected Immediate Offset (011...)
  // LDR/STR (Word): 0110 L(1) Imm5(6-10) Rn(3-5) Rd(0-2)
  // LDRB/STRB:      0111 L(1) Imm5(6-10) Rn(3-5) Rd(0-2)
  if ((instruction & 0xE000) == 0x6000) { // 011xxxxx...
    // Wait.
    // Format 9 (LDR/STR Imm5): 011 B L Imm5 Rn Rd
    // Opcode bits: 15-13 = 011.
    // Bit 12: B (Byte if 1, Word if 0). (Actually in GBATEK: 01100=STR,
    // 01101=LDR, 01110=STRB, 01111=LDRB) So Bits 15-11 define it. 01100: STR,
    // 01101: LDR. 01110: STRB, 01111: LDRB.

    bool L = (instruction >> 11) & 1;
    bool B = (instruction >> 12) & 1;

    u32 imm5 = (instruction >> 6) & 0x1F;
    u32 rn = (instruction >> 3) & 7;
    u32 rd = instruction & 7;

    u32 offset = imm5 * (B ? 1 : 4);
    u32 addr = cpu->r[rn] + offset;

    // printf("  [Thumb] %s%s R%d, [R%d, #%X]\n", L ? "LDR" : "STR", B ? "B" :
    // "", rd, rn, offset);

    if (L) {
      if (B) {
        cpu->r[rd] = bus_read8(addr);
      } else {
        u32 val = bus_read32(addr);
        // Store as rotated? LDR logic: if aligned, just read.
        cpu->r[rd] = val; // Alignment handling skipped for now
      }
    } else {
      if (B) {
        bus_write8(addr, cpu->r[rd] & 0xFF);
      } else {
        bus_write32(addr, cpu->r[rd]);
      }
    }
    return 1;
  }

  // Format 10: Halfword Data Transfer (STRH/LDRH)
  // Format: 1000 L 0 Imm5 Rn Rd
  if ((instruction & 0xF000) == 0x8000) {
    bool L = (instruction >> 11) & 1;
    u32 imm5 = (instruction >> 6) & 0x1F;
    u32 rn = (instruction >> 3) & 7;
    u32 rd = instruction & 7;

    u32 addr = cpu->r[rn] + (imm5 << 1); // Offset is Imm5 * 2

    if (L) { // LDRH
      u16 val = bus_read16(addr);
      cpu->r[rd] = val;
      // printf("  [Thumb] LDRH R%d, [R%d, #%d] (Addr %08X, Val %04X)\n", rd,
      // rn, imm5 * 2, addr, val);
    } else { // STRH
      u16 val = cpu->r[rd] & 0xFFFF;
      bus_write16(addr, val);
      // printf("  [Thumb] STRH R%d, [R%d, #%d] (Addr %08X, Val %04X)\n", rd,
      // rn, imm5 * 2, addr, val);
    }
    return 1;
  }

  // Format 13: Add Offset to Stack Pointer (ADD SP, #Imm)
  // Format: 1011 0000 0 Im7  (ADD SP, #Imm)
  // Format: 1011 0000 1 Im7  (ADD SP, #-Imm) -> SUB SP, #Imm
  if ((instruction & 0xFF00) == 0xB000) {
    bool S = (instruction >> 7) & 1; // 0=Add, 1=Sub
    u32 imm7 = (instruction & 0x7F) * 4;
    if (S) {
      cpu->r[REG_SP] -= imm7;
      // printf("  [Thumb] SUB SP, #%X\n", imm7);
    } else {
      cpu->r[REG_SP] += imm7;
      // printf("  [Thumb] ADD SP, #%X\n", imm7);
    }
    return 1;
  }

  // Format 14: Push/Pop Registers
  // PUSH: 1011 010 L Rlist
  // POP:  1011 110 R Rlist
  if ((instruction & 0xF600) == 0xB400) {
    bool L = (instruction >> 11) & 1; // 0=Push, 1=Pop
    bool R = (instruction >> 8) & 1;  // PC/LR Bit
    u8 rlist = instruction & 0xFF;

    if (L) { // POP ({Rlist} + PC)
      u32 sp = cpu->r[REG_SP];
      for (int i = 0; i < 8; i++) {
        if ((rlist >> i) & 1) {
          cpu->r[i] = bus_read32(sp);
          sp += 4;
        }
      }
      if (R) { // POP PC
        u32 new_pc = bus_read32(sp);
        sp += 4;
        cpu->r[REG_PC] = new_pc & ~1; // Thumb mode check? (Bit 0)
        // Ideally update Thumb state bit if PC Bit 0 set (Standard ARM behavior
        // T variant) But for now assume staying in Thumb or simple branch.
        // Actually POP PC does trigger interworking if Arch >= 5, GBA is
        // ARMv4T.
        if (new_pc & 1)
          cpu->cpsr |= FLAG_T;
        else
          cpu->cpsr &= ~FLAG_T;
        // Wait, BX handles T bit. POP PC in ARMv4T copies T bit?
        // Docs: "POP {PC}" in Thumb behaves interworking on ARMv4T.
      }
      cpu->r[REG_SP] = sp;
      // printf("  [Thumb] POP {Rlist%s}\n", R ? ", PC" : "");
    } else { // PUSH ({Rlist} + LR)
      u32 sp = cpu->r[REG_SP];
      if (R) { // PUSH LR
        sp -= 4;
        bus_write32(sp, cpu->r[REG_LR]);
      }
      for (int i = 7; i >= 0; i--) {
        if ((rlist >> i) & 1) {
          sp -= 4;
          bus_write32(sp, cpu->r[i]);
        }
      }
      cpu->r[REG_SP] = sp;
      // printf("  [Thumb] PUSH {Rlist%s}\n", R ? ", LR" : "");
    }
    return 1;
  }

  // Format 12: Load Address (ADD Rd, PC/SP, #Imm)
  // Format: 1010 SP Rd Imm8
  if ((instruction & 0xF000) == 0xA000) {
    bool SP = (instruction >> 11) & 1;
    u32 rd = (instruction >> 8) & 7;
    u32 imm8 = (instruction & 0xFF) * 4;
    u32 src =
        SP ? cpu->r[REG_SP]
           : (cpu->r[REG_PC] & ~2); // PC aligned 4 or 2? Align 4 in Thumb?
    // GBA Tek: "PC: The value is the address of the current instruction + 4.
    // Bit 1 of the PC is forced to zero." So (PC & ~2). OK.
    cpu->r[rd] = src + imm8;
    // printf("  [Thumb] ADD R%d, %s, #%X (Addr %08X)\n", rd, SP ? "SP" : "PC",
    // imm8, cpu->r[rd]);
    return 1;
  }

  // Format 6: PC-relative Load (LDR Rd, [PC, #Imm])
  // Format: 0100 1 Rd Imm8
  if ((instruction & 0xF800) == 0x4800) {
    u32 rd = (instruction >> 8) & 7;
    u32 imm8 = (instruction & 0xFF) * 4;
    // PC-relative load.
    // Our PC is (InstructionAddr + 2).
    // Target = (InstructionAddr + 4) + imm8
    //        = ((PC - 2) + 4) & ~2 + imm8
    //        = (PC + 2) & ~2 + imm8
    u32 base = (cpu->r[REG_PC] + 2) & ~2;
    u32 val = bus_read32(base + imm8);
    cpu->r[rd] = val;
    // printf("  [Thumb] LDR R%d, [PC, #%X] (Addr %08X, Val %08X)\n", rd, imm8,
    // base + imm8, val);
    return 1;
  }

  // Format 5: Hi-Register Operations / BX
  // Format: 0100 01 Op H1 H2 Rs/Hs Rd/Hd
  // Op: 00=ADD, 01=CMP, 10=MOV, 11=BX
  if ((instruction & 0xFC00) == 0x4400) {
    u32 op = (instruction >> 8) & 3;
    bool H1 = (instruction >> 7) & 1; // Rd msb
    bool H2 = (instruction >> 6) & 1; // Rs msb
    u32 rs = (instruction >> 3) & 7;
    u32 rd = instruction & 7;

    u32 reg_d = rd + (H1 ? 8 : 0);
    u32 reg_s = rs + (H2 ? 8 : 0);

    if (op == 3) { // BX
      // Handled separately? Or integrate here?
      // Previous BX impl:
      // if ((instruction & 0xFF00) == 0x4700) ...
      // It was Format 5 Op 3.
      // Let's keep logic merged.
      u32 target = cpu->r[reg_s];
      if (target & 1) {
        cpu->cpsr |= FLAG_T;
        cpu->r[REG_PC] = target & ~1;
        // printf("  [Thumb] BX R%d (Thumb %08X)\n", reg_s, cpu->r[REG_PC]);
      } else {
        cpu->cpsr &= ~FLAG_T;
        cpu->r[REG_PC] = target & ~2; // Align 4?
        // printf("  [Thumb] BX R%d (ARM %08X)\n", reg_s, cpu->r[REG_PC]);
      }
    } else if (op == 0) { // ADD
      cpu->r[reg_d] += cpu->r[reg_s];
      // Use CPU logic? No flags affected for Hi Reg Add mostly?
      // "There are no flags affected" (except CMP).
      // printf("  [Thumb] ADD R%d, R%d\n", reg_d, reg_s);
    } else if (op == 1) { // CMP
      u32 val_n = cpu->r[reg_d];
      u32 val_m = cpu->r[reg_s];
      u32 result = val_n - val_m;
      // Set flags (N, Z, C, V)
      if (result == 0)
        cpu->cpsr |= FLAG_Z;
      else
        cpu->cpsr &= ~FLAG_Z;
      if (result & 0x80000000)
        cpu->cpsr |= FLAG_N;
      else
        cpu->cpsr &= ~FLAG_N;
      if (val_n >= val_m)
        cpu->cpsr |= FLAG_C;
      else
        cpu->cpsr &= ~FLAG_C;
      // printf("  [Thumb] CMP R%d, R%d\n", reg_d, reg_s);
    } else if (op == 2) { // MOV
      cpu->r[reg_d] = cpu->r[reg_s];
      // printf("  [Thumb] MOV R%d, R%d\n", reg_d, reg_s);
    }
    return 1;
  }
  // Format 18: Unconditional Branch (E0xx)
  if ((instruction & 0xF800) == 0xE000) {
    int32_t offset = (instruction & 0x7FF);
    if (offset & 0x400)
      offset |= 0xFFFFF800; // Sign extend 11-bit
    offset <<= 1;
    cpu->r[REG_PC] += offset + 2; // PC is already +2, need PC+4+offset?
    // Thumb fetch is PC. Executing. PC has advanced +2.
    // Branch target is PC+4 + offset.
    // So PC (current next) + 2 + offset.
    // printf("  [Thumb] B (offset %d)\n", offset);
    return 1;
  }

  // printf("  [Thumb] Unknown: %04X\n", instruction);
  return 1;
}

int cpu_step_arm(ARM7TDMI *cpu) {
  // 1. Fetch
  u32 instruction = bus_read32(cpu->r[REG_PC]);

  // 2. Decode Condition
  u32 cond = instruction >> 28;
  if (!check_condition(cond, cpu->cpsr)) {
    cpu->r[REG_PC] += 4;
    return 1;
  }

  // BX Check (Pattern: 0001 0010 1111 1111 1111 0001 xxxx)
  // Mask: 0x0FFFFFF0, Val: 0x012FFF10
  if ((instruction & 0x0FFFFFF0) == 0x012FFF10) {
    u32 rm = instruction & 0xF;
    u32 target = cpu->r[rm];

    if (target & 1) {
      cpu->cpsr |= FLAG_T;
      cpu->r[REG_PC] = target & ~1;
      // printf("  BX R%d -> Thumb Mode at %08X\n", rm, cpu->r[REG_PC]);
    } else {
      cpu->cpsr &= ~FLAG_T;
      cpu->r[REG_PC] = target & ~3;
      // printf("  BX R%d -> ARM Mode at %08X\n", rm, cpu->r[REG_PC]);
    }
    return 3; // Pipeline flush implied by setting PC directly
  }

  // 3. Decode & Execute
  if ((instruction & 0x0C000000) == 0x00000000) {
    // Data Processing (ALU)
    u32 opcode = (instruction >> 21) & 0xF;
    bool s_bit = (instruction >> 20) & 1;
    u32 rn_idx = (instruction >> 16) & 0xF;
    u32 rd_idx = (instruction >> 12) & 0xF;

    u32 op1 = (rn_idx == REG_PC) ? (cpu->r[REG_PC] + 8) : cpu->r[rn_idx];
    u32 op2 = 0;
    u32 shifter_carry = (cpu->cpsr & FLAG_C) ? 1 : 0;
    u32 alu_carry = shifter_carry; // Default if not updated

    // Barrel Shifter Logic
    if (instruction & 0x02000000) { // Immediate Operand (Rotate)
      u32 imm = instruction & 0xFF;
      u32 rotate = ((instruction >> 8) & 0xF) * 2;
      // Using ROR for immediate rotation
      op2 = barrel_shift(imm, 3, rotate, &shifter_carry);
    } else { // Register Operand (Shift)
      u32 rm_idx = instruction & 0xF;
      u32 val = (rm_idx == REG_PC) ? (cpu->r[REG_PC] + 8) : cpu->r[rm_idx];
      u32 shift_type = (instruction >> 5) & 3;
      u32 amount = 0;

      if ((instruction >> 4) & 1) { // Register Shift
        u32 rs_idx = (instruction >> 8) & 0xF;
        amount = cpu->r[rs_idx] & 0xFF; // Bottom 8 bits
        // If amount is 0, no shift, C flag not updated by shifter (stays old C)
        if (amount == 0) {
          op2 = val;
          // shifter_carry remains old C
        } else {
          op2 = barrel_shift(val, shift_type, amount, &shifter_carry);
        }
      } else { // Immediate Shift
        amount = (instruction >> 7) & 0x1F;
        // Decode Special Immediate 0 cases
        if (amount == 0) {
          // LSL #0 -> No Shift
          if (shift_type == 0) {
            op2 = val;
          }
          // LSR #0 -> LSR #32
          else if (shift_type == 1) {
            op2 = barrel_shift(val, 1, 32, &shifter_carry);
          }
          // ASR #0 -> ASR #32
          else if (shift_type == 2) {
            op2 = barrel_shift(val, 2, 32, &shifter_carry);
          }
          // ROR #0 -> RRX
          else if (shift_type == 3) {
            // RRX: (C << 31) | (val >> 1)
            u32 old_c = (cpu->cpsr & FLAG_C) ? 1 : 0;
            shifter_carry = val & 1;
            op2 = (old_c << 31) | (val >> 1);
          }
        } else {
          op2 = barrel_shift(val, shift_type, amount, &shifter_carry);
        }
      }
    }

    u32 result = 0;
    bool write_result = true;
    bool arithmetic_op = false; // Add/Sub/Cmp etc

    switch (opcode) {
    case 0x0: // AND
      result = op1 & op2;
      break;
    case 0x1: // EOR
      result = op1 ^ op2;
      break;
    case 0x2: // SUB
      result = op1 - op2;
      alu_carry = (op1 >= op2); // Not Borrow
      arithmetic_op = true;
      break;
    case 0x3: // RSB (Reverse Subtract)
      result = op2 - op1;
      alu_carry = (op2 >= op1);
      arithmetic_op = true;
      break;
    case 0x4: // ADD
      result = op1 + op2;
      alu_carry = (result < op1); // Overflow wrap
      arithmetic_op = true;
      break;
    case 0x5: // ADC
      result = op1 + op2 + ((cpu->cpsr & FLAG_C) ? 1 : 0);
      alu_carry = (result < op1); // TODO: Fix ADC carry check for multi-step
      // Note: Accurate ADC Carry: (op1 + op2) > 0xFFFFFFFF or similar.
      // Simply: bool c_in = (cpu->cpsr & FLAG_C); u64 sum = (u64)op1 + op2 +
      // c_in; alu_carry = sum >> 32;
      {
        u64 sum = (u64)op1 + op2 + ((cpu->cpsr & FLAG_C) ? 1 : 0);
        alu_carry = (sum >> 32) & 1;
      }
      arithmetic_op = true;
      break;
    case 0x6: // SBC
      // result = op1 - op2 - !C
      {
        u32 borrow = (cpu->cpsr & FLAG_C) ? 0 : 1;
        u64 diff = (u64)op1 - op2 - borrow;
        result = (u32)diff;
        alu_carry = !(diff >> 32); // Not Borrow
      }
      arithmetic_op = true;
      break;
    case 0x7: // RSC
    {
      u32 borrow = (cpu->cpsr & FLAG_C) ? 0 : 1;
      u64 diff = (u64)op2 - op1 - borrow;
      result = (u32)diff;
      alu_carry = !(diff >> 32);
    }
      arithmetic_op = true;
      break;
    case 0x8: // TST
      result = op1 & op2;
      write_result = false;
      break;
    case 0x9: // TEQ
      result = op1 ^ op2;
      write_result = false;
      break;
    case 0xA: // CMP
      result = op1 - op2;
      alu_carry = (op1 >= op2);
      write_result = false;
      arithmetic_op = true;
      break;
    case 0xB: // CMN (Compare Negative) -> op1 + op2
      result = op1 + op2;
      {
        u64 sum = (u64)op1 + op2;
        alu_carry = (sum >> 32) & 1;
      }
      write_result = false;
      arithmetic_op = true;
      break;
    case 0xC: // ORR
      result = op1 | op2;
      break;
    case 0xD: // MOV
      result = op2;
      break;
    case 0xE: // BIC
      result = op1 & (~op2);
      break;
    case 0xF: // MVN (Move Not)
      result = ~op2;
      break;
    }

    // printf("  DP Op: %X, Res: %X, C_new: %d\n", opcode, result,
    //        arithmetic_op ? alu_carry : shifter_carry);

    if (write_result) {
      if (rd_idx == REG_PC) {
        // Writing to PC (not handled specifically here but allowed)
        // Should flush pipeline in real emu
      }
      cpu->r[rd_idx] = result;
    }

    if (s_bit) {
      if (rd_idx == REG_PC) {
        // If Rd is PC and S is set, restore SPSR to CPSR (Not fully
        // implemented)
      } else {
        // Update N, Z
        if (result & 0x80000000)
          cpu->cpsr |= FLAG_N;
        else
          cpu->cpsr &= ~FLAG_N;
        if (result == 0)
          cpu->cpsr |= FLAG_Z;
        else
          cpu->cpsr &= ~FLAG_Z;

        // Update C (Carry)
        // Arithmetic: ALU Carry
        // Logic: Shifter Carry
        if (arithmetic_op) {
          if (alu_carry)
            cpu->cpsr |= FLAG_C;
          else
            cpu->cpsr &= ~FLAG_C;
        } else {
          if (shifter_carry)
            cpu->cpsr |= FLAG_C;
          else
            cpu->cpsr &= ~FLAG_C;
        }
      }
    }

  }
  // Load / Store (Single Data Transfer)
  else if ((instruction & 0x0C000000) == 0x04000000) {
    bool I_bit = (instruction >> 25) & 1; // 0=Imm Offset, 1=Reg Offset
    bool P_bit = (instruction >> 24) & 1; // Pre/Post Index
    bool U_bit = (instruction >> 23) & 1; // Up/Down
    bool B_bit = (instruction >> 22) & 1; // Byte/Word
    bool W_bit = (instruction >> 21) & 1; // Write-back
    bool L_bit = (instruction >> 20) & 1; // Load/Store

    u32 rn_idx = (instruction >> 16) & 0xF;
    u32 rd_idx = (instruction >> 12) & 0xF;

    u32 base_addr = (rn_idx == REG_PC) ? (cpu->r[REG_PC] + 8) : cpu->r[rn_idx];
    u32 offset = 0;

    if (!I_bit) { // Immediate Offset (12-bit)
      offset = instruction & 0xFFF;
    } else {
      // Register Offset - User requested "only support immediate for now"
      // We'll treat as 0 or error.
      printf("  LDR/STR: Register offset not supported yet.\n");
    }

    u32 addr = base_addr;
    // Pre-indexing
    if (P_bit) {
      if (U_bit)
        addr += offset;
      else
        addr -= offset;
    }

    // printf("  %s%s R%d, [R%d] (Addr: %X)\n", L_bit ? "LDR" : "STR",
    //        B_bit ? "B" : "", rd_idx, rn_idx, addr);

    if (L_bit) {   // LDR
      if (B_bit) { // LDRB
        u8 val = bus_read8(addr);
        cpu->r[rd_idx] = val;
      } else { // LDR
        u32 val = bus_read32(addr);
        // Rotate aligned? ARMv4T behavior:
        // If Addr not 4-aligned, result is rotated. Simply reading aligned for
        // now. Emulating aligned read logic from basic bus_read32
        cpu->r[rd_idx] = val;
      }
    } else {       // STR
      if (B_bit) { // STRB
        // STRB takes Read register and writes lowest byte
        u8 val = cpu->r[rd_idx] & 0xFF;
        bus_write8(addr, val);
      } else { // STR
        // STR takes Read register (Rd) and writes word
        u32 val = (rd_idx == REG_PC)
                      ? (cpu->r[REG_PC] + 12)
                      : cpu->r[rd_idx]; // PC is +12 for STR? Typically PC+8
        // Actually STR PC stores PC+12 (ARM7TDMI).
        // Let's stick to simple PC+8 for general consistency or just val.
        val = cpu->r[rd_idx];
        bus_write32(addr, val);
      }
    }

    // Write-back or Post-indexing
    if (!P_bit || W_bit) {
      // !P: Post-indexing (always W implied). Start addr was base. Base updated
      // to base +/- offset. W: Pre-indexing with write-back.
      u32 new_base = (U_bit) ? base_addr + offset : base_addr - offset;
      cpu->r[rn_idx] = new_base;
    }
  } else if ((instruction & 0x0F000000) == 0x0A000000) { // Branch
    int32_t offset = (instruction & 0xFFFFFF);
    if (offset & 0x800000)
      offset |= 0xFF000000;
    cpu->r[REG_PC] += (offset << 2) + 8;
    return 3;
  } else {
    // printf("Unknown / Unimplemented Instruction: 0x%08X\n", instruction);
  }

  // 3. Advance PC
  cpu->r[REG_PC] += 4;
  return 1;
}
