/* Bitwise operations demo — fully bytecode on ISA v2.1 VM */
#include "isa_v2.h"
#include <stdio.h>

static void test_op(const char* name, ISA2VM* vm, uint8_t op, int rs1, int rs2, int expected) {
    uint8_t code[64];
    int pc = 0;

    /* MOVI R10, rs1 */
    isa2_encode_movi(&code[pc], 10, (int16_t)rs1); pc += 4;
    /* MOVI R11, rs2 */
    isa2_encode_movi(&code[pc], 11, (int16_t)rs2); pc += 4;
    /* ALU op R12, R10, R11 */
    isa2_encode_alu(&code[pc], op, 12, 10, 11); pc += 4;
    /* PRINT R12 */
    code[pc] = ISA2_PRINT; code[pc+1] = 12; code[pc+2] = 0; code[pc+3] = 0; pc += 4;
    /* HALT */
    isa2_encode_halt(&code[pc]); pc += 4;

    isa2_init(vm);
    isa2_execute(vm, code, pc);

    int got = vm->gp[12];
    int ok = (got == expected);
    printf("  %s(%d, %d) = %d (expected %d) %s\n", name, rs1, rs2, got, expected, ok?"✅":"❌");
}

int main(void) {
    ISA2VM vm;

    printf("ISA v2.1 Bitwise Operations (bytecode)\n");
    printf("=======================================\n\n");

    test_op("AND",  &vm, ISA2_AND,  0b1100, 0b1010, 8);
    test_op("OR",   &vm, ISA2_OR,   0b1100, 0b1010, 14);
    test_op("XOR",  &vm, ISA2_XOR,  0b1100, 0b1010, 6);
    test_op("SHL",  &vm, ISA2_SHL,  1, 4, 16);
    test_op("SHR",  &vm, ISA2_SHR,  64, 3, 8);
    test_op("MOD",  &vm, ISA2_IMOD, 17, 5, 2);

    printf("\nAll done.\n");
    return 0;
}
