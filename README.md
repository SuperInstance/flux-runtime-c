# flux-runtime-c

FLUX Runtime — a C11 micro-VM (Fluid Language Universal eXecution) providing zero-dependency, portable bytecode execution for agent-based systems. Compiles cleanly on ARM64 (Jetson), x86-64, and any C11 platform. The VM is the execution engine that powers agent instruction processing across the Cocapn fleet.

## Brand Line

> FLUX is the mitochondrion made manifest — bytecode is DNA, opcodes are enzymes, fetch-decode-execute is cellular metabolism. Each agent instinct maps to an opcode group.

## Installation

```bash
gcc -std=c11 -Wall -O2 -Iinclude -o flux-runtime src/*.c -lm
# or
make && make test
```

## Usage

```bash
./flux-runtime bytecode.bin
```

Quick start with test suite:

```bash
make
make test    # 27 tests
./flux-runtime bytecode.bin
```

## Architecture

- **85 opcodes**: arithmetic, bitwise, comparison, stack, control flow, SIMD, A2A agent protocol, system calls
- **64-register file**: R0–R15 (int32), F0–F15 (float), V0–V15 (128-bit SIMD vectors)
- **Linear region memory** with ownership semantics (capability-based)
- **A2A protocol opcodes**: TELL, ASK, DELEGATE, BROADCAST, TRUST, CAPABILITY, BARRIER
- **Boxed values**: type-tagged dynamic values (int, float, bool)
- **6 instruction formats**: A (1B), B (2B), C (3B), D (4B + i16), E (4B 3-reg), G (variable)

## Opcodes

| Range | Category | Examples |
|-------|----------|----------|
| 0x00–0x07 | Control | NOP, MOV, LOAD, STORE, JMP, JZ, JNZ, CALL |
| 0x08–0x0F | Int Arith | IADD, ISUB, IMUL, IDIV, IMOD, INEG, INC, DEC |
| 0x10–0x17 | Bitwise | IAND, IOR, IXOR, INOT, ISHL, ISHR, ROTL, ROTR |
| 0x18–0x1F | Compare | ICMP, IEQ, ILT, ILE, IGT, IGE, TEST, SETCC |
| 0x20–0x27 | Stack | PUSH, POP, DUP, SWAP, ROT, ENTER, LEAVE, ALLOCA |
| 0x28–0x2F | Function | RET, CALL_IND, TAILCALL, MOVI, IREM, CMP, JE, JNE |
| 0x30–0x37 | Memory | REGION_CREATE/DESTROY/TRANSFER, MEMCOPY/SET/CMP |
| 0x38–0x3F | Type | CAST, BOX, UNBOX, CHECK_TYPE, CHECK_BOUNDS |
| 0x40–0x4F | Float/SIMD | FADD..FGE, VLOAD/VSTORE/VADD/VSUB/VMUL/VDIV/VFMA |
| 0x60–0x7B | A2A | TELL, ASK, DELEGATE, BROADCAST, TRUST, CAP, BARRIER |
| 0x80–0x84 | System | HALT, YIELD, RESOURCE_ACQUIRE/RELEASE, DEBUG_BREAK |

## Connection to cuda-genepool

Each instinct in [cuda-genepool](https://github.com/Lucineer/cuda-genepool) maps to an opcode group:

- **Survive** → HALT, TRAP, RESOURCE_ACQUIRE
- **Perceive** → IO_READ, LOAD, CMP, TEST
- **Navigate** → JMP, JE, JNE, CALL, RET
- **Communicate** → A2A_SEND, A2A_BROADCAST, IO_WRITE
- **Learn** → BOX, UNBOX, MOV, REGION_CREATE
- **Defend** → REGION_GUARD, VERIFY, CAP_REQUIRE

## Fleet Context

Part of the Cocapn fleet. Related repos:

- [flux-runtime](https://github.com/SuperInstance/flux-runtime) — Python predecessor to this C implementation
- [cuda-genepool](https://github.com/Lucineer/cuda-genepool) — GPU-native agent evolution where FLUX opcodes map to CUDA instincts
- [flux](https://github.com/SuperInstance/flux) — The broader FLUX language and toolchain
- [plato-sdk](https://github.com/SuperInstance/plato-sdk) — SDK for PLATO room-based agent coordination

---
🦐 Cocapn fleet — lighthouse keeper architecture