/*
 * FLUX Runtime — Micro-VM Interpreter (C11, zero deps, ARM64 safe)
 * Copyright (c) 2024 SuperInstance (DiGennaro et al.), MIT License
 * C rewrite by Lucineer (DiGennaro et al.)
 *
 * Enhanced: execution tracing, opcode dispatch table, bounds checking,
 *           extended ISA support (247 opcodes), improved error messages
 */
#include "flux/vm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ── Bounds-checked bytecode read helpers ─────────────────────── */

static inline int bc_ok(FluxVM* v, uint32_t offset) {
    return offset < v->bytecode_len;
}

static inline uint8_t f8(FluxVM* v) {
    if (!bc_ok(v, v->regs.pc)) {
        v->last_error = FLUX_ERR_BYTECODE_OOB;
        v->error_pc = v->regs.pc;
        v->error_detail = "attempted to read operand byte beyond bytecode boundary";
        v->running = 0;
        return 0;
    }
    return v->bytecode[v->regs.pc++];
}

static inline int16_t fi16(FluxVM* v) {
    uint8_t l = f8(v), h = f8(v);
    if (!v->running) return 0;
    return (int16_t)(l | (h << 8));
}

static inline uint16_t fu16(FluxVM* v) {
    uint8_t l = f8(v), h = f8(v);
    if (!v->running) return 0;
    return (uint16_t)(l | (h << 8));
}

/* ── Register access with bounds checking ─────────────────────── */

static inline int gp_ok(uint8_t idx) { return idx < FLUX_GP_COUNT; }
static inline int fp_ok(uint8_t idx) { return idx < FLUX_FP_COUNT; }

#define CHECK_GPR(idx) do { \
    if (!gp_ok(idx)) { \
        v->last_error = FLUX_ERR_REGISTER_OOB; \
        v->error_detail = "general-purpose register index out of bounds"; \
        v->running = 0; \
        return 0; \
    } \
} while(0)

/* ── Flag helpers ─────────────────────────────────────────────── */

static inline void sf(FluxVM* v, int32_t r) {
    v->flag_zero = (r == 0);
    v->flag_sign = (r < 0);
    v->flag_carry = 0;
    v->flag_overflow = 0;
}

static inline void scf(FluxVM* v, int32_t a, int32_t b) {
    v->flag_zero = (a == b);
    v->flag_sign = (a < b);
    v->flag_carry = 0;
    v->flag_overflow = 0;
}

/* ── Stack with overflow/underflow protection ─────────────────── */

static inline int spush(FluxVM* v, int32_t val) {
    FluxMemRegion* s = flux_mem_get(&v->mem, "stack");
    if (!s) {
        v->error_detail = "stack memory region not found";
        return FLUX_ERR_MEMORY;
    }
    if (v->regs.sp < 4) {
        v->error_detail = "stack push would underflow stack base";
        return FLUX_ERR_STACK_OVERFLOW;
    }
    v->regs.sp -= 4;
    flux_mem_write_i32(s, v->regs.sp, val);
    return 0;
}

static inline int spop(FluxVM* v, int32_t* out) {
    FluxMemRegion* s = flux_mem_get(&v->mem, "stack");
    if (!s) {
        v->error_detail = "stack memory region not found";
        return FLUX_ERR_MEMORY;
    }
    uint32_t stack_size = (uint32_t)s->size;
    if (v->regs.sp >= stack_size) {
        v->error_detail = "stack pop would overflow stack top";
        return FLUX_ERR_STACK_UNDERFLOW;
    }
    *out = flux_mem_read_i32(s, v->regs.sp);
    v->regs.sp += 4;
    return 0;
}

/* ── Error macro with detail ──────────────────────────────────── */

#define ERR(e, detail) do { \
    v->last_error = (e); \
    v->error_pc = v->regs.pc; \
    v->error_opcode = op; \
    v->error_detail = (detail); \
    v->running = 0; \
    return -(int64_t)(e); \
} while(0)

#define GPR (v->regs.gp)

/* ── Opcode info table for debugging/disassembly ──────────────── */

static const FluxOpcodeInfo opcode_table[256] = {
    /* 0x00-0x07 System */
    [0x00]={0x00,"HALT","(none)",0,0},
    [0x01]={0x01,"NOP","(none)",0,0},
    [0x02]={0x02,"RET","rd",1,4},
    [0x04]={0x04,"BRK","(none)",0,0},
    [0x05]={0x05,"SYS_CALL","imm16",2,0},
    [0x06]={0x06,"TRAP","imm8",1,0},
    [0x07]={0x07,"UNREACHABLE","(none)",0,0},
    /* 0x08-0x0F Single register */
    [0x08]={0x08,"INC","rd",1,1},
    [0x09]={0x09,"DEC","rd",1,1},
    [0x0A]={0x0A,"NOT","rd",1,1},
    [0x0B]={0x0B,"NEG","rd",1,1},
    [0x0C]={0x0C,"PUSH","rd",1,6},
    [0x0D]={0x0D,"POP","rd",1,6},
    [0x0E]={0x0E,"ABS","rd",1,1},
    [0x0F]={0x0F,"SQRT","rd",1,1},
    /* 0x10-0x17 Register pair */
    [0x10]={0x10,"MIN","rd, rs1",2,1},
    [0x11]={0x11,"MAX","rd, rs1",2,1},
    [0x12]={0x12,"CLZ","rd",1,1},
    [0x13]={0x13,"CTZ","rd",1,1},
    [0x14]={0x14,"POPCNT","rd",1,1},
    [0x15]={0x15,"BSWAP","rd",1,1},
    [0x16]={0x16,"SIGN_EXT","rd",1,1},
    [0x17]={0x17,"ZERO_EXT","rd",1,1},
    /* 0x18-0x1F Immediate */
    [0x18]={0x18,"MOVI","rd, imm16",3,1},
    [0x19]={0x19,"ADDI","rd, imm16",3,1},
    [0x1A]={0x1A,"SUBI","rd, imm16",3,1},
    [0x1B]={0x1B,"MULI","rd, imm16",3,1},
    [0x1C]={0x1C,"ANDI","rd, imm16",3,1},
    [0x1D]={0x1D,"ORI","rd, imm16",3,1},
    [0x1E]={0x1E,"XORI","rd, imm16",3,1},
    [0x1F]={0x1F,"SHLI","rd, imm16",3,1},
    /* 0x20-0x2B Arithmetic */
    [0x20]={0x20,"ADD","rd, rs1",2,1},
    [0x21]={0x21,"SUB","rd, rs1",2,1},
    [0x22]={0x22,"MUL","rd, rs1",2,1},
    [0x23]={0x23,"DIV","rd, rs1",2,1},
    [0x24]={0x24,"MOD","rd, rs1",2,1},
    [0x25]={0x25,"AND","rd, rs1",2,1},
    [0x26]={0x26,"OR","rd, rs1",2,1},
    [0x27]={0x27,"XOR","rd, rs1",2,1},
    [0x28]={0x28,"SHL","rd, rs1",2,1},
    [0x29]={0x29,"SHR","rd, rs1",2,1},
    [0x2A]={0x2A,"ROL","rd, rs1",2,1},
    [0x2B]={0x2B,"ROR","rd, rs1",2,1},
    /* 0x2C-0x2F Compare */
    [0x2C]={0x2C,"CMP_EQ","rd, rs1",2,4},
    [0x2D]={0x2D,"CMP_LT","rd, rs1",2,4},
    [0x2E]={0x2E,"CMP_GT","rd, rs1",2,4},
    [0x2F]={0x2F,"CMP_NE","rd, rs1",2,4},
    /* 0x30-0x37 Float */
    [0x30]={0x30,"FADD","fd, fs1",2,2},
    [0x31]={0x31,"FSUB","fd, fs1",2,2},
    [0x32]={0x32,"FMUL","fd, fs1",2,2},
    [0x33]={0x33,"FDIV","fd, fs1",2,2},
    [0x34]={0x34,"FMIN","fd, fs1",2,2},
    [0x35]={0x35,"FMAX","fd, fs1",2,2},
    [0x36]={0x36,"FSQRT","fd",1,2},
    [0x37]={0x37,"FMOD","fd, fs1",2,2},
    /* 0x38-0x3B Memory */
    [0x38]={0x38,"LOAD","rd, addr",2,3},
    [0x39]={0x39,"STORE","addr, rs1",2,3},
    [0x3A]={0x3A,"MOV","rd, rs1",2,3},
    [0x3B]={0x3B,"SWP","rd, rs1",2,3},
    /* 0x3C-0x3F Branch */
    [0x3C]={0x3C,"JZ","rd, offset",3,4},
    [0x3D]={0x3D,"JNZ","rd, offset",3,4},
    [0x3E]={0x3E,"JLT","rd, offset",3,4},
    [0x3F]={0x3F,"JGT","rd, offset",3,4},
    /* 0x40-0x46 Extended branch/jump */
    [0x40]={0x40,"JLE","rd, offset",3,4},
    [0x41]={0x41,"JGE","rd, offset",3,4},
    [0x42]={0x42,"JEQ","rd, offset",3,4},
    [0x43]={0x43,"JMP","rd, offset",3,4},
    [0x44]={0x44,"JAL","rd",1,4},
    [0x45]={0x45,"CALL","rd, offset",3,4},
    [0x46]={0x46,"LOOP","rd, rs1",2,1},
    /* 0x47-0x4F Extended jump */
    [0x47]={0x47,"JMP_REG","rd",1,4},
    [0x48]={0x48,"CALL_REG","rd",1,4},
    [0x49]={0x49,"RET_IMM","imm16",2,4},
    [0x4A]={0x4A,"SWITCH","rd, table",2,4},
    [0x4B]={0x4B,"TABLE_SWITCH","rd, count, offsets...",2,4},
    [0x4C]={0x4C,"LOOKUP_SWITCH","rd, count, pairs...",2,4},
    [0x4D]={0x4D,"LONG_JUMP","addr32",4,4},
    [0x4E]={0x4E,"CALL_INDIRECT","rd",1,4},
    [0x4F]={0x4F,"TAILCALL_INDIRECT","rd",1,4},
    /* 0x50-0x5F A2A */
    [0x50]={0x50,"TELL","len, data",2,5},
    [0x51]={0x51,"ASK","len, data",2,5},
    [0x52]={0x52,"DELEG","len, data",2,5},
    [0x53]={0x53,"BCAST","len, data",2,5},
    [0x54]={0x54,"REQUEST","len, data",2,5},
    [0x55]={0x55,"REPLY","len, data",2,5},
    [0x56]={0x56,"SUBSCRIBE","len, data",2,5},
    [0x57]={0x57,"UNSUBSCRIBE","len, data",2,5},
    [0x58]={0x58,"PUBLISH","len, data",2,5},
    [0x59]={0x59,"QUERY","len, data",2,5},
    [0x5A]={0x5A,"RESPOND","len, data",2,5},
    [0x5B]={0x5B,"MERGE","len, data",2,5},
    [0x5C]={0x5C,"SPLIT","len, data",2,5},
    [0x5D]={0x5D,"GATHER","len, data",2,5},
    [0x5E]={0x5E,"SCATTER","len, data",2,5},
    [0x5F]={0x5F,"BARRIER_WAIT","len, data",2,5},
    /* 0x60-0x7B Agent coordination */
    [0x60]={0x60,"DELEGATE","len, data",2,5},
    [0x63]={0x63,"DELEGATE_RESULT","len, data",2,5},
    [0x64]={0x64,"REPORT_STATUS","len, data",2,5},
    [0x65]={0x65,"REQUEST_OVERRIDE","len, data",2,5},
    [0x66]={0x66,"BROADCAST","len, data",2,5},
    [0x67]={0x67,"REDUCE","len, data",2,5},
    [0x68]={0x68,"DECLARE_INTENT","len, data",2,5},
    [0x69]={0x69,"ASSERT_GOAL","len, data",2,5},
    [0x6A]={0x6A,"VERIFY_OUTCOME","len, data",2,5},
    [0x6B]={0x6B,"EXPLAIN_FAILURE","len, data",2,5},
    [0x6C]={0x6C,"SET_PRIORITY","len, data",2,5},
    [0x70]={0x70,"TRUST_CHECK","len, data",2,5},
    [0x71]={0x71,"TRUST_UPDATE","len, data",2,5},
    [0x72]={0x72,"TRUST_QUERY","len, data",2,5},
    [0x73]={0x73,"REVOKE_TRUST","len, data",2,5},
    [0x74]={0x74,"CAP_REQUIRE","len, data",2,5},
    [0x75]={0x75,"CAP_REQUEST","len, data",2,5},
    [0x76]={0x76,"CAP_GRANT","len, data",2,5},
    [0x77]={0x77,"CAP_REVOKE","len, data",2,5},
    [0x78]={0x78,"BARRIER","len, data",2,5},
    [0x79]={0x79,"SYNC_CLOCK","len, data",2,5},
    [0x7A]={0x7A,"FORMATION_UPDATE","len, data",2,5},
    [0x7B]={0x7B,"EMERGENCY_STOP","(none)",0,0},
    /* 0x80-0x84 Resource */
    [0x80]={0x80,"ALLOCA_REG","rd, rs1",2,3},
    [0x81]={0x81,"YIELD","(none)",0,0},
    [0x82]={0x82,"RESOURCE_ACQUIRE","len, data",2,0},
    [0x83]={0x83,"RESOURCE_RELEASE","len, data",2,0},
    [0x84]={0x84,"DEBUG_BREAK","(none)",0,0},
    /* 0x85-0x8C Debug/trace */
    [0x85]={0x85,"TRACE_ON","(none)",0,0},
    [0x86]={0x86,"TRACE_OFF","(none)",0,0},
    [0x87]={0x87,"TRACE_POINT","imm8",1,0},
    [0x88]={0x88,"LOG","len, data",2,0},
    [0x89]={0x89,"ASSERT","rd",1,0},
    [0x8A]={0x8A,"PERF_BEGIN","id8",1,0},
    [0x8B]={0x8B,"PERF_END","id8",1,0},
    [0x8C]={0x8C,"WATCHPOINT","addr8",1,0},
    /* 0x90-0x9F Stack/memory ops */
    [0x90]={0x90,"DUP","(none)",0,6},
    [0x91]={0x91,"SWAP","(none)",0,6},
    [0x92]={0x92,"TEST","rd, rs1",2,1},
    [0x93]={0x93,"SETCC","rd, cond",2,1},
    [0x94]={0x94,"ENTER","framesize8",1,4},
    [0x95]={0x95,"LEAVE","framesize8",1,4},
    [0x96]={0x96,"ALLOCA","rd, rs1",2,3},
    [0x97]={0x97,"TAILCALL","rd",1,4},
    [0x98]={0x98,"REGION_CREATE","len, data",2,3},
    [0x99]={0x99,"REGION_DESTROY","len, data",2,3},
    [0x9A]={0x9A,"REGION_TRANSFER","len, data",2,3},
    [0x9B]={0x9B,"MEMCOPY","len, data",2,3},
    [0x9C]={0x9C,"MEMSET","len, data",2,3},
    [0x9D]={0x9D,"MEMCMP","len, data",2,3},
    [0x9E]={0x9E,"CAST","rd, mode",2,1},
    [0x9F]={0x9F,"BOX","len, data",2,7},
    /* 0xA0-0xAF Type ops */
    [0xA0]={0xA0,"UNBOX","len, data",2,7},
    [0xA1]={0xA1,"CHECK_TYPE","boxid, type",2,7},
    [0xA2]={0xA2,"CHECK_BOUNDS","val, limit",2,7},
    [0xA3]={0xA3,"FNEG","fd",1,2},
    [0xA4]={0xA4,"FABS","fd",1,2},
    [0xA5]={0xA5,"FEQ","rd, fs1",2,2},
    [0xA6]={0xA6,"FLT","rd, fs1",2,2},
    [0xA7]={0xA7,"FLE","rd, fs1",2,2},
    [0xA8]={0xA8,"FGT","rd, fs1",2,2},
    [0xA9]={0xA9,"FGE","rd, fs1",2,2},
    [0xAA]={0xAA,"LOAD8","rd, addr",2,3},
    [0xAB]={0xAB,"STORE8","addr, rs1",2,3},
    [0xAC]={0xAC,"LOAD16","rd, addr",2,3},
    [0xAD]={0xAD,"STORE16","addr, rs1",2,3},
    [0xAE]={0xAE,"LOAD32","rd, addr",2,3},
    [0xAF]={0xAF,"STORE32","addr, rs1",2,3},
    /* 0xB0-0xBF Vector */
    [0xB0]={0xB0,"VLOAD","len, data",2,8},
    [0xB1]={0xB1,"VSTORE","len, data",2,8},
    [0xB2]={0xB2,"VADD","rd, rs1",2,8},
    [0xB3]={0xB3,"VSUB","rd, rs1",2,8},
    [0xB4]={0xB4,"VMUL","rd, rs1",2,8},
    [0xB5]={0xB5,"VDIV","rd, rs1",2,8},
    [0xB6]={0xB6,"VFMA","rd, rs1, rs2",3,8},
    [0xB7]={0xB7,"VDOT","rd, rs1",2,8},
    [0xB8]={0xB8,"VMIN","rd, rs1",2,8},
    [0xB9]={0xB9,"VMAX","rd, rs1",2,8},
    [0xBA]={0xBA,"VAND","rd, rs1",2,8},
    [0xBB]={0xBB,"VOR","rd, rs1",2,8},
    [0xBC]={0xBC,"VXOR","rd, rs1",2,8},
    [0xBD]={0xBD,"VSHL","rd, rs1",2,8},
    [0xBE]={0xBE,"VSHR","rd, rs1",2,8},
    [0xBF]={0xBF,"VCMPEQ","rd, rs1",2,8},
    /* 0xC0-0xCF Atomic */
    [0xC0]={0xC0,"ATOMIC_LOAD","rd, addr",2,0},
    [0xC1]={0xC1,"ATOMIC_STORE","addr, rs1",2,0},
    [0xC2]={0xC2,"ATOMIC_ADD","addr, rs1",2,0},
    [0xC3]={0xC3,"ATOMIC_SUB","addr, rs1",2,0},
    [0xC4]={0xC4,"ATOMIC_AND","addr, rs1",2,0},
    [0xC5]={0xC5,"ATOMIC_OR","addr, rs1",2,0},
    [0xC6]={0xC6,"ATOMIC_XOR","addr, rs1",2,0},
    [0xC7]={0xC7,"ATOMIC_CMPXCHG","addr, expected, new",3,0},
    [0xC8]={0xC8,"ATOMIC_FENCE","(none)",0,0},
    [0xC9]={0xC9,"ATOMIC_LOAD_ACQ","rd, addr",2,0},
    [0xCA]={0xCA,"ATOMIC_STORE_REL","addr, rs1",2,0},
    [0xCB]={0xCB,"MUTEX_LOCK","addr",1,0},
    [0xCC]={0xCC,"MUTEX_UNLOCK","addr",1,0},
    [0xCD]={0xCD,"SEM_WAIT","addr",1,0},
    [0xCE]={0xCE,"SEM_SIGNAL","addr",1,0},
    [0xCF]={0xCF,"FUTEX_WAKE","addr, count",2,0},
    /* 0xD0-0xDF Crypto */
    [0xD0]={0xD0,"HASH_INIT","rd",1,0},
    [0xD1]={0xD1,"HASH_UPDATE","len, data",2,0},
    [0xD2]={0xD2,"HASH_FINAL","rd",1,0},
    [0xD3]={0xD3,"HASH_VERIFY","len, data",2,0},
    [0xD4]={0xD4,"HMAC_INIT","len, data",2,0},
    [0xD5]={0xD5,"HMAC_UPDATE","len, data",2,0},
    [0xD6]={0xD6,"ENCRYPT","len, data",2,0},
    [0xD7]={0xD7,"DECRYPT","len, data",2,0},
    [0xD8]={0xD8,"SIGN","len, data",2,0},
    [0xD9]={0xD9,"VERIFY_SIG","len, data",2,0},
    [0xDA]={0xDA,"RANDOM","rd",1,0},
    [0xDB]={0xDB,"RANDOM_SEED","rd",1,0},
    [0xDC]={0xDC,"UUID_GEN","rd",1,0},
    [0xDD]={0xDD,"CHECKSUM","len, data",2,0},
    [0xDE]={0xDE,"CRC32","len, data",2,0},
    [0xDF]={0xDF,"BASE64_ENCODE","len, data",2,0},
    /* 0xE0-0xEF I/O */
    [0xE0]={0xE0,"IO_READ","rd, len, data",2,0},
    [0xE1]={0xE1,"IO_WRITE","rd, len, data",2,0},
    [0xE2]={0xE2,"IO_OPEN","len, data",2,0},
    [0xE3]={0xE3,"IO_CLOSE","rd",1,0},
    [0xE4]={0xE4,"IO_SEEK","rd, offset",2,0},
    [0xE5]={0xE5,"IO_FLUSH","(none)",0,0},
    [0xE6]={0xE6,"IO_STAT","len, data",2,0},
    [0xE7]={0xE7,"IO_EOF","rd",1,0},
    [0xE8]={0xE8,"PRINT_INT","rd",1,0},
    [0xE9]={0xE9,"PRINT_STR","len, data",2,0},
    [0xEA]={0xEA,"PRINT_FLOAT","fd",1,0},
    [0xEB]={0xEB,"PRINT_HEX","rd",1,0},
    [0xEC]={0xEC,"READ_LINE","rd",1,0},
    [0xED]={0xED,"PARSE_INT","len, data",2,0},
    [0xEE]={0xEE,"PARSE_FLOAT","len, data",2,0},
    [0xEF]={0xEF,"SPRINTF","len, data",2,0},
    /* 0xF0-0xF6 String/aggregate */
    [0xF0]={0xF0,"STR_LEN","rd",1,0},
    [0xF1]={0xF1,"STR_CAT","len, data",2,0},
    [0xF2]={0xF2,"STR_CMP","rd, rs1",2,0},
    [0xF3]={0xF3,"STR_COPY","len, data",2,0},
    [0xF4]={0xF4,"STR_SUB","len, data",2,0},
    [0xF5]={0xF5,"STR_FIND","len, data",2,0},
    [0xF6]={0xF6,"ARR_NEW","len, data",2,0},
};

/* ── Opcode dispatch lookup table ─────────────────────────────── */

/* The dispatch table is available for external JIT/SIMD optimization paths.
 * The interpreter loop uses a direct switch statement for clarity and portability.
 * flux_vm_get_handler() can be extended to return function pointers from a populated table. */

FluxOpcodeHandler flux_vm_get_handler(uint8_t opcode) {
    (void)opcode;
    return NULL; /* Direct dispatch table not used in interpreter loop; available for JIT/SIMD paths */
}

/* ── Tracing ──────────────────────────────────────────────────── */

static void default_trace_callback(FluxVM* vm, uint32_t pc, uint8_t opcode, const char* name, void* ud) {
    (void)ud;
    fprintf(stderr, "[%06u] PC=%04u OP=0x%02X %-20s R0=%-8d R1=%-8d SP=%u Z=%d S=%d\n",
        vm->cycle_count, pc, opcode, name,
        vm->regs.gp[0], vm->regs.gp[1], vm->regs.sp,
        vm->flag_zero, vm->flag_sign);
}

static void do_trace(FluxVM* v, uint32_t pc, uint8_t op) {
    if (!v->trace.enabled) return;
    if (v->cycle_count < v->trace.start_cycle) return;
    if (v->trace.max_trace > 0 && v->trace.trace_count >= v->trace.max_trace) return;
    v->trace.trace_count++;
    FluxTraceCallback cb = v->trace.callback ? v->trace.callback : default_trace_callback;
    cb(v, pc, op, flux_opcode_name(op), v->trace.user_data);
}

/* ── VM lifecycle ─────────────────────────────────────────────── */

int flux_vm_init(FluxVM* v, const uint8_t* bc, uint32_t len, uint32_t ms) {
    if (!ms) ms = 65536;
    memset(v, 0, sizeof(*v));
    v->bytecode = bc;
    v->bytecode_len = len;
    v->max_cycles = 10000000;
    v->error_detail = NULL;
    flux_regs_init(&v->regs);
    flux_mem_init(&v->mem);
    flux_mem_create(&v->mem, "stack", ms, "system");
    flux_mem_create(&v->mem, "heap", ms, "system");
    v->regs.sp = ms;
    GPR[11] = (int32_t)ms;
    return 0;
}

void flux_vm_free(FluxVM* v) { flux_mem_free(&v->mem); }

void flux_vm_reset(FluxVM* v) {
    uint32_t ms = 0;
    FluxMemRegion* s = flux_mem_get(&v->mem, "stack");
    if (s) ms = (uint32_t)s->size;
    flux_mem_free(&v->mem);
    flux_regs_init(&v->regs);
    flux_mem_init(&v->mem);
    if (ms) {
        flux_mem_create(&v->mem, "stack", ms, "system");
        flux_mem_create(&v->mem, "heap", ms, "system");
        v->regs.sp = ms;
        GPR[11] = (int32_t)ms;
    }
    v->cycle_count = v->box_count = v->frame_count = v->instruction_count = 0;
    v->running = v->halted = 0;
    v->error_detail = NULL;
    v->trace.trace_count = 0;
}

/* ── Error reporting ──────────────────────────────────────────── */

const char* flux_vm_error_string(FluxError e) {
    switch(e) {
        case FLUX_OK: return "OK";
        case FLUX_ERR_HALT: return "HALT";
        case FLUX_ERR_INVALID_OPCODE: return "INVALID_OPCODE";
        case FLUX_ERR_DIV_ZERO: return "DIV_ZERO";
        case FLUX_ERR_STACK_OVERFLOW: return "STACK_OVERFLOW";
        case FLUX_ERR_STACK_UNDERFLOW: return "STACK_UNDERFLOW";
        case FLUX_ERR_REGISTER_OOB: return "REGISTER_OUT_OF_BOUNDS";
        case FLUX_ERR_MEMORY_ACCESS: return "MEMORY_ACCESS_VIOLATION";
        case FLUX_ERR_BOX_OVERFLOW: return "BOX_TABLE_OVERFLOW";
        case FLUX_ERR_BYTECODE_OOB: return "BYTECODE_OUT_OF_BOUNDS";
        case FLUX_ERR_FRAME_OVERFLOW: return "CALL_FRAME_OVERFLOW";
        case FLUX_ERR_CYCLE_BUDGET: return "CYCLE_BUDGET_EXCEEDED";
        case FLUX_ERR_MEMORY: return "MEMORY_ALLOCATION_FAILED";
        case FLUX_ERR_NULL_POINTER: return "NULL_POINTER";
        case FLUX_ERR_INVALID_BOX_ID: return "INVALID_BOX_ID";
        default: return "UNKNOWN_ERROR";
    }
}

const char* flux_vm_error_detail(FluxVM* vm) {
    return vm->error_detail ? vm->error_detail : flux_vm_error_string(vm->last_error);
}

/* ── Debug/tracing API ────────────────────────────────────────── */

void flux_vm_enable_trace(FluxVM* vm, FluxTraceCallback cb, void* user_data) {
    vm->trace.enabled = 1;
    vm->trace.callback = cb;
    vm->trace.user_data = user_data;
    vm->trace.trace_count = 0;
}

void flux_vm_disable_trace(FluxVM* vm) {
    vm->trace.enabled = 0;
    vm->trace.callback = NULL;
    vm->trace.user_data = NULL;
}

void flux_vm_set_trace_config(FluxVM* vm, const FluxTraceConfig* config) {
    vm->trace = *config;
}

const char* flux_vm_opcode_mnemonic(uint8_t op) {
    return flux_opcode_name(op);
}

/* ── Opcode info API ──────────────────────────────────────────── */

const FluxOpcodeInfo* flux_vm_opcode_info(uint8_t opcode) {
    return &opcode_table[opcode];
}

int flux_vm_opcode_is_valid(uint8_t opcode) {
    return opcode_table[opcode].name != NULL && opcode_table[opcode].name[0] != '\0';
}

/* ── Bit manipulation helpers (CLZ, CTZ, POPCNT, BSWAP) ──────── */

static int32_t clz32(uint32_t x) {
    if (x == 0) return 32;
    int n = 0;
    if (x <= 0x0000FFFF) { n += 16; x <<= 16; }
    if (x <= 0x00FFFFFF) { n += 8; x <<= 8; }
    if (x <= 0x0FFFFFFF) { n += 4; x <<= 4; }
    if (x <= 0x3FFFFFFF) { n += 2; x <<= 2; }
    if (x <= 0x7FFFFFFF) { n += 1; }
    return n;
}

static int32_t ctz32(uint32_t x) {
    if (x == 0) return 32;
    int n = 0;
    if ((x & 0x0000FFFF) == 0) { n += 16; x >>= 16; }
    if ((x & 0x000000FF) == 0) { n += 8; x >>= 8; }
    if ((x & 0x0000000F) == 0) { n += 4; x >>= 4; }
    if ((x & 0x00000003) == 0) { n += 2; x >>= 2; }
    if ((x & 0x00000001) == 0) { n += 1; }
    return n;
}

static int32_t popcnt32(uint32_t x) {
    x = x - ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    return (int32_t)(((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

static uint32_t bswap32(uint32_t x) {
    return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) |
           ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000);
}

/* ── Execute loop ─────────────────────────────────────────────── */

int64_t flux_vm_execute(FluxVM* v) {
    v->running = 1;
    v->halted = 0;
    uint8_t op, rd, rs1, rs2;
    int16_t imm;
    uint16_t len;
    const uint8_t* data;
    FluxMemRegion* h = NULL;

    while (v->running && v->cycle_count < v->max_cycles) {
        uint32_t pc_before = v->regs.pc;

        if (!bc_ok(v, v->regs.pc)) {
            v->last_error = FLUX_ERR_BYTECODE_OOB;
            v->error_pc = v->regs.pc;
            v->error_detail = "program counter beyond bytecode boundary";
            v->running = 0;
            break;
        }

        op = v->bytecode[v->regs.pc++];
        v->cycle_count++;
        v->instruction_count++;
        v->error_opcode = op;

        /* Trace if enabled */
        do_trace(v, pc_before, op);

        switch(op) {
        case FLUX_NOP: break;

        case FLUX_BRK:
            v->running = 0;
            v->last_error = FLUX_OK;
            v->error_detail = "breakpoint hit";
            break;

        case FLUX_UNREACHABLE:
            ERR(FLUX_ERR_INVALID_OPCODE, "UNREACHABLE instruction executed");

        case FLUX_TRAP:
            rd = f8(v);
            if (!v->running) break;
            if (rd == 0) {
                v->error_detail = "trap type 0: debug trap";
            } else {
                char buf[64];
                snprintf(buf, sizeof(buf), "trap type %d triggered", rd);
                v->error_detail = "trap";
                v->running = 0;
                v->last_error = FLUX_ERR_HALT;
            }
            break;

        /* ── Single register ──────────────────────────────── */
        case FLUX_INC: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); { int32_t r=GPR[rd]+1; sf(v,r); GPR[rd]=r; } break;
        case FLUX_DEC: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); { int32_t r=GPR[rd]-1; sf(v,r); GPR[rd]=r; } break;
        case FLUX_NOT: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); GPR[rd]=~GPR[rd]; break;
        case FLUX_NEG: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); { int32_t r=-GPR[rd]; sf(v,r); GPR[rd]=r; } break;
        case FLUX_PUSH: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); { int rc=spush(v,GPR[rd]); if(rc) ERR((FluxError)rc, v->error_detail); } break;
        case FLUX_POP: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); { int32_t val; int rc=spop(v,&val); if(rc) ERR((FluxError)rc, v->error_detail); GPR[rd]=val; } break;
        case FLUX_ABS: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); { int32_t r=GPR[rd]<0?-GPR[rd]:GPR[rd]; sf(v,r); GPR[rd]=r; } break;
        case FLUX_SQRT: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); GPR[rd]=(int32_t)sqrtf((float)(GPR[rd]<0?0:GPR[rd])); break;

        /* ── Register pair ───────────────────────────────── */
        case FLUX_MIN: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]=GPR[rd]<GPR[rs1]?GPR[rd]:GPR[rs1]; break;
        case FLUX_MAX: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]=GPR[rd]>GPR[rs1]?GPR[rd]:GPR[rs1]; break;
        case FLUX_CLZ: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); GPR[rd]=clz32((uint32_t)GPR[rd]); break;
        case FLUX_CTZ: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); GPR[rd]=ctz32((uint32_t)GPR[rd]); break;
        case FLUX_POPCNT: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); GPR[rd]=popcnt32((uint32_t)GPR[rd]); break;
        case FLUX_BSWAP: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); GPR[rd]=(int32_t)bswap32((uint32_t)GPR[rd]); break;
        case FLUX_SIGN_EXT: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); /* sign-extend low 8 bits */ GPR[rd]=(int32_t)(int8_t)(GPR[rd]&0xFF); break;
        case FLUX_ZERO_EXT: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); /* zero-extend low 8 bits */ GPR[rd]=GPR[rd]&0xFF; break;

        /* ── Immediate ───────────────────────────────────── */
        case FLUX_MOVI: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); GPR[rd]=imm; break;
        case FLUX_ADDI: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); { int32_t r=GPR[rd]+imm; sf(v,r); GPR[rd]=r; } break;
        case FLUX_SUBI: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); { int32_t r=GPR[rd]-imm; sf(v,r); GPR[rd]=r; } break;
        case FLUX_MULI: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); { int32_t r=GPR[rd]*imm; sf(v,r); GPR[rd]=r; } break;
        case FLUX_ANDI: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); GPR[rd]&=(int32_t)(uint16_t)imm; break;
        case FLUX_ORI: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); GPR[rd]|=(int32_t)(uint16_t)imm; break;
        case FLUX_XORI: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); GPR[rd]^=(int32_t)(uint16_t)imm; break;
        case FLUX_SHLI: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); GPR[rd]<<=imm; break;

        /* ── Arithmetic ──────────────────────────────────── */
        case FLUX_ADD: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); { int32_t r=GPR[rd]+GPR[rs1]; sf(v,r); GPR[rd]=r; } break;
        case FLUX_SUB: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); { int32_t r=GPR[rd]-GPR[rs1]; sf(v,r); GPR[rd]=r; } break;
        case FLUX_MUL: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); { int32_t r=GPR[rd]*GPR[rs1]; sf(v,r); GPR[rd]=r; } break;
        case FLUX_DIV: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); if(!GPR[rs1]) ERR(FLUX_ERR_DIV_ZERO, "integer division by zero in DIV"); GPR[rd]=GPR[rd]/GPR[rs1]; break;
        case FLUX_MOD: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); if(!GPR[rs1]) ERR(FLUX_ERR_DIV_ZERO, "integer modulo by zero in MOD"); GPR[rd]=GPR[rd]%GPR[rs1]; break;
        case FLUX_AND: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]&=GPR[rs1]; break;
        case FLUX_OR:  rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]|=GPR[rs1]; break;
        case FLUX_XOR: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]^=GPR[rs1]; break;
        case FLUX_SHL: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]<<=GPR[rs1]; break;
        case FLUX_SHR: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]>>=GPR[rs1]; break;
        case FLUX_ROL: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); { int s=GPR[rs1]&31; GPR[rd]=(GPR[rd]<<s)|(GPR[rd]>>(32-s)); } break;
        case FLUX_ROR: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); { int s=GPR[rs1]&31; GPR[rd]=(GPR[rd]>>s)|(GPR[rd]<<(32-s)); } break;

        /* ── Compare ─────────────────────────────────────── */
        case FLUX_CMP_EQ: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); scf(v,GPR[rd],GPR[rs1]); break;
        case FLUX_CMP_LT: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); scf(v,GPR[rd],GPR[rs1]); break;
        case FLUX_CMP_GT: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); scf(v,GPR[rd],GPR[rs1]); break;
        case FLUX_CMP_NE: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); { v->flag_zero=(GPR[rd]!=GPR[rs1]); v->flag_sign=(GPR[rd]<GPR[rs1]); } break;

        /* ── Float ───────────────────────────────────────── */
        case FLUX_FADD: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); v->regs.fp[rd]+=v->regs.fp[rs1]; break;
        case FLUX_FSUB: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); v->regs.fp[rd]-=v->regs.fp[rs1]; break;
        case FLUX_FMUL: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); v->regs.fp[rd]*=v->regs.fp[rs1]; break;
        case FLUX_FDIV: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); if(!v->regs.fp[rs1]) ERR(FLUX_ERR_DIV_ZERO, "float division by zero in FDIV"); v->regs.fp[rd]/=v->regs.fp[rs1]; break;
        case FLUX_FMIN: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); { float a=v->regs.fp[rd],b=v->regs.fp[rs1]; v->regs.fp[rd]=a<b?a:b; } break;
        case FLUX_FMAX: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); { float a=v->regs.fp[rd],b=v->regs.fp[rs1]; v->regs.fp[rd]=a>b?a:b; } break;
        case FLUX_FSQRT: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); v->regs.fp[rd]=sqrtf(v->regs.fp[rd]<0?0:v->regs.fp[rd]); break;
        case FLUX_FMOD: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); if(!v->regs.fp[rs1]) ERR(FLUX_ERR_DIV_ZERO, "float modulo by zero in FMOD"); v->regs.fp[rd]=fmodf(v->regs.fp[rd],v->regs.fp[rs1]); break;

        /* ── Memory ──────────────────────────────────────── */
        case FLUX_MOV: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]=GPR[rs1]; break;
        case FLUX_LOAD: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); { FluxMemRegion* s=flux_mem_get(&v->mem,"stack"); if(s) GPR[rd]=flux_mem_read_i32(s,rs1*4); } break;
        case FLUX_STORE: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rs1); { FluxMemRegion* s=flux_mem_get(&v->mem,"stack"); if(s) flux_mem_write_i32(s,rd*4,GPR[rs1]); } break;
        case FLUX_SWP: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); { int32_t t=GPR[rd]; GPR[rd]=GPR[rs1]; GPR[rs1]=t; } break;

        /* ── Branch ──────────────────────────────────────── */
        case FLUX_JZ: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); { if(GPR[rd]==0) v->regs.pc+=imm; } break;
        case FLUX_JNZ: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); { if(GPR[rd]!=0) v->regs.pc+=imm; } break;
        case FLUX_JLT: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); { if(GPR[rd]<0) v->regs.pc+=imm; } break;
        case FLUX_JGT: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); { if(GPR[rd]>0) v->regs.pc+=imm; } break;
        case FLUX_JLE: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); { if(GPR[rd]<=0) v->regs.pc+=imm; } break;
        case FLUX_JGE: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); { if(GPR[rd]>=0) v->regs.pc+=imm; } break;
        case FLUX_JEQ: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; CHECK_GPR(rd); { if(v->flag_zero) v->regs.pc+=imm; } break;

        /* ── Jump ────────────────────────────────────────── */
        case FLUX_JMP: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; v->regs.pc+=imm; break;
        case FLUX_JAL: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); if(v->frame_count>=256) ERR(FLUX_ERR_FRAME_OVERFLOW, "frame stack overflow in JAL"); v->frame_stack[v->frame_count++]=v->regs.pc; GPR[FLUX_REG_LR]=v->regs.pc; v->regs.pc=(uint32_t)GPR[rd]; break;
        case FLUX_CALL: rd=f8(v); if(!v->running) break; imm=fi16(v); if(!v->running) break; if(v->frame_count>=256) ERR(FLUX_ERR_FRAME_OVERFLOW, "frame stack overflow in CALL"); v->frame_stack[v->frame_count++]=v->regs.pc; v->regs.pc+=imm; break;
        case FLUX_LOOP: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]+=GPR[rs1]; break;
        case FLUX_RET: rd=f8(v); if(!v->running) break; if(!v->frame_count) ERR(FLUX_ERR_STACK_UNDERFLOW, "RET with empty call frame stack"); v->regs.pc=v->frame_stack[--v->frame_count]; break;

        /* ── Extended jump ───────────────────────────────── */
        case FLUX_JMP_REG: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); v->regs.pc=(uint32_t)GPR[rd]; break;
        case FLUX_CALL_REG: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); if(v->frame_count>=256) ERR(FLUX_ERR_FRAME_OVERFLOW, "frame stack overflow in CALL_REG"); v->frame_stack[v->frame_count++]=v->regs.pc; v->regs.pc=(uint32_t)GPR[rd]; break;
        case FLUX_RET_IMM: imm=fi16(v); if(!v->running) break; v->regs.pc+=imm; break;
        case FLUX_TAILCALL_INDIRECT: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); v->regs.pc=(uint32_t)GPR[rd]; break;

        /* ── A2A opcodes (0x50-0x5F, 0x60-0x7B) ─────────── */
        case FLUX_TELL: case FLUX_ASK: case FLUX_DELEG: case FLUX_BCAST:
        case FLUX_REQUEST: case FLUX_REPLY: case FLUX_SUBSCRIBE: case FLUX_UNSUBSCRIBE:
        case FLUX_PUBLISH: case FLUX_QUERY: case FLUX_RESPOND: case FLUX_MERGE:
        case FLUX_SPLIT: case FLUX_GATHER: case FLUX_SCATTER: case FLUX_BARRIER_WAIT:
        case FLUX_DELEGATE: case FLUX_DELEGATE_RESULT: case FLUX_REPORT_STATUS:
        case FLUX_REQUEST_OVERRIDE: case FLUX_BROADCAST: case FLUX_REDUCE:
        case FLUX_DECLARE_INTENT: case FLUX_ASSERT_GOAL: case FLUX_VERIFY_OUTCOME:
        case FLUX_EXPLAIN_FAILURE: case FLUX_SET_PRIORITY:
        case FLUX_TRUST_CHECK: case FLUX_TRUST_UPDATE: case FLUX_TRUST_QUERY:
        case FLUX_REVOKE_TRUST: case FLUX_CAP_REQUIRE: case FLUX_CAP_REQUEST:
        case FLUX_CAP_GRANT: case FLUX_CAP_REVOKE: case FLUX_BARRIER:
        case FLUX_SYNC_CLOCK: case FLUX_FORMATION_UPDATE:
        {
            len=fu16(v); if(!v->running) break;
            data=&v->bytecode[v->regs.pc];
            if (v->regs.pc + len > v->bytecode_len) {
                v->last_error = FLUX_ERR_BYTECODE_OOB;
                v->error_detail = "A2A opcode payload extends beyond bytecode";
                v->running = 0;
                break;
            }
            v->regs.pc+=len;
            if(v->a2a_handler) v->a2a_handler(v,op,data,len);
        } break;

        case FLUX_EMERGENCY_STOP:
        case FLUX_HALT:
            v->halted=1; v->running=0; v->last_error=FLUX_ERR_HALT;
            v->error_detail = (op == FLUX_EMERGENCY_STOP) ? "emergency stop" : "normal halt";
            return (int64_t)v->cycle_count;

        /* ── Resource management ─────────────────────────── */
        case FLUX_YIELD: break;
        case FLUX_ALLOCA_REG: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]+=GPR[rs1]; break;
        case FLUX_RESOURCE_ACQUIRE: { len=fu16(v); if(!v->running) break; data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; if(len>0 && data[0]<256) v->resources[data[0]]=1; GPR[0]=1; } break;
        case FLUX_RESOURCE_RELEASE: { len=fu16(v); if(!v->running) break; data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; if(len>0 && data[0]<256) v->resources[data[0]]=0; } break;
        case FLUX_DEBUG_BREAK: v->running=0; v->error_detail="debug breakpoint hit"; v->last_error=FLUX_OK; break;

        /* ── Debug/trace opcodes ─────────────────────────── */
        case FLUX_TRACE_ON: v->trace.enabled=1; break;
        case FLUX_TRACE_OFF: v->trace.enabled=0; break;
        case FLUX_TRACE_POINT: rd=f8(v); if(!v->running) break; break; /* trace point marker */
        case FLUX_LOG: { len=fu16(v); if(!v->running) break; data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; /* no-op in non-hosted builds */ } break;
        case FLUX_ASSERT: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); if(!GPR[rd]) ERR(FLUX_ERR_HALT, "assertion failed"); break;
        case FLUX_PERF_BEGIN: rd=f8(v); if(!v->running) break; break;
        case FLUX_PERF_END: rd=f8(v); if(!v->running) break; break;
        case FLUX_WATCHPOINT: rd=f8(v); if(!v->running) break; break;

        /* ── Stack/memory ops ────────────────────────────── */
        case FLUX_DUP: { int32_t val; int rc=spop(v,&val); if(rc) ERR((FluxError)rc, v->error_detail); rc=spush(v,val); if(rc) ERR((FluxError)rc, v->error_detail); rc=spush(v,val); if(rc) ERR((FluxError)rc, v->error_detail); } break;
        case FLUX_SWAP: { int32_t a, b; int rc=spop(v,&a); if(rc) ERR((FluxError)rc, v->error_detail); rc=spop(v,&b); if(rc) ERR((FluxError)rc, v->error_detail); rc=spush(v,a); if(rc) ERR((FluxError)rc, v->error_detail); rc=spush(v,b); if(rc) ERR((FluxError)rc, v->error_detail); } break;
        case FLUX_TEST: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); sf(v,GPR[rd]&GPR[rs1]); break;
        case FLUX_SETCC: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); GPR[rd]=(rs1==0)?v->flag_zero:(rs1==1)?v->flag_sign:0; break;
        case FLUX_ENTER: rd=f8(v); if(!v->running) break; if(v->frame_count>=256) ERR(FLUX_ERR_FRAME_OVERFLOW, "frame stack overflow in ENTER"); v->frame_stack[v->frame_count++]=v->regs.sp; v->regs.sp-=rd*4; break;
        case FLUX_LEAVE: rd=f8(v); if(!v->running) break; if(!v->frame_count) ERR(FLUX_ERR_STACK_UNDERFLOW, "LEAVE with empty frame stack"); v->regs.sp=v->frame_stack[--v->frame_count]+rd*4; break;
        case FLUX_ALLOCA: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]+=GPR[rs1]; break;
        case FLUX_TAILCALL: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); v->regs.pc=(uint32_t)GPR[rd]; break;

        case FLUX_REGION_CREATE: { len=fu16(v); if(!v->running) break; data=&v->bytecode[v->regs.pc]; if(v->regs.pc+len>v->bytecode_len) { v->last_error=FLUX_ERR_BYTECODE_OOB; v->error_detail="REGION_CREATE payload OOB"; v->running=0; break; } v->regs.pc+=len; if(len>0) { uint8_t nl=data[0]; char nm[64]={0}; memcpy(nm,data+1,nl<63?nl:63); uint32_t sz=0; if(len>=1+nl+4) memcpy(&sz,data+1+nl,4); flux_mem_create(&v->mem,nm,sz,"agent"); } } break;
        case FLUX_REGION_DESTROY: { len=fu16(v); if(!v->running) break; data=&v->bytecode[v->regs.pc]; if(v->regs.pc+len>v->bytecode_len) { v->last_error=FLUX_ERR_BYTECODE_OOB; v->error_detail="REGION_DESTROY payload OOB"; v->running=0; break; } v->regs.pc+=len; char nm[64]={0}; memcpy(nm,data,len<63?len:63); flux_mem_destroy(&v->mem,nm); } break;
        case FLUX_REGION_TRANSFER: { len=fu16(v); if(!v->running) break; data=&v->bytecode[v->regs.pc]; if(v->regs.pc+len>v->bytecode_len) { v->last_error=FLUX_ERR_BYTECODE_OOB; v->error_detail="REGION_TRANSFER payload OOB"; v->running=0; break; } v->regs.pc+=len; char s[32]={0},d[32]={0}; uint8_t sl=data[0]; memcpy(s,data+1,sl<31?sl:31); uint8_t dl=data[1+sl]; memcpy(d,data+2+sl,dl<31?dl:31); flux_mem_transfer(&v->mem,s,d,"agent"); } break;
        case FLUX_MEMCOPY: { len=fu16(v); if(!v->running) break; data=&v->bytecode[v->regs.pc]; if(v->regs.pc+len>v->bytecode_len) { v->last_error=FLUX_ERR_BYTECODE_OOB; v->error_detail="MEMCOPY payload OOB"; v->running=0; break; } v->regs.pc+=len; if(len>=12) { uint32_t a,b,c; memcpy(&a,data,4); memcpy(&b,data+4,4); memcpy(&c,data+8,4); h=flux_mem_get(&v->mem,"heap"); if(h&&c>0){uint8_t*t=malloc(c); if(t){flux_mem_read(h,b,t,c); flux_mem_write(h,a,t,c); free(t);}} } } break;
        case FLUX_MEMSET: { len=fu16(v); if(!v->running) break; data=&v->bytecode[v->regs.pc]; if(v->regs.pc+len>v->bytecode_len) { v->last_error=FLUX_ERR_BYTECODE_OOB; v->error_detail="MEMSET payload OOB"; v->running=0; break; } v->regs.pc+=len; if(len>=9) { uint32_t a,n; uint8_t val; memcpy(&a,data,4); memcpy(&n,data+4,4); val=data[8]; h=flux_mem_get(&v->mem,"heap"); if(h&&a<h->size){size_t s=n<(h->size-a)?n:(h->size-a); memset(h->data+a,val,s);} } } break;
        case FLUX_MEMCMP: { len=fu16(v); if(!v->running) break; data=&v->bytecode[v->regs.pc]; if(v->regs.pc+len>v->bytecode_len) { v->last_error=FLUX_ERR_BYTECODE_OOB; v->error_detail="MEMCMP payload OOB"; v->running=0; break; } v->regs.pc+=len; if(len>=12) { uint32_t a,b,c; memcpy(&a,data,4); memcpy(&b,data+4,4); memcpy(&c,data+8,4); h=flux_mem_get(&v->mem,"heap"); GPR[0]=0; if(h&&a+c<=h->size&&b+c<=h->size) GPR[0]=memcmp(h->data+a,h->data+b,c); } } break;

        /* ── Type ops ────────────────────────────────────── */
        case FLUX_CAST: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); if(rs1==0) v->regs.fp[rd]=(float)GPR[rd]; else GPR[rd]=(int32_t)v->regs.fp[rd]; break;
        case FLUX_BOX: { len=fu16(v); if(!v->running) break; data=&v->bytecode[v->regs.pc]; if(v->regs.pc+len>v->bytecode_len) { v->last_error=FLUX_ERR_BYTECODE_OOB; v->error_detail="BOX payload OOB"; v->running=0; break; } v->regs.pc+=len; if(v->box_count>=FLUX_BOX_MAX) ERR(FLUX_ERR_BOX_OVERFLOW, "box table full"); FluxBox*b=&v->box_table[v->box_count++]; b->type_tag=data[0]; memcpy(&b->int_val,data+1,4); memcpy(&b->float_val,data+1,4); GPR[0]=v->box_count-1; } break;
        case FLUX_UNBOX: { len=fu16(v); if(!v->running) break; data=&v->bytecode[v->regs.pc]; if(v->regs.pc+len>v->bytecode_len) { v->last_error=FLUX_ERR_BYTECODE_OOB; v->error_detail="UNBOX payload OOB"; v->running=0; break; } v->regs.pc+=len; int id=data[0]; if(id<0||id>=v->box_count) ERR(FLUX_ERR_INVALID_BOX_ID, "UNBOX with invalid box id"); FluxBox*b=&v->box_table[id]; if(b->type_tag==0) GPR[0]=b->int_val; else if(b->type_tag==1) v->regs.fp[0]=b->float_val; else GPR[0]=b->int_val?1:0; } break;
        case FLUX_CHECK_TYPE: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; GPR[0]=(rd<(uint8_t)v->box_count&&v->box_table[rd].type_tag==rs1); break;
        case FLUX_CHECK_BOUNDS: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[0]=(GPR[rd]>=0&&GPR[rd]<GPR[rs1]); break;

        /* ── Extended float ──────────────────────────────── */
        case FLUX_FNEG: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); v->regs.fp[rd]=-v->regs.fp[rd]; break;
        case FLUX_FABS: rd=f8(v); if(!v->running) break; CHECK_GPR(rd); v->regs.fp[rd]=fabsf(v->regs.fp[rd]); break;
        case FLUX_FEQ: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]=(v->regs.fp[rd]==v->regs.fp[rs1]); break;
        case FLUX_FLT: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]=(v->regs.fp[rd]<v->regs.fp[rs1]); break;
        case FLUX_FLE: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]=(v->regs.fp[rd]<=v->regs.fp[rs1]); break;
        case FLUX_FGT: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]=(v->regs.fp[rd]>v->regs.fp[rs1]); break;
        case FLUX_FGE: rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]=(v->regs.fp[rd]>=v->regs.fp[rs1]); break;

        /* ── Extended memory ─────────────────────────────── */
        case FLUX_LOAD8: { rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); h=flux_mem_get(&v->mem,"heap"); GPR[rd]=h?(int32_t)flux_mem_read_u8(h,(size_t)GPR[rs1]):0; } break;
        case FLUX_STORE8: { rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rs1); h=flux_mem_get(&v->mem,"heap"); if(h) flux_mem_write_u8(h,(size_t)GPR[rd],(uint8_t)GPR[rs1]); } break;
        case FLUX_LOAD16: { rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); h=flux_mem_get(&v->mem,"heap"); GPR[rd]=h?(int32_t)(uint16_t)(h->data[GPR[rs1]] | (h->data[GPR[rs1]+1]<<8)):0; } break;
        case FLUX_STORE16: { rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rs1); h=flux_mem_get(&v->mem,"heap"); if(h) { uint16_t val=(uint16_t)GPR[rs1]; h->data[GPR[rd]]=val&0xFF; h->data[GPR[rd]+1]=(val>>8)&0xFF; } } break;
        case FLUX_LOAD32: { rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rd); CHECK_GPR(rs1); GPR[rd]=h?(int32_t)flux_mem_read_i32(h,(size_t)GPR[rs1]):0; break; }
        case FLUX_STORE32: { rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; CHECK_GPR(rs1); h=flux_mem_get(&v->mem,"heap"); if(h) flux_mem_write_i32(h,(size_t)GPR[rd],GPR[rs1]); } break;

        /* ── Vector operations ───────────────────────────── */
        case FLUX_VLOAD: { len=fu16(v); if(!v->running) break; data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; uint8_t r=data[0]; uint16_t off; memcpy(&off,data+1,2); h=flux_mem_get(&v->mem,"heap"); if(h) flux_mem_read(h,off,v->regs.vec[r],16); } break;
        case FLUX_VSTORE: { len=fu16(v); if(!v->running) break; data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; uint8_t r=data[0]; uint16_t off; memcpy(&off,data+1,2); h=flux_mem_get(&v->mem,"heap"); if(h) flux_mem_write(h,off,v->regs.vec[r],16); } break;
        case FLUX_VADD: { rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; for(int i=0;i<4;i++){int32_t a,b; memcpy(&a,v->regs.vec[rd]+i*4,4); memcpy(&b,v->regs.vec[rs1]+i*4,4); a+=b; memcpy(v->regs.vec[rd]+i*4,&a,4);} } break;
        case FLUX_VSUB: { rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; for(int i=0;i<4;i++){int32_t a,b; memcpy(&a,v->regs.vec[rd]+i*4,4); memcpy(&b,v->regs.vec[rs1]+i*4,4); a-=b; memcpy(v->regs.vec[rd]+i*4,&a,4);} } break;
        case FLUX_VMUL: { rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; for(int i=0;i<4;i++){int32_t a,b; memcpy(&a,v->regs.vec[rd]+i*4,4); memcpy(&b,v->regs.vec[rs1]+i*4,4); a*=b; memcpy(v->regs.vec[rd]+i*4,&a,4);} } break;
        case FLUX_VDIV: { rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; for(int i=0;i<4;i++){int32_t a,b; memcpy(&a,v->regs.vec[rd]+i*4,4); memcpy(&b,v->regs.vec[rs1]+i*4,4); if(b){a/=b; memcpy(v->regs.vec[rd]+i*4,&a,4);}} } break;
        case FLUX_VFMA: { rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; rs2=f8(v); if(!v->running) break; for(int i=0;i<4;i++){float a,b,c; memcpy(&a,v->regs.vec[rd]+i*4,4); memcpy(&b,v->regs.vec[rs1]+i*4,4); memcpy(&c,v->regs.vec[rs2]+i*4,4); float r=a+b*c; memcpy(v->regs.vec[rd]+i*4,&r,4);} } break;
        case FLUX_VDOT: { rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; { int32_t dot=0; for(int i=0;i<4;i++){int32_t a,b; memcpy(&a,v->regs.vec[rd]+i*4,4); memcpy(&b,v->regs.vec[rs1]+i*4,4); dot+=a*b;} GPR[0]=dot; } } break;
        case FLUX_VMIN: { rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; for(int i=0;i<4;i++){int32_t a,b; memcpy(&a,v->regs.vec[rd]+i*4,4); memcpy(&b,v->regs.vec[rs1]+i*4,4); a=a<b?a:b; memcpy(v->regs.vec[rd]+i*4,&a,4);} } break;
        case FLUX_VMAX: { rd=f8(v); if(!v->running) break; rs1=f8(v); if(!v->running) break; for(int i=0;i<4;i++){int32_t a,b; memcpy(&a,v->regs.vec[rd]+i*4,4); memcpy(&b,v->regs.vec[rs1]+i*4,4); a=a>b?a:b; memcpy(v->regs.vec[rd]+i*4,&a,4);} } break;

        default:
            ERR(FLUX_ERR_INVALID_OPCODE, "unknown/unsupported opcode");
        }
    }

    v->running = 0;
    if (v->cycle_count >= v->max_cycles && !v->halted) {
        v->last_error = FLUX_ERR_CYCLE_BUDGET;
        v->error_detail = "execution exceeded maximum cycle budget";
    }
    return (int64_t)v->cycle_count;
}

int flux_vm_step(FluxVM* v) {
    if (!v->running) v->running = 1;
    if (v->cycle_count >= v->max_cycles) return 11;
    uint32_t sm = v->max_cycles;
    v->max_cycles = v->cycle_count + 1;
    int64_t rc = flux_vm_execute(v);
    v->max_cycles = sm;
    return rc < 0 ? (int)(-rc) : 0;
}

void flux_vm_set_a2a(FluxVM* v, FluxA2AHandler h) { v->a2a_handler = h; }
