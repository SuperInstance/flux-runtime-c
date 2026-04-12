#include <stdio.h>
#include "flux.h"

int main() {
    printf("Testing DUP instruction\n");
    
    // MOVI R0, 7; PUSH R0; DUP; POP R1; POP R2; HALT
    uint8_t bc[] = {
        0x18, 0x00, 0x07, 0x00,  // MOVI R0, 7
        0x0C, 0x00,              // PUSH R0
        0x90,                    // DUP
        0x0D, 0x01,              // POP R1
        0x0D, 0x02,              // POP R2
        0x00                     // HALT
    };
    
    FluxVM vm;
    flux_vm_init(&vm, bc, sizeof(bc), 4096);
    
    printf("Before execution:\n");
    printf("  R0 = %d, R1 = %d, R2 = %d\n", vm.regs.gp[0], vm.regs.gp[1], vm.regs.gp[2]);
    printf("  SP = %u\n", vm.regs.sp);
    
    flux_vm_execute(&vm);
    
    printf("After execution:\n");
    printf("  R0 = %d (expected 7)\n", vm.regs.gp[0]);
    printf("  R1 = %d (expected 7)\n", vm.regs.gp[1]);
    printf("  R2 = %d (expected 7)\n", vm.regs.gp[2]);
    printf("  SP = %u\n", vm.regs.sp);
    
    flux_vm_free(&vm);
    return 0;
}
