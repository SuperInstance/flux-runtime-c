#include "isa_v2.h"
#include <string.h>

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
