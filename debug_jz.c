#include <stdio.h>
#include "flux.h"

int main() {
    printf("Testing JZ instruction\n");
    
    // MOVI R0, 0; JZ R0, +3; MOVI R0, 99; MOVI R1, 42; HALT
    uint8_t bc[] = {
        0x18, 0x00, 0x00, 0x00,  // MOVI R0, 0
        0x3C, 0x00, 0x03, 0x00,  // JZ R0, +3
        0x18, 0x00, 0x63, 0x00,  // MOVI R0, 99 (should be skipped)
        0x18, 0x01, 0x2A, 0x00,  // MOVI R1, 42
        0x00                     // HALT
    };
    
    FluxVM vm;
    flux_vm_init(&vm, bc, sizeof(bc), 4096);
    flux_vm_execute(&vm);
    
    printf("R0 = %d (expected 0)\n", vm.regs.gp[0]);
    printf("R1 = %d (expected 42)\n", vm.regs.gp[1]);
    printf("PC = %u\n", vm.regs.pc);
    printf("flag_zero = %d\n", vm.flag_zero);
    printf("flag_sign = %d\n", vm.flag_sign);
    
    flux_vm_free(&vm);
    return 0;
}
