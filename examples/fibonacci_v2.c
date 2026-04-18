/* Fibonacci — full bytecode, one execute call */
#include "isa_v2.h"
#include <stdio.h>

int main(void) {
    ISA2VM vm;
    isa2_init(&vm);

    uint8_t code[128];
    int pc = 0;

    /* Setup */
    isa2_encode_movi(&code[pc], 0, 0); pc += 4;   /* R0 = a = 0 */
    isa2_encode_movi(&code[pc], 1, 1); pc += 4;   /* R1 = b = 1 */
    isa2_encode_movi(&code[pc], 2, 0); pc += 4;   /* R2 = count = 0 */
    isa2_encode_movi(&code[pc], 3, 10); pc += 4;  /* R3 = limit = 10 */

    /* loop: (offset 16) */
    int loop_pc = pc;
    code[pc] = ISA2_PRINT; code[pc+1] = 0; code[pc+2] = 0; code[pc+3] = 0; pc += 4;  /* PRINT R0 */
    isa2_encode_alu(&code[pc], ISA2_IADD, 4, 0, 1); pc += 4;  /* R4 = R0 + R1 */
    /* MOV R0, R1 */
    code[pc] = ISA2_MOV; code[pc+1] = 0; code[pc+2] = 1; code[pc+3] = 0; pc += 4;
    /* MOV R1, R4 */
    code[pc] = ISA2_MOV; code[pc+1] = 1; code[pc+2] = 4; code[pc+3] = 0; pc += 4;
    /* INC R2 */
    code[pc] = ISA2_INC; code[pc+1] = 2; code[pc+2] = 0; code[pc+3] = 0; pc += 4;
    /* CMP R2, R3 */
    code[pc] = ISA2_CMP; code[pc+1] = 2; code[pc+2] = 3; code[pc+3] = 0; pc += 4;
    /* JNZ R2, back to loop_pc */
    /* JNZ uses relative offset from NEXT instruction: next_pc = pc+4+offset */
    /* We want to go to loop_pc, so offset = loop_pc - (pc + 4) */
    int offset = loop_pc - (pc + 4);
    code[pc] = ISA2_JNZ; code[pc+1] = 2;
    code[pc+2] = (uint8_t)(offset & 0xFF);
    code[pc+3] = (uint8_t)((offset >> 8) & 0xFF);
    pc += 4;
    /* HALT */
    isa2_encode_halt(&code[pc]); pc += 4;

    printf("Fibonacci (ISA v2.1 bytecode):\n");
    isa2_execute(&vm, code, pc);
    printf("\nFinal: R0=%d R1=%d count=%d cycles=%d\n",
        vm.gp[0], vm.gp[1], vm.gp[2], vm.cycles);
    return 0;
}
