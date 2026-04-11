#ifndef FLUX_VM_H
#define FLUX_VM_H

#include "formats.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    // General purpose registers
    int32_t gp[FLUX_NUM_REGS];
    
    // Confidence registers (parallel to GP)
    int32_t conf[FLUX_NUM_REGS];
    
    // Float registers (overlay on GP, accessed via float ops)
    float   fp[FLUX_NUM_REGS];
    
    // Flags
    int32_t zero_flag;
    int32_t neg_flag;
    
    // Stack
    int32_t stack[FLUX_STACK_SIZE];
    int32_t sp;
    
    // Program counter
    int32_t pc;
    
    // Memory
    uint8_t memory[FLUX_MEMORY_SIZE];
    
    // State
    bool halted;
    bool faulted;
    int32_t fault_code;
    int32_t cycles;
    int32_t agent_id;
    
    // Strip confidence counter
    int32_t stripconf_remaining;
} FluxVM;

void flux_vm_init(FluxVM* vm);
int32_t flux_vm_execute(FluxVM* vm, const uint8_t* bytecode, int32_t len);
int32_t flux_vm_step(FluxVM* vm, const uint8_t* bytecode, int32_t len);

// Format detection
int flux_format_size(uint8_t opcode);

#endif // FLUX_VM_H
