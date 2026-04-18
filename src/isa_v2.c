#include "isa_v2.h"
#include <string.h>
#include <stdio.h>

void isa2_init(ISA2VM* vm) {
    memset(vm, 0, sizeof(ISA2VM));
}

static inline int16_t read_s16(uint8_t lo, uint8_t hi) {
    return (int16_t)((uint16_t)lo | ((uint16_t)hi << 8));
}

int32_t isa2_execute(ISA2VM* vm, uint8_t* bc, int32_t len) {
    isa2_init(vm);
    
    while (vm->pc >= 0 && vm->pc + 3 < len && !vm->halted) {
        uint8_t op = bc[vm->pc];
        uint8_t b1 = bc[vm->pc + 1];
        uint8_t b2 = bc[vm->pc + 2];
        uint8_t b3 = bc[vm->pc + 3];
        int next_pc = vm->pc + 4;
        
        switch (op) {
            case ISA2_NOP:
                break;
                
            case ISA2_MOV:  // [0x01][rd][rs][0x00]
                vm->gp[b1] = vm->gp[b2];
                break;
                
            case ISA2_MOVI: { // [0x02][rd][imm_lo][imm_hi]
                int16_t imm = read_s16(b2, b3);
                vm->gp[b1] = (int32_t)imm;
                break;
            }
            
            case ISA2_IADD:  // [0x08][rd][rs1][rs2]
                vm->gp[b1] = vm->gp[b2] + vm->gp[b3];
                break;
                
            case ISA2_ISUB:
                vm->gp[b1] = vm->gp[b2] - vm->gp[b3];
                break;
                
            case ISA2_IMUL:
                vm->gp[b1] = vm->gp[b2] * vm->gp[b3];
                break;
                
            case ISA2_IDIV:
                if (vm->gp[b3] != 0)
                    vm->gp[b1] = vm->gp[b2] / vm->gp[b3];
                break;
                
            case ISA2_INC:  // [0x0E][rd][0x00][0x00]
                vm->gp[b1]++;
                break;
                
            case ISA2_DEC:
                vm->gp[b1]--;
                break;
                
            case ISA2_CMP: { // [0x2D][rd][rs][0x00]
                int32_t diff = vm->gp[b1] - vm->gp[b2];
                vm->flags = (diff == 0) ? 1 : 0;
                break;
            }
            
            case ISA2_JNZ: { // [0x06][rd][off_lo][off_hi]
                int16_t off = read_s16(b2, b3);
                if (vm->gp[b1] != 0) {
                    next_pc = vm->pc + 4 + off;
                }
                break;
            }
            
            case ISA2_JZ: { // [0x2E][rd][off_lo][off_hi]
                int16_t off = read_s16(b2, b3);
                if (vm->gp[b1] == 0) {
                    next_pc = vm->pc + 4 + off;
                }
                break;
            }
            
            case ISA2_JMP: { // [0x30][0x00][off_lo][off_hi]
                int16_t off = read_s16(b2, b3);
                next_pc = vm->pc + 4 + off;
                break;
            }
            
            case ISA2_PUSH:  // [0x31][rd][0x00][0x00]
                if (vm->sp < ISA2_STACK_SIZE) {
                    vm->stack[vm->sp++] = vm->gp[b1];
                }
                break;
                
            case ISA2_POP:  // [0x32][rd][0x00][0x00]
                if (vm->sp > 0) {
                    vm->gp[b1] = vm->stack[--vm->sp];
                }
                break;
                
            // Extended opcodes (ISA v2.1)
            case ISA2_CALL: { // [0x40][0x00][off_lo][off_hi] — relative offset like JMP
                int16_t off = read_s16(b2, b3);
                if (vm->sp < ISA2_STACK_SIZE) {
                    vm->stack[vm->sp++] = vm->pc + 4; // push return address
                }
                next_pc = vm->pc + 4 + off; // relative jump like all other branches
                break;
            }
            
            case ISA2_RET: // [0x41][0x00][0x00][0x00]
                if (vm->sp > 0) {
                    next_pc = vm->stack[--vm->sp];
                }
                break;
            
            case ISA2_AND: // [0x42][rd][rs1][rs2]
                vm->gp[b1] = vm->gp[b2] & vm->gp[b3];
                break;
            
            case ISA2_OR: // [0x43][rd][rs1][rs2]
                vm->gp[b1] = vm->gp[b2] | vm->gp[b3];
                break;
            
            case ISA2_XOR: // [0x44][rd][rs1][rs2]
                vm->gp[b1] = vm->gp[b2] ^ vm->gp[b3];
                break;
            
            case ISA2_NOT: // [0x45][rd][rs][0x00]
                vm->gp[b1] = ~vm->gp[b2];
                break;
            
            case ISA2_SHL: // [0x46][rd][rs1][rs2]
                vm->gp[b1] = vm->gp[b2] << (vm->gp[b3] & 0x1F);
                break;
            
            case ISA2_SHR: // [0x47][rd][rs1][rs2]
                vm->gp[b1] = (int32_t)((uint32_t)vm->gp[b2] >> (vm->gp[b3] & 0x1F));
                break;
            
            case ISA2_IMOD: // [0x48][rd][rs1][rs2]
                if (vm->gp[b3] != 0)
                    vm->gp[b1] = vm->gp[b2] % vm->gp[b3];
                break;
            
            case ISA2_PRINT: // [0x49][rd][0x00][0x00]
                printf("R%d = %d\n", b1, vm->gp[b1]);
                break;
            
            case ISA2_LOAD: // [0x4A][rd][rs][0x00] — load from stack[rs]
                if (vm->gp[b2] >= 0 && vm->gp[b2] < ISA2_STACK_SIZE)
                    vm->gp[b1] = vm->stack[vm->gp[b2]];
                break;
            
            case ISA2_STORE: // [0x4B][rd][rs][0x00] — store rs to stack[rd]
                if (vm->gp[b1] >= 0 && vm->gp[b1] < ISA2_STACK_SIZE)
                    vm->stack[vm->gp[b1]] = vm->gp[b2];
                break;
            
            case ISA2_DUP: // [0x4C][rd][0x00][0x00]
                if (vm->sp < ISA2_STACK_SIZE) {
                    vm->stack[vm->sp] = vm->stack[vm->sp - 1];
                    vm->sp++;
                }
                break;
            
            case ISA2_SWAP: // [0x4D][rs1][rs2][0x00]
                { int32_t tmp = vm->gp[b1]; vm->gp[b1] = vm->gp[b2]; vm->gp[b2] = tmp; }
                break;
            
            case ISA2_NEG: // [0x4E][rd][rs][0x00]
                vm->gp[b1] = -vm->gp[b2];
                break;
            
            case ISA2_XCHG: // [0x4F][0x00][0x00][0x00]
                if (vm->sp >= 2) {
                    int32_t tmp = vm->stack[vm->sp - 1];
                    vm->stack[vm->sp - 1] = vm->stack[vm->sp - 2];
                    vm->stack[vm->sp - 2] = tmp;
                }
                break;
            
            case ISA2_HALT:
                vm->halted = true;
                break;
                
            default:
                return -1; // Unknown opcode
        }
        
        vm->pc = next_pc;
        vm->cycles++;
        
        if (vm->cycles > 1000000) break; // Safety limit
    }
    
    return vm->gp[0];
}
