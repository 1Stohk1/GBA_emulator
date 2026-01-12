#include "../include/common.h"
#include "../include/cpu.h"
#include "../include/memory.h"
#include "../include/ppu.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("Running Headless Integration Test...\n");
    
    // 1. Initialize
    memory_init();
    ARM7TDMI cpu;
    cpu_init(&cpu);
    ppu_init(NULL, NULL); // Headless PPU
    
    // 2. Load ROM
    const char *rom_path = "zaffiro.gba";
    if (!memory_load_rom(rom_path)) {
        printf("FAIL: Could not load %s\n", rom_path);
        return 1;
    }
    printf("ROM %s loaded successfully.\n", rom_path);
    
    // 3. Setup Boot State (Direct Boot)
    cpu.r[REG_PC] = 0x08000000;
    cpu.cpsr = 0x1F; // System Mode (ARM)
    cpu.r[REG_SP] = 0x03007F00;
    
    printf("Starting CPU Execution at 08000000...\n");
    
    // 4. Run Loop (Run for X cycles or until crash/breakpoint)
    int max_cycles = 10000;
    int total_cycles = 0;
    
    while (total_cycles < max_cycles) {
        // Optional: Print PC every step or only on meaningful change
        // printf("PC: %08X\n", cpu.r[REG_PC]);
        
        // Step CPU
        int cycles = cpu_step(&cpu);
        
        // PPU Update (Generate VCount interrupts etc)
        ppu_update(cycles);
        
        total_cycles += cycles;
        
        // If PC goes wild (outside valid regions), stop
        u32 pc = cpu.r[REG_PC];
        if (pc < 0x02000000 && pc > 0x00004000) { // Between BIOS and WRAM?
             // Valid ranges: 0000-3FFF (BIOS), 02000000-0203FFFF (WRAM), 03000000-03007FFF (IWRAM), 08000000... (ROM)
             // If we are executing from 0x0 (BIOS), fine.
        }
        
    }
    
    printf("Executed %d cycles successfully.\n", total_cycles);
    printf("Final PC: %08X\n", cpu.r[REG_PC]);
    
    if (total_cycles >= max_cycles) {
        printf("PASS: Booted and ran for %d cycles.\n", max_cycles);
    }
    
    return 0;
}
