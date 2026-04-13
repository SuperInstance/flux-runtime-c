#ifndef FLUX_VM_H
#define FLUX_VM_H
#include "flux/opcodes.h"
#include "flux/registers.h"
#include "flux/memory.h"
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FLUX_OK=0,
    FLUX_ERR_HALT=1,
    FLUX_ERR_INVALID_OPCODE=2,
    FLUX_ERR_DIV_ZERO=3,
    FLUX_ERR_STACK_OVERFLOW=4,
    FLUX_ERR_STACK_UNDERFLOW=5,
    FLUX_ERR_REGISTER_OOB=6,
    FLUX_ERR_MEMORY_ACCESS=7,
    FLUX_ERR_BOX_OVERFLOW=8,
    FLUX_ERR_BYTECODE_OOB=9,
    FLUX_ERR_FRAME_OVERFLOW=10,
    FLUX_ERR_CYCLE_BUDGET=11,
    FLUX_ERR_MEMORY=12,
    FLUX_ERR_NULL_POINTER=13,
    FLUX_ERR_INVALID_BOX_ID=14
} FluxError;

#define FLUX_BOX_INT 0
#define FLUX_BOX_FLOAT 1
#define FLUX_BOX_BOOL 2
#define FLUX_BOX_MAX 64

typedef struct { int type_tag; int32_t int_val; float float_val; } FluxBox;
typedef struct FluxVM FluxVM;
typedef int (*FluxA2AHandler)(FluxVM*, uint8_t, const uint8_t*, uint16_t);
typedef void (*FluxTraceCallback)(FluxVM*, uint32_t pc, uint8_t opcode, const char* name, void* user_data);

/* Trace configuration */
typedef struct {
    int enabled;                /* 1 = tracing on */
    FluxTraceCallback callback; /* called each step, NULL = use default stderr */
    void* user_data;
    int trace_a2a;              /* trace a2a opcodes */
    int trace_memory;           /* trace memory opcodes */
    uint32_t start_cycle;       /* start tracing after this cycle */
    uint32_t max_trace;         /* max trace lines (0 = unlimited) */
    uint32_t trace_count;
} FluxTraceConfig;

struct FluxVM {
    const uint8_t* bytecode; uint32_t bytecode_len;
    FluxRegFile regs; FluxMemManager mem;
    uint8_t flag_zero, flag_sign, flag_carry, flag_overflow;
    uint32_t cycle_count, max_cycles; uint8_t running, halted;
    FluxBox box_table[FLUX_BOX_MAX]; int box_count;
    uint8_t resources[256];
    uint32_t frame_stack[256]; uint32_t frame_count;
    FluxA2AHandler a2a_handler; void* user_data;
    FluxError last_error; uint32_t error_pc; uint8_t error_opcode;
    /* Enhanced features */
    FluxTraceConfig trace;
    uint32_t instruction_count;  /* total instructions executed (including NOP) */
    const char* error_detail;    /* human-readable error detail string */
};

int flux_vm_init(FluxVM* vm, const uint8_t* bytecode, uint32_t len, uint32_t mem_size);
void flux_vm_free(FluxVM* vm);
void flux_vm_reset(FluxVM* vm);
int64_t flux_vm_execute(FluxVM* vm);
int flux_vm_step(FluxVM* vm);
void flux_vm_set_a2a(FluxVM* vm, FluxA2AHandler h);

/* Error reporting */
const char* flux_vm_error_string(FluxError err);
const char* flux_vm_error_detail(FluxVM* vm);

/* Debug/tracing */
void flux_vm_enable_trace(FluxVM* vm, FluxTraceCallback cb, void* user_data);
void flux_vm_disable_trace(FluxVM* vm);
void flux_vm_set_trace_config(FluxVM* vm, const FluxTraceConfig* config);
const char* flux_vm_opcode_mnemonic(uint8_t op);

/* Opcode dispatch lookup (for performance-critical paths) */
typedef int (*FluxOpcodeHandler)(FluxVM* vm);
FluxOpcodeHandler flux_vm_get_handler(uint8_t opcode);

/* Opcode info for debugging/disassembly */
typedef struct {
    uint8_t opcode;
    const char* name;
    const char* format;    /* e.g., "rd, rs1" or "rd, imm16" */
    int operand_count;     /* number of operand bytes after opcode */
    int category;          /* 0=system, 1=arith, 2=float, 3=mem, 4=branch, 5=a2a, 6=stack, 7=extended, 8=vector */
} FluxOpcodeInfo;
const FluxOpcodeInfo* flux_vm_opcode_info(uint8_t opcode);
int flux_vm_opcode_is_valid(uint8_t opcode);

#ifdef __cplusplus
}
#endif
#endif
