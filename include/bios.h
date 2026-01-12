#ifndef BIOS_H
#define BIOS_H

#include "common.h"
#include "cpu.h"

void bios_handle_swi(ARM7TDMI *cpu, u8 swi_number);

#endif
