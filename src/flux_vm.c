#include "flux_vm.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

// Helper: read signed i16 from bytecode at position
static int16_t read_i16(const uint8_t* bc, int32_t pos) {
    return (int16_t)((uint16_t)bc[pos] | ((uint16_t)bc[pos+1] << 8));
}

// Helper: read unsigned u16
static uint16_t read_u16(const uint8_t* bc, int32_t pos) {
    return (uint16_t)bc[pos] | ((uint16_t)bc[pos+1] << 8);
}

// Helper: sign-extend 8-bit to 32-bit
static int32_t sign_ext8(int8_t val) {
    return (int32_t)val;
}

// Format size lookup
int flux_format_size(uint8_t opcode) {
    if (opcode <= 0x07) return 1;  // Format A
    if (opcode <= 0x0F) return 2;  // Format B
    if (opcode <= 0x17) return 2;  // Format C
    if (opcode <= 0x1F) return 3;  // Format D
    if (opcode <= 0x3F) return 4;  // Format E
    if (opcode <= 0x47) return 4;  // Format F
    if (opcode <= 0x4F) return 5;  // Format G
    if (opcode <= 0x6F) return 4;  // CONF_ and A2A (Format E)
    if (opcode <= 0x7F) return 4;  // Viewpoint (Format E)
    if (opcode <= 0x8F) return 4;  // Sensor (Format E)
    if (opcode <= 0x9F) return 4;  // Math (Format E)
    if (opcode <= 0xAF) return 4;  // Collection (Format E)
    if (opcode <= 0xBF) return 4;  // Vector (Format E)
    if (opcode <= 0xCF) return 4;  // Tensor (Format E)
    if (opcode <= 0xDF) return 5;  // Ext memory (Format G)
    if (opcode <= 0xEF) return 4;  // Long jumps (Format F)
    if (opcode <= 0xFF) return 1;  // Extended system (Format A)
    return 1;
}

void flux_vm_init(FluxVM* vm) {
    memset(vm, 0, sizeof(FluxVM));
    vm->sp = FLUX_STACK_SIZE;
    vm->agent_id = -1;
}

int32_t flux_vm_execute(FluxVM* vm, const uint8_t* bytecode, int32_t len) {
    while (!vm->halted && !vm->faulted && vm->pc < len) {
        int32_t result = flux_vm_step(vm, bytecode, len);
        if (result < 0) break;   // error
        if (result == 0) continue; // jump: pc already set by step
        vm->pc += result;         // normal: advance by instruction size
    }
    return vm->cycles;
}

int32_t flux_vm_step(FluxVM* vm, const uint8_t* bc, int32_t len) {
    if (vm->pc >= len) return 0;
    
    uint8_t op = bc[vm->pc];
    int size = flux_format_size(op);
    
    // Bounds check
    if (vm->pc + size > len) {
        vm->faulted = true;
        vm->fault_code = 1; // bytecode overrun
        return 0;
    }
    
    vm->cycles++;
    
    // Decrement stripconf if active
    if (vm->stripconf_remaining > 0) {
        vm->stripconf_remaining--;
    }
    int use_conf = (vm->stripconf_remaining == 0);
    
    switch (op) {
        // ── Format A ──────────────────────────────────────────────
        case OP_HALT:
            vm->halted = true;
            return 1;
        case OP_NOP:
            return 1;
        case OP_RET: {
            // Pop return address
            if (vm->sp < FLUX_STACK_SIZE) {
                vm->pc = vm->stack[vm->sp++];
            }
            return 1;
        }
        case OP_BRK:
            // Breakpoint — just halt for now
            vm->halted = true;
            return 1;
            
        // ── Format B ──────────────────────────────────────────────
        case OP_INC: {
            uint8_t rd = bc[vm->pc + 1];
            vm->gp[rd]++;
            vm->zero_flag = (vm->gp[rd] == 0);
            vm->neg_flag = (vm->gp[rd] < 0);
            return 2;
        }
        case OP_DEC: {
            uint8_t rd = bc[vm->pc + 1];
            vm->gp[rd]--;
            vm->zero_flag = (vm->gp[rd] == 0);
            vm->neg_flag = (vm->gp[rd] < 0);
            return 2;
        }
        case OP_NOT: {
            uint8_t rd = bc[vm->pc + 1];
            vm->gp[rd] = ~vm->gp[rd];
            return 2;
        }
        case OP_NEG: {
            uint8_t rd = bc[vm->pc + 1];
            vm->gp[rd] = -vm->gp[rd];
            return 2;
        }
        case OP_PUSH: {
            uint8_t rd = bc[vm->pc + 1];
            vm->stack[--vm->sp] = vm->gp[rd];
            return 2;
        }
        case OP_POP: {
            uint8_t rd = bc[vm->pc + 1];
            vm->gp[rd] = vm->stack[vm->sp++];
            return 2;
        }
            
        // ── Format C ──────────────────────────────────────────────
        case OP_SYS:
            // System call — stub
            return 2;
        case OP_STRIPCONF: {
            uint8_t n = bc[vm->pc + 1];
            vm->stripconf_remaining = n;
            return 2;
        }
        case OP_YIELD:
            return 2;
            
        // ── Format D ──────────────────────────────────────────────
        case OP_MOVI: {
            uint8_t rd = bc[vm->pc + 1];
            int8_t imm = (int8_t)bc[vm->pc + 2];
            vm->gp[rd] = sign_ext8(imm);
            return 3;
        }
        case OP_ADDI: {
            uint8_t rd = bc[vm->pc + 1];
            int8_t imm = (int8_t)bc[vm->pc + 2];
            vm->gp[rd] += sign_ext8(imm);
            vm->zero_flag = (vm->gp[rd] == 0);
            vm->neg_flag = (vm->gp[rd] < 0);
            return 3;
        }
        case OP_SUBI: {
            uint8_t rd = bc[vm->pc + 1];
            int8_t imm = (int8_t)bc[vm->pc + 2];
            vm->gp[rd] -= sign_ext8(imm);
            vm->zero_flag = (vm->gp[rd] == 0);
            vm->neg_flag = (vm->gp[rd] < 0);
            return 3;
        }
        case OP_ANDI: {
            uint8_t rd = bc[vm->pc + 1];
            vm->gp[rd] &= bc[vm->pc + 2];
            return 3;
        }
        case OP_ORI: {
            uint8_t rd = bc[vm->pc + 1];
            vm->gp[rd] |= bc[vm->pc + 2];
            return 3;
        }
        case OP_SHLI: {
            uint8_t rd = bc[vm->pc + 1];
            vm->gp[rd] <<= bc[vm->pc + 2];
            return 3;
        }
        case OP_SHRI: {
            uint8_t rd = bc[vm->pc + 1];
            vm->gp[rd] >>= bc[vm->pc + 2];
            return 3;
        }
            
        // ── Format E — Arithmetic ─────────────────────────────────
        case OP_ADD: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            vm->gp[rd] = vm->gp[rs1] + vm->gp[rs2];
            vm->zero_flag = (vm->gp[rd] == 0);
            vm->neg_flag = (vm->gp[rd] < 0);
            return 4;
        }
        case OP_SUB: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            vm->gp[rd] = vm->gp[rs1] - vm->gp[rs2];
            vm->zero_flag = (vm->gp[rd] == 0);
            vm->neg_flag = (vm->gp[rd] < 0);
            return 4;
        }
        case OP_MUL: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            vm->gp[rd] = vm->gp[rs1] * vm->gp[rs2];
            return 4;
        }
        case OP_DIV: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            if (vm->gp[rs2] == 0) { vm->faulted = true; vm->fault_code = 2; return 4; }
            vm->gp[rd] = vm->gp[rs1] / vm->gp[rs2];
            return 4;
        }
        case OP_MOD: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            if (vm->gp[rs2] == 0) { vm->faulted = true; vm->fault_code = 2; return 4; }
            vm->gp[rd] = vm->gp[rs1] % vm->gp[rs2];
            return 4;
        }
        case OP_AND: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            vm->gp[rd] = vm->gp[rs1] & vm->gp[rs2];
            return 4;
        }
        case OP_OR: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            vm->gp[rd] = vm->gp[rs1] | vm->gp[rs2];
            return 4;
        }
        case OP_XOR: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            vm->gp[rd] = vm->gp[rs1] ^ vm->gp[rs2];
            return 4;
        }
        case OP_SHL: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            vm->gp[rd] = vm->gp[rs1] << vm->gp[rs2];
            return 4;
        }
        case OP_SHR: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            vm->gp[rd] = vm->gp[rs1] >> vm->gp[rs2];
            return 4;
        }
        case OP_MIN: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            vm->gp[rd] = (vm->gp[rs1] < vm->gp[rs2]) ? vm->gp[rs1] : vm->gp[rs2];
            return 4;
        }
        case OP_MAX: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            vm->gp[rd] = (vm->gp[rs1] > vm->gp[rs2]) ? vm->gp[rs1] : vm->gp[rs2];
            return 4;
        }
        case OP_CMP_EQ: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            vm->gp[rd] = (vm->gp[rs1] == vm->gp[rs2]) ? 1 : 0;
            return 4;
        }
        case OP_CMP_LT: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            vm->gp[rd] = (vm->gp[rs1] < vm->gp[rs2]) ? 1 : 0;
            return 4;
        }
        case OP_CMP_GT: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            vm->gp[rd] = (vm->gp[rs1] > vm->gp[rs2]) ? 1 : 0;
            return 4;
        }
        case OP_CMP_NE: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2], rs2 = bc[vm->pc + 3];
            vm->gp[rd] = (vm->gp[rs1] != vm->gp[rs2]) ? 1 : 0;
            return 4;
        }
            
        // ── Format E — Memory & Control ───────────────────────────
        case OP_MOV: {
            uint8_t rd = bc[vm->pc + 1], rs1 = bc[vm->pc + 2];
            vm->gp[rd] = vm->gp[rs1];
            return 4;
        }
        case OP_JZ: {
            uint8_t rd = bc[vm->pc + 1];
            int8_t off = (int8_t)bc[vm->pc + 2];
            if (vm->gp[rd] == 0) { vm->pc += (int32_t)off; return 0; }
            return 4;
        }
        case OP_JNZ: {
            uint8_t rd = bc[vm->pc + 1];
            int8_t off = (int8_t)bc[vm->pc + 2];
            if (vm->gp[rd] != 0) { vm->pc += (int32_t)off; return 0; }
            return 4;
        }
        case OP_JLT: {
            uint8_t rd = bc[vm->pc + 1];
            int8_t off = (int8_t)bc[vm->pc + 2];
            if (vm->gp[rd] < 0) { vm->pc += (int32_t)off; return 0; }
            return 4;
        }
        case OP_JGT: {
            uint8_t rd = bc[vm->pc + 1];
            int8_t off = (int8_t)bc[vm->pc + 2];
            if (vm->gp[rd] > 0) { vm->pc += (int32_t)off; return 0; }
            return 4;
        }
            
        // ── Format F ──────────────────────────────────────────────
        case OP_MOVI16: {
            uint8_t rd = bc[vm->pc + 1];
            int16_t imm = read_i16(bc, vm->pc + 2);
            vm->gp[rd] = (int32_t)imm;
            return 4;
        }
        case OP_JMP: {
            int16_t off = read_i16(bc, vm->pc + 2);
            vm->pc += (int32_t)off;
            return 0;
        }
        case OP_JAL: {
            uint8_t rd = bc[vm->pc + 1];
            int16_t off = read_i16(bc, vm->pc + 2);
            vm->gp[rd] = vm->pc + 4;
            vm->pc += (int32_t)off;
            return 0;
        }
        case OP_LOOP: {
            uint8_t rd = bc[vm->pc + 1];
            int16_t off = read_i16(bc, vm->pc + 2);
            vm->gp[rd]--;
            if (vm->gp[rd] > 0) { vm->pc -= (int32_t)off; return 0; }
            return 4;
        }
            
        default:
            vm->faulted = true;
            vm->fault_code = 99; // unknown opcode
            return size;
    }
}
