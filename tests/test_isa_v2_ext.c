// Test ISA v2.1 extended opcodes
#include "isa_v2.h"
#include <assert.h>
#include <stdio.h>

void test_call_ret() {
    // CALL to subroutine that sets R0=42, then RET
    // Main: MOVI R0, 0 → CALL sub → HALT
    // Sub:  MOVI R0, 42 → RET
    uint8_t code[32] = {0};
    int pc = 0;
    
    // MOVI R0, 0
    isa2_encode_movi(&code[pc], 0, 0); pc += 4;
    // CALL to offset 8 (the subroutine)
    code[pc] = ISA2_CALL; code[pc+1] = 0; code[pc+2] = 8; code[pc+3] = 0; pc += 4;
    // HALT
    code[pc] = ISA2_HALT; pc += 4;
    // Subroutine: MOVI R0, 42
    isa2_encode_movi(&code[pc], 0, 42); pc += 4;
    // RET
    code[pc] = ISA2_RET; pc += 4;
    
    ISA2VM vm;
    int32_t result = isa2_execute(&vm, code, pc);
    assert(result == 42);
    printf("test_call_ret: PASS (R0=%d)\n", result);
}

void test_bitwise_and() {
    uint8_t code[16] = {0};
    isa2_encode_movi(&code[0], 0, 0xFF);    // R0 = 255
    isa2_encode_movi(&code[4], 1, 0x0F);    // R1 = 15
    code[8] = ISA2_AND; code[9] = 2; code[10] = 0; code[11] = 1; // R2 = R0 & R1
    code[12] = ISA2_HALT;
    
    ISA2VM vm;
    int32_t result = isa2_execute(&vm, code, 16);
    assert(result == 15); // 255 & 15 = 15
    printf("test_bitwise_and: PASS (R2=%d)\n", vm.gp[2]);
}

void test_bitwise_xor() {
    uint8_t code[16] = {0};
    isa2_encode_movi(&code[0], 0, 0xFF);    // R0 = 255
    isa2_encode_movi(&code[4], 1, 0x0F);    // R1 = 15
    code[8] = ISA2_XOR; code[9] = 2; code[10] = 0; code[11] = 1; // R2 = R0 ^ R1
    code[12] = ISA2_HALT;
    
    ISA2VM vm;
    isa2_execute(&vm, code, 16);
    assert(vm.gp[2] == 240); // 255 ^ 15 = 240
    printf("test_bitwise_xor: PASS (R2=%d)\n", vm.gp[2]);
}

void test_shift_left() {
    uint8_t code[16] = {0};
    isa2_encode_movi(&code[0], 0, 1);       // R0 = 1
    isa2_encode_movi(&code[4], 1, 4);       // R1 = 4
    code[8] = ISA2_SHL; code[9] = 2; code[10] = 0; code[11] = 1; // R2 = R0 << R1
    code[12] = ISA2_HALT;
    
    ISA2VM vm;
    isa2_execute(&vm, code, 16);
    assert(vm.gp[2] == 16); // 1 << 4 = 16
    printf("test_shift_left: PASS (R2=%d)\n", vm.gp[2]);
}

void test_modulo() {
    uint8_t code[16] = {0};
    isa2_encode_movi(&code[0], 0, 17);      // R0 = 17
    isa2_encode_movi(&code[4], 1, 5);       // R1 = 5
    code[8] = ISA2_IMOD; code[9] = 2; code[10] = 0; code[11] = 1; // R2 = R0 % R1
    code[12] = ISA2_HALT;
    
    ISA2VM vm;
    isa2_execute(&vm, code, 16);
    assert(vm.gp[2] == 2); // 17 % 5 = 2
    printf("test_modulo: PASS (R2=%d)\n", vm.gp[2]);
}

void test_negate() {
    uint8_t code[16] = {0};
    isa2_encode_movi(&code[0], 0, 42);      // R0 = 42
    code[4] = ISA2_NEG; code[5] = 1; code[6] = 0; code[7] = 0; // R1 = -R0
    code[8] = ISA2_HALT;
    
    ISA2VM vm;
    isa2_execute(&vm, code, 16);
    assert(vm.gp[1] == -42);
    printf("test_negate: PASS (R1=%d)\n", vm.gp[1]);
}

void test_swap() {
    uint8_t code[16] = {0};
    isa2_encode_movi(&code[0], 0, 10);      // R0 = 10
    isa2_encode_movi(&code[4], 1, 20);      // R1 = 20
    code[8] = ISA2_SWAP; code[9] = 0; code[10] = 1; code[11] = 0; // SWAP R0, R1
    code[12] = ISA2_HALT;
    
    ISA2VM vm;
    isa2_execute(&vm, code, 16);
    assert(vm.gp[0] == 20 && vm.gp[1] == 10);
    printf("test_swap: PASS (R0=%d, R1=%d)\n", vm.gp[0], vm.gp[1]);
}

int main() {
    printf("=== ISA v2.1 Extended Opcode Tests ===\n\n");
    test_call_ret();
    test_bitwise_and();
    test_bitwise_xor();
    test_shift_left();
    test_modulo();
    test_negate();
    test_swap();
    printf("\n7/7 extended tests passed!\n");
    return 0;
}
