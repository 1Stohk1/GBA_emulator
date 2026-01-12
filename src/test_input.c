#include "../include/memory.h"
#include <stdio.h>
#include <assert.h>

void test_input_read_write() {
    printf("Testing Input Read/Write...\n");
    memory_init();
    
    // Default state: 0x03FF (All Released)
    u16 keys = bus_read16(0x04000130);
    if (keys != 0x03FF) {
        printf("FAIL: Initial Keys expected 0x03FF, got 0x%04X\n", keys);
    } else {
        printf("PASS: Initial Keys 0x03FF\n");
    }
    
    // Press A (Bit 0 = 0)
    // Mask: 1111 1111 1111 1110 -> 0x03FE
    memory_set_key_state(0x03FE);
    
    keys = bus_read16(0x04000130);
    if (keys != 0x03FE) {
        printf("FAIL: Keys expected 0x03FE, got 0x%04X\n", keys);
    } else {
        printf("PASS: Key A Pressed (0x03FE)\n");
    }
    
    // Press Start (Bit 3) and Select (Bit 2)
    // 0x03FF & ~(1<<3) & ~(1<<2) = 0x03FF & ~1100 = 0x3F3
    memory_set_key_state(0x03F3);
    
    keys = bus_read16(0x04000130);
    if (keys != 0x03F3) {
        printf("FAIL: Keys expected 0x03F3, got 0x%04X\n", keys);
    } else {
        printf("PASS: Start+Select Pressed (0x03F3)\n");
    }
}

int main() {
    test_input_read_write();
    return 0;
}
