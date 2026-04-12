#include <stdio.h>
#include "flux.h"

int main() {
    printf("Testing JZ instruction with detailed trace\n");
    
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
    
    printf("Instruction trace:\n");
    printf("  PC=0: MOVI R0, 0\n");
    printf("  PC=4: JZ R0, +3\n");
    printf("    Expected: jump to PC=11 (MOVI R1, 42)\n");
    printf("    Expected: skip PC=7 (MOVI R0, 99)\n");
    
    flux_vm_execute(&vm);
    
    printf("\nResults:\n");
    printf("  R0 = %d (expected 0)\n", vm.regs.gp[0]);
    printf("  R1 = %d (expected 42)\n", vm.regs.gp[1]);
    printf("  PC = %u (expected 15)\n", vm.regs.pc);
    printf("  flag_zero = %d\n", vm.flag_zero);
    
    flux_vm_free(&vm);
    return 0;
}
