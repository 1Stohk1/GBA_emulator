#include "../include/bios.h"
#include "../include/memory.h"
#include <stdio.h>

// Helper to read/write memory
// We use bus_read/bus_write from memory.h

void swi_soft_reset(ARM7TDMI *cpu) {
    // 0x00: SoftReset
    // Clears 0x02000000 - 0x0203FFFF (0x40000 bytes)
    // Clears 0x03000000 - 0x03007FFF (0x8000 bytes)
    // Resets IO registers...
    // Return to 0x08000000 or 0x02000000? Usually 0x8000000.
    // For now, logging.
    printf("[BIOS] SoftReset called. Ignored for now.\n");
}

void swi_register_ram_reset(ARM7TDMI *cpu) {
    // 0x01: RegisterRamReset
    // Flags in R0 indicate what to clear.
    u32 flags = cpu->r[0];
    printf("[BIOS] RegisterRamReset Flags=%02X\n", flags);
    
    // Simplification: Clear visible things if requested
    // Bit 0: WRAM (256K on-board)
    // Bit 1: WRAM (32K on-chip)
    // Bit 2: Palette
    // Bit 3: VRAM
    // Bit 4: OAM
    // ...
    // Note: Do NOT clear if we want to debug, but game expects it 0.
    
    if (flags & 0x04) { // Palette
        for (int i=0; i<0x400; i+=4) bus_write32(0x05000000+i, 0);
    }
    if (flags & 0x08) { // VRAM
        for (int i=0; i<0x18000; i+=4) bus_write32(0x06000000+i, 0);
    }
    if (flags & 0x10) { // OAM
        for (int i=0; i<0x400; i+=4) bus_write32(0x07000000+i, 0);
    }
}

void swi_cpu_set(ARM7TDMI *cpu) {
    // 0x0B: CpuSet(src, dst, control)
    u32 src = cpu->r[0];
    u32 dst = cpu->r[1];
    u32 len_ctrl = cpu->r[2];
    
    int count = len_ctrl & 0x1FFFFF;
    bool is_32 = len_ctrl & 0x04000000; // Bit 26
    bool fixed_src = len_ctrl & 0x01000000; // Bit 24
    
    // printf("[BIOS] CpuSet Src=%08X Dst=%08X Len=%X 32bit=%d Fixed=%d\n", src, dst, count, is_32, fixed_src);
    
    if (is_32) {
        for (int i=0; i<count; i++) {
            u32 val = bus_read32(fixed_src ? src : src + i*4);
            bus_write32(dst + i*4, val);
        }
    } else {
        for (int i=0; i<count; i++) {
            u16 val = bus_read16(fixed_src ? src : src + i*2);
            bus_write16(dst + i*2, val);
        }
    }
}

void swi_cpu_fast_set(ARM7TDMI *cpu) {
    // 0x0C: CpuFastSet(src, dst, control)
    // Always 32-bit copy. count is in words.
    u32 src = cpu->r[0];
    u32 dst = cpu->r[1];
    u32 len_ctrl = cpu->r[2];
    
    int count = len_ctrl & 0x1FFFFF;
    bool fixed_src = len_ctrl & 0x01000000; // Bit 24
    
    // printf("[BIOS] CpuFastSet Src=%08X Dst=%08X Len=%X\n", src, dst, count);
    
    for (int i=0; i<count; i++) {
        u32 val = bus_read32(fixed_src ? src : src + i*4);
        bus_write32(dst + i*4, val);
    }
}

void swi_lz77_uncomp(ARM7TDMI *cpu, bool wram) {
    // 0x11: LZ77UnCompWram / 0x12: LZ77UnCompVram
    // R0: Source, R1: Dest
    u32 src = cpu->r[0];
    u32 dst = cpu->r[1];
    
    // Read Header
    u32 header = bus_read32(src);
    src += 4;
    
    // Compression Type (Bit 4-7 = 1) -> 0x10
    if ((header & 0xFF) != 0x10) {
        printf("[BIOS] LZ77 Fail: Invalid Header %08X at %08X\n", header, src-4);
        return;
    }
    
    u32 decompressed_size = header >> 8;
    printf("[BIOS] LZ77UnComp%s Src=%08X Dst=%08X Size=%X\n", wram ? "Wram" : "Vram", src-4, dst, decompressed_size);
    
    u32 current_out_size = 0;
    
    while (current_out_size < decompressed_size) {
        u8 flags = bus_read8(src++);
        for (int i=0; i<8; i++) {
             if (current_out_size >= decompressed_size) break;
             
             if (flags & 0x80) { // Compressed
                 // Read 2 bytes
                 u8 b1 = bus_read8(src++);
                 u8 b2 = bus_read8(src++);
                 
                 // (b1<<8) | b2  -> Disp: MSB 4 bits of b2 | b1. Len: Low 4 bits of b2 + 3.
                 // Actually:
                 // Flags: 1, then data is (DispMSB(4) Len(4)) (DispLSB(8))
                 // Wait, standard GBA LZ77:
                 // Data = (Byte1 << 8) | Byte2
                 // Length = (Byte1 >> 4) + 3
                 // Disp = ((Byte1 & 0xF) << 8) | Byte2
                 
                 // Let's re-read specs carefully.
                 // "Load 2 bytes: Data = [src] OR [src+1]<<8"? No Endianness matters.
                 // We read byte by byte.
                 
                 // GBATEK: 
                 // Flag=1: Compressed.
                 // Read Block (16-bit): D L D D D D D D D D D D
                 // Length = (Block >> 12) + 3. 
                 // Disp = (Block & 0xFFF).
                 // Disp is offset BACKWARDS from current dest. 
                 
                 // My read: b1 is first byte, b2 is second. 
                 // 16-bit var = (b2 << 8) | b1? No, GBA is Little Endian.
                 // But this is a stream.
                 // Usually: D L (High byte) ...
                 // Let's use simple logic:
                 // Block = bus_read8(src) | (bus_read8(src+1)<<8) ??
                 // NO. The stream is read byte-by-byte.
                 // Byte 1: (Length_High | Disp_High)
                 // Byte 2: Disp_Low
                 
                 // GBA Little Endian:
                 // Byte 1 (b1) = Disp LSB
                 // Byte 2 (b2) = Length | Disp MSB
                 
                 int length = (b2 >> 4) + 3;
                 int disp_high = b2 & 0xF;
                 int disp_low = b1;
                 int disp = (disp_high << 8) | disp_low;
                 
                 // Copy from (dst - disp - 1)
                 u32 copy_src = dst - disp - 1;
                 for (int j=0; j<length; j++) {
                     if (current_out_size >= decompressed_size) break;
                     // We must read from the *output* buffer (which might be VRAM)
                     // bus_read8 handles VRAM reading.
                     u8 val = bus_read8(copy_src++); 
                     bus_write8(dst++, val);
                     current_out_size++;
                 }
                 
             } else { // Uncompressed
                 u8 val = bus_read8(src++);
                 bus_write8(dst++, val);
                 current_out_size++;
             }
             flags <<= 1;
        }
    }
}

void swi_div(ARM7TDMI *cpu) {
    // 0x06: Div (R0 / R1)
    // Results: R0 = Quot, R1 = Rem, R3 = Abs(Quot)
    int32_t num = (int32_t)cpu->r[0];
    int32_t den = (int32_t)cpu->r[1];
    
    if (den == 0) {
        // Division by zero behavior? 
        // Usually returns large value or hangs. 
        // For now, avoid crash.
        cpu->r[0] = 0; 
        cpu->r[1] = 0; // ?
    } else {
        cpu->r[0] = num / den;
        cpu->r[1] = num % den;
        cpu->r[3] = (cpu->r[0] < 0) ? -cpu->r[0] : cpu->r[0];
    }
}

void swi_vblank_intr_wait(ARM7TDMI *cpu) {
    // 0x05: VBlankIntrWait
    // Waits for VBlank Interrupt.
    // In our HLE, we can just assume we are fast enough or just return.
    // Ideally, we should yield execution, but for now, successful return.
    // We should probably check IF/IE registers?
    // Stub: do nothing, just return. (Game will loop if it needs to wait more)
}

void bios_handle_swi(ARM7TDMI *cpu, u8 swi_number) {
    // printf("[BIOS] Handling SWI %02X\n", swi_number);
    switch (swi_number) {
        case 0x00: swi_soft_reset(cpu); break;
        case 0x01: swi_register_ram_reset(cpu); break;
        
        case 0x05: swi_vblank_intr_wait(cpu); break;
        case 0x06: swi_div(cpu); break;
        
        case 0x0B: swi_cpu_set(cpu); break;
        case 0x0C: swi_cpu_fast_set(cpu); break;
        
        case 0x11: swi_lz77_uncomp(cpu, true); break; // LZ77 WRAM
        case 0x12: swi_lz77_uncomp(cpu, false); break; // LZ77 VRAM
        
        default:
             printf("[BIOS] Unimplemented SWI %02X\n", swi_number);
            break;
    }
}
