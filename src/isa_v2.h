#ifndef ISA_V2_H
#define ISA_V2_H

#include <stdint.h>
#include <stdbool.h>

// ISA v2: All instructions fixed 4 bytes: [opcode][byte1][byte2][byte3]
#define ISA2_NOP   0x00
#define ISA2_MOV   0x01
#define ISA2_MOVI  0x02
#define ISA2_IADD  0x08
#define ISA2_ISUB  0x09
#define ISA2_IMUL  0x0A
#define ISA2_IDIV  0x0B
#define ISA2_INC   0x0E
#define ISA2_DEC   0x0F
#define ISA2_JNZ   0x06
#define ISA2_JZ    0x2E
#define ISA2_CMP   0x2D
#define ISA2_JMP   0x30
#define ISA2_PUSH  0x31
#define ISA2_POP   0x32
#define ISA2_HALT  0x80

#define ISA2_NUM_REGS 16
#define ISA2_STACK_SIZE 256

typedef struct {
    int32_t gp[ISA2_NUM_REGS];  // R0-R15
    int32_t flags;               // CMP flags: 1 if equal, 0 otherwise
    int32_t stack[ISA2_STACK_SIZE];
    int32_t sp;
    int32_t pc;
    bool halted;
    int32_t cycles;
} ISA2VM;

void isa2_init(ISA2VM* vm);
int32_t isa2_execute(ISA2VM* vm, uint8_t* bytecode, int32_t len);

// Encode helpers
static inline void isa2_encode_movi(uint8_t* buf, int rd, int16_t imm) {
    buf[0] = ISA2_MOVI;
    buf[1] = (uint8_t)rd;
    buf[2] = (uint8_t)(imm & 0xFF);
    buf[3] = (uint8_t)((imm >> 8) & 0xFF);
}

static inline void isa2_encode_alu(uint8_t* buf, uint8_t op, int rd, int rs1, int rs2) {
    buf[0] = op;
    buf[1] = (uint8_t)rd;
    buf[2] = (uint8_t)rs1;
    buf[3] = (uint8_t)rs2;
}

static inline void isa2_encode_jnz(uint8_t* buf, int rd, int16_t off) {
    buf[0] = ISA2_JNZ;
    buf[1] = (uint8_t)rd;
    buf[2] = (uint8_t)(off & 0xFF);
    buf[3] = (uint8_t)((off >> 8) & 0xFF);
}

static inline void isa2_encode_halt(uint8_t* buf) {
    buf[0] = ISA2_HALT; buf[1] = 0; buf[2] = 0; buf[3] = 0;
}

#endif

// Extended opcodes (ISA v2.1)
#define ISA2_CALL  0x40  // CALL addr — push PC+4, jump to addr
#define ISA2_RET   0x41  // RET — pop address, jump to it
#define ISA2_AND   0x42  // AND rd, rs1, rs2
#define ISA2_OR    0x43  // OR rd, rs1, rs2
#define ISA2_XOR   0x44  // XOR rd, rs1, rs2
#define ISA2_NOT   0x45  // NOT rd, rs — bitwise complement
#define ISA2_SHL   0x46  // SHL rd, rs1, rs2 — shift left
#define ISA2_SHR   0x47  // SHR rd, rs1, rs2 — shift right
#define ISA2_IMOD  0x48  // IMOD rd, rs1, rs2 — modulo
#define ISA2_PRINT 0x49  // PRINT rd — print register value
#define ISA2_LOAD  0x4A  // LOAD rd, [rs] — load from memory
#define ISA2_STORE 0x4B  // STORE [rd], rs — store to memory
#define ISA2_DUP   0x4C  // DUP — duplicate top of stack
#define ISA2_SWAP  0x4D  // SWAP rs1, rs2 — swap register values
#define ISA2_NEG   0x4E  // NEG rd, rs — negate
#define ISA2_XCHG  0x4F  // XCHG — exchange top two stack elements
