#include "../include/cpu.h"
#include "../include/memory.h"
#include <stdio.h>
#include <assert.h>

void test_arm_basic_alu() {
    printf("Testing ARM Basic ALU...\n");
    ARM7TDMI cpu;
    cpu_init(&cpu);
    cpu.r[REG_PC] = 0x02000000;
    
    // 1. UPDATED: MOV R0, #42
    // E3A0002A
    bus_write32(0x02000000, 0xE3A0002A);
    
    // 2. ADD R1, R0, #10
    // 1110 00 1 0100 0 0000 0001 000000001010
    // E280100A
    bus_write32(0x02000004, 0xE280100A);
    
    // 3. SUB R2, R1, #5
    // 1110 00 1 0010 0 0001 0010 000000000101
    // E2412005
    bus_write32(0x02000008, 0xE2412005);
    
    // 4. AND R3, R2, #0xF
    // E202300F
    bus_write32(0x0200000C, 0xE202300F);

    // Step 1: MOV
    cpu_step(&cpu);
    if (cpu.r[0] != 42) printf("FAIL: MOV R0, #42 -> %d\n", cpu.r[0]);
    else printf("PASS: MOV R0, #42\n");
    
    // Step 2: ADD
    cpu_step(&cpu);
    if (cpu.r[1] != 52) printf("FAIL: ADD R1, R0, #10 -> %d\n", cpu.r[1]);
    else printf("PASS: ADD R1, R0, #10\n");
    
    // Step 3: SUB
    cpu_step(&cpu);
    if (cpu.r[2] != 47) printf("FAIL: SUB R2, R1, #5 -> %d\n", cpu.r[2]);
    else printf("PASS: SUB R2, R1, #5\n");

    // Step 4: AND
    cpu_step(&cpu); // 47 (0x2F) & 0xF = 0xF (15)
    if (cpu.r[3] != 15) printf("FAIL: AND R3, R2, #15 -> %d\n", cpu.r[3]);
    else printf("PASS: AND R3, R2, #15\n");
}

void test_arm_memory() {
    printf("Testing ARM Memory (LDR/STR)...\n");
    ARM7TDMI cpu;
    cpu_init(&cpu);
    cpu.r[REG_PC] = 0x02000000;
    
    // Setup R0 with address 0x02002000 (Safe WRAM)
    // MOV R0, #0x02000000 (Can't load immediate > 8 bits easily without rotate)
    // We'll manually set register for this test setup
    cpu.r[0] = 0x02002000;
    cpu.r[1] = 0xDEADBEEF;
    
    // STR R1, [R0]
    // 1110 01 0 1100 0 0000 0001 000000000000 (Offset 0)
    // E5801000
    bus_write32(0x02000000, 0xE5801000);
    
    // LDR R2, [R0]
    // E5902000
    bus_write32(0x02000004, 0xE5902000);
    
    // Step 1: STR
    cpu_step(&cpu);
    u32 mem_val = bus_read32(0x02002000);
    if (mem_val != 0xDEADBEEF) printf("FAIL: STR R1, [R0] -> Mem = %X\n", mem_val);
    else printf("PASS: STR R1, [R0]\n");
    
    // Step 2: LDR
    cpu_step(&cpu);
    if (cpu.r[2] != 0xDEADBEEF) printf("FAIL: LDR R2, [R0] -> R2 = %X\n", cpu.r[2]);
    else printf("PASS: LDR R2, [R0]\n");
}

void test_thumb_basic() {
    printf("Testing Thumb Basic...\n");
    ARM7TDMI cpu;
    cpu_init(&cpu);
    cpu.r[REG_PC] = 0x02000000;
    cpu.cpsr |= FLAG_T; // Set Thumb Mode manually
    
    // 1. MOV R0, #10
    // Format 3: 001 00 (MOV) 000 (R0) 00001010 (10)
    // 0010 0000 0000 1010 -> 200A
    bus_write16(0x02000000, 0x200A);
    
    // 2. ADD R0, #5
    // Format 3: 001 10 (ADD) 000 (R0) 00000101 (5)
    // 0011 0000 0000 0101 -> 3005
    bus_write16(0x02000002, 0x3005);
    
    // 3. LSL R1, R0, #2
    // Format 1: 000 00 (LSL) 00010 (2) 000 (R0) 001 (R1)
    // Binary: 0000 0000 1000 0001 -> 0081
    bus_write16(0x02000004, 0x0081);

    // Step 1: MOV
    cpu_step(&cpu); 
    if (cpu.r[0] != 10) printf("FAIL: [Thumb] MOV R0, #10 -> %d\n", cpu.r[0]);
    else printf("PASS: [Thumb] MOV R0, #10\n");
    
    // Step 2: ADD
    cpu_step(&cpu); // 10 + 5 = 15
    if (cpu.r[0] != 15) printf("FAIL: [Thumb] ADD R0, #5 -> %d\n", cpu.r[0]);
    else printf("PASS: [Thumb] ADD R0, #5\n");
    
    // Step 3: LSL
    cpu_step(&cpu); // 15 << 2 = 60
    if (cpu.r[1] != 60) printf("FAIL: [Thumb] LSL R1, R0, #2 -> %d\n", cpu.r[1]);
    else printf("PASS: [Thumb] LSL R1, R0, #2\n");
}

int main() {
    printf("Running CPU Unit Tests...\n");
    memory_init(); 
    
    test_arm_basic_alu();
    test_arm_memory();
    test_thumb_basic();
    
    printf("Tests Complete.\n");
    return 0;
}
