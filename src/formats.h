#ifndef FLUX_FORMATS_H
#define FLUX_FORMATS_H

/*
 * FLUX Unified ISA — FORMAT_A-G Encoding Reference
 * 
 * Format A: 1 byte  [op]
 * Format B: 2 bytes [op][rd]
 * Format C: 2 bytes [op][imm8]
 * Format D: 3 bytes [op][rd][imm8]
 * Format E: 4 bytes [op][rd][rs1][rs2]
 * Format F: 4 bytes [op][rd][imm16hi][imm16lo]
 * Format G: 5 bytes [op][rd][rs1][imm16hi][imm16lo]
 *
 * JetsonClaw1 ports this to C against the Python reference.
 */

// ── System Control (Format A) ──────────────────────────────────
#define OP_HALT      0x00
#define OP_NOP       0x01
#define OP_RET       0x02
#define OP_IRET      0x03
#define OP_BRK       0x04
#define OP_WFI       0x05
#define OP_RESET     0x06
#define OP_SYN       0x07

// ── Single Register (Format B) ─────────────────────────────────
#define OP_INC       0x08
#define OP_DEC       0x09
#define OP_NOT       0x0A
#define OP_NEG       0x0B
#define OP_PUSH      0x0C
#define OP_POP       0x0D
#define OP_CONF_LD   0x0E
#define OP_CONF_ST   0x0F

// ── Immediate Only (Format C) ──────────────────────────────────
#define OP_SYS       0x10
#define OP_TRAP      0x11
#define OP_DBG       0x12
#define OP_CLF       0x13
#define OP_SEMA      0x14
#define OP_YIELD     0x15
#define OP_CACHE     0x16
#define OP_STRIPCONF 0x17

// ── Register + Imm8 (Format D) ─────────────────────────────────
#define OP_MOVI      0x18
#define OP_ADDI      0x19
#define OP_SUBI      0x1A
#define OP_ANDI      0x1B
#define OP_ORI       0x1C
#define OP_XORI      0x1D
#define OP_SHLI      0x1E
#define OP_SHRI      0x1F

// ── Integer Arithmetic (Format E) ──────────────────────────────
#define OP_ADD       0x20
#define OP_SUB       0x21
#define OP_MUL       0x22
#define OP_DIV       0x23
#define OP_MOD       0x24
#define OP_AND       0x25
#define OP_OR        0x26
#define OP_XOR       0x27
#define OP_SHL       0x28
#define OP_SHR       0x29
#define OP_MIN       0x2A
#define OP_MAX       0x2B
#define OP_CMP_EQ    0x2C
#define OP_CMP_LT    0x2D
#define OP_CMP_GT    0x2E
#define OP_CMP_NE    0x2F

// ── Float, Memory, Control (Format E) ──────────────────────────
#define OP_FADD      0x30
#define OP_FSUB      0x31
#define OP_FMUL      0x32
#define OP_FDIV      0x33
#define OP_FMIN      0x34
#define OP_FMAX      0x35
#define OP_FTOI      0x36
#define OP_ITOF      0x37
#define OP_LOAD      0x38
#define OP_STORE     0x39
#define OP_MOV       0x3A
#define OP_SWP       0x3B
#define OP_JZ        0x3C
#define OP_JNZ       0x3D
#define OP_JLT       0x3E
#define OP_JGT       0x3F

// ── Register + Imm16 (Format F) ────────────────────────────────
#define OP_MOVI16    0x40
#define OP_ADDI16    0x41
#define OP_SUBI16    0x42
#define OP_JMP       0x43
#define OP_JAL       0x44
#define OP_CALL      0x45
#define OP_LOOP      0x46
#define OP_SELECT    0x47

// ── Register + Register + Imm16 (Format G) ─────────────────────
#define OP_LOADOFF   0x48
#define OP_STOREOF   0x49
#define OP_LOADI     0x4A
#define OP_STOREI    0x4B
#define OP_ENTER     0x4C
#define OP_LEAVE     0x4D
#define OP_COPY      0x4E
#define OP_FILL      0x4F

// ── A2A Fleet Ops (Format E) ───────────────────────────────────
#define OP_TELL      0x50
#define OP_ASK       0x51
#define OP_DELEG     0x52
#define OP_BCAST     0x53
#define OP_ACCEPT    0x54
#define OP_DECLINE   0x55
#define OP_REPORT    0x56
#define OP_MERGE     0x57
#define OP_FORK      0x58
#define OP_JOIN      0x59
#define OP_SIGNAL    0x5A
#define OP_AWAIT     0x5B
#define OP_TRUST     0x5C
#define OP_DISCOV    0x5D
#define OP_STATUS    0x5E
#define OP_HEARTBT   0x5F

// ── Confidence Variants (Format E) ─────────────────────────────
#define OP_C_ADD     0x60
#define OP_C_SUB     0x61
#define OP_C_MUL     0x62
#define OP_C_DIV     0x63
#define OP_C_FADD    0x64
#define OP_C_FSUB    0x65
#define OP_C_FMUL    0x66
#define OP_C_FDIV    0x67
#define OP_C_MERGE   0x68
#define OP_C_THRESH  0x69
#define OP_C_BOOST   0x6A
#define OP_C_DECAY   0x6B
#define OP_C_SOURCE  0x6C
#define OP_C_CALIB   0x6D
#define OP_C_EXPLY   0x6E
#define OP_C_VOTE    0x6F

// ── Viewpoint Ops — Babel Reserved (Format E) ──────────────────
#define OP_V_EVID    0x70
// ... 0x71-0x7F reserved for Babel's 16 viewpoint ops

// ── Extended System (Format A) ─────────────────────────────────
#define OP_HALT_ERR  0xF0
#define OP_DUMP      0xF2
#define OP_ASSERT    0xF3
#define OP_ID        0xF4
#define OP_VER       0xF5
#define OP_ILLEGAL   0xFF

// ── VM Configuration ───────────────────────────────────────────
#define FLUX_NUM_REGS       64
#define FLUX_CONF_REGS      64
#define FLUX_STACK_SIZE     4096
#define FLUX_MEMORY_SIZE    (64 * 1024)
#define FLUX_MAX_BYTECODE   (1024 * 1024)

// ISA version
#define FLUX_ISA_VERSION    2

#endif // FLUX_FORMATS_H
