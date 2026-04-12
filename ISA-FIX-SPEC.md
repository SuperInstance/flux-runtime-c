# ISA v2 Convergence for flux-runtime-c

## Problem
This C runtime uses OLD opcode numbering:
- 0x08=IADD, 0x09=ISUB, 0x20=PUSH, 0x40=FADD, etc.

But the unified ISA (isa_unified.py in flux-runtime) uses:
- 0x20=ADD, 0x21=SUB, 0x0C=PUSH, 0x30=FADD, etc.

## Task
1. Read src/opcodes.c to understand current opcode mapping
2. Read src/vm.c to understand the dispatch switch statement
3. Update opcodes.c names[] array to match ISA v2 numbering
4. Update vm.c switch statement to match new opcode numbers
5. Keep existing test compatibility — the tests use the OLD opcodes too, so update tests/test_*.c as needed
6. Build with `make clean && make`
7. Run `./test_vm && ./test_memory && ./test_asm` to verify

## ISA v2 Opcode Table (from isa_unified.py)
```
System:   HALT=0x00, NOP=0x01, RET=0x02, IRET=0x03, BRK=0x04, WFI=0x05, RESET=0x06, SYN=0x07
Single:   INC=0x08, DEC=0x09, NOT=0x0A, NEG=0x0B, PUSH=0x0C, POP=0x0D
Imm:      MOVI=0x18, ADDI=0x19, SUBI=0x1A
Arith:    ADD=0x20, SUB=0x21, MUL=0x22, DIV=0x23, MOD=0x24, AND=0x25, OR=0x26, XOR=0x27, SHL=0x28, SHR=0x29
Compare:  CMP_EQ=0x2C, CMP_LT=0x2D, CMP_GT=0x2E, CMP_NE=0x2F
Float:    FADD=0x30, FSUB=0x31, FMUL=0x32, FDIV=0x33
Mem:      LOAD=0x38, STORE=0x39, MOV=0x3A, SWP=0x3B
Branch:   JZ=0x3C, JNZ=0x3D, JLT=0x3E, JGT=0x3F
Jump:     JMP=0x43, JAL=0x44, CALL=0x45
A2A:      TELL=0x50, ASK=0x51, DELEG=0x52, BCAST=0x53
HALT=0x00 (stop execution)
```

## Critical Notes
- Format E is [opcode][rd][rs1][rs2] = 4 bytes for arithmetic
- Format D is [opcode][rd][imm8] = 3 bytes for immediates
- Format B is [opcode][rd] = 2 bytes for single reg ops
- Format A is [opcode] = 1 byte for system ops
- PUSH/POP are Format B (0x0C/0x0D), NOT Format E (0x20/0x21 old)
- Make sure the enum/defines in header files match

## Verification
After changes: `make clean && make && ./test_vm && ./test_memory && ./test_asm`
