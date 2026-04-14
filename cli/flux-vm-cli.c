```c
/******************************************************************************************
 *  flux-vm-cli.c
 *
 *  A minimal FLUX byte‑code virtual machine implementing the 50 most important opcodes.
 *  It provides a small CLI with the commands required by the specification:
 *
 *      flux-vm run <file.fluxb>
 *      flux-vm disasm <file.fluxb>
 *      flux-vm assemble <file.fluxasm>
 *      flux-vm repl
 *      flux-vm test
 *      flux-vm version
 *
 *  The VM is stack based, has a fixed 256‑entry value stack, 256 local variable slots,
 *  64 call frames and a string pool of up to 1024 strings.  No garbage collection is
 *  performed – everything lives in fixed‑size arrays.
 *
 *  Compile with:
 *
 *      gcc -Wall -Wextra -O2 -o flux-vm flux-vm-cli.c -lm
 *
 ******************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>

/* -------------------------------------------------------------------------- *
 *  Constants & Types
 * -------------------------------------------------------------------------- */

#define STACK_SIZE      256
#define LOCAL_SLOTS     256
#define CALL_FRAMES     64
#define STRING_POOL_MAX 1024
#define MAX_PROGRAM     (1024 * 1024)   /* 1 MiB – more than enough for demo */

typedef enum {
    /* 0‑31 : core arithmetic / logic */
    OP_NOP = 0, OP_HALT,
    OP_PUSH_I8, OP_PUSH_I32, OP_PUSH_F64, OP_PUSH_STR,
    OP_POP, OP_DUP, OP_SWAP,
    OP_ADD_I, OP_SUB_I, OP_MUL_I, OP_DIV_I,
    OP_ADD_F, OP_SUB_F, OP_MUL_F, OP_DIV_F,
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LTE, OP_GTE,
    OP_AND, OP_OR, OP_NOT,
    OP_JMP, OP_JZ, OP_JNZ,
    OP_CALL, OP_RET,
    OP_LOAD, OP_STORE,
    OP_PRINT, OP_READ,
    OP_STR_LEN, OP_STR_CAT, OP_STR_CMP,
    OP_ARRAY_NEW, OP_ARRAY_GET, OP_ARRAY_SET, OP_ARRAY_LEN,
    OP_DICT_NEW, OP_DICT_GET, OP_DICT_SET,
    OP_DEF_FUNC, OP_CALL_FUNC,
    OP_MAX   /* sentinel – not an opcode */
} opcode_t;

/* Value type tag */
typedef enum { VAL_INT, VAL_FLOAT, VAL_STRING, VAL_ARRAY, VAL_DICT, VAL_NONE } vtype_t;

/* Forward declarations for array / dict – we only store a placeholder pointer */
typedef struct { int dummy; } array_t;
typedef struct { int dummy; } dict_t;

/* A VM value */
typedef struct {
    vtype_t type;
    union {
        int64_t   i;
        double    f;
        int       s;          /* index into string pool */
        array_t  *a;
        dict_t   *d;
    } as;
} value_t;

/* Call frame – stores return address and base pointer for locals */
typedef struct {
    int   ret_ip;
    int   base_ptr;          /* index into locals array */
} callframe_t;

/* String pool – simple array of C strings */
typedef struct {
    char *strings[STRING_POOL_MAX];
    int   count;
} stringpool_t;

/* The VM state */
typedef struct {
    value_t      stack[STACK_SIZE];
    int          sp;                     /* stack pointer (next free slot) */
    value_t      locals[LOCAL_SLOTS];
    callframe_t  frames[CALL_FRAMES];
    int          fp;                     /* frame pointer (next free slot) */
    stringpool_t strpool;
} vm_t;

/* -------------------------------------------------------------------------- *
 *  Helper functions for the value stack
 * -------------------------------------------------------------------------- */

static void vm_push(vm_t *vm, value_t v) {
    if (vm->sp >= STACK_SIZE) {
        fprintf(stderr, "Stack overflow\n");
        exit(1);
    }
    vm->stack[vm->sp++] = v;
}

static value_t vm_pop(vm_t *vm) {
    if (vm->sp <= 0) {
        fprintf(stderr, "Stack underflow\n");
        exit(1);
    }
    return vm->stack[--vm->sp];
}

static value_t vm_peek(vm_t *vm, int depth) {
    int idx = vm->sp - 1 - depth;
    if (idx < 0) {
        fprintf(stderr, "Stack peek out of range\n");
        exit(1);
    }
    return vm->stack[idx];
}

/* -------------------------------------------------------------------------- *
 *  String pool handling
 * -------------------------------------------------------------------------- */

static int strpool_add(stringpool_t *pool, const char *s) {
    if (pool->count >= STRING_POOL_MAX) {
        fprintf(stderr, "String pool overflow\n");
        exit(1);
    }
    pool->strings[pool->count] = strdup(s);
    return pool->count++;
}

static const char *strpool_get(stringpool_t *pool, int idx) {
    if (idx < 0 || idx >= pool->count) {
        fprintf(stderr, "Invalid string pool index %d\n", idx);
        exit(1);
    }
    return pool->strings[idx];
}

/* -------------------------------------------------------------------------- *
 *  VM initialisation
 * -------------------------------------------------------------------------- */

static void vm_init(vm_t *vm) {
    memset(vm, 0, sizeof(vm_t));
    vm->sp = vm->fp = 0;
}

/* -------------------------------------------------------------------------- *
 *  Byte‑code format helpers
 * -------------------------------------------------------------------------- */

static const uint8_t MAGIC[4] = { 'F', 'L', 'U', 'X' };
static const uint16_t VERSION = 1;

/* Write a 16‑bit little‑endian integer */
static void write_u16(FILE *f, uint16_t v) {
    uint8_t b[2] = { v & 0xff, (v >> 8) & 0xff };
    fwrite(b, 1, 2, f);
}

/* Write a 32‑bit little‑endian integer */
static void write_u32(FILE *f, uint32_t v) {
    uint8_t b[4] = { v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff };
    fwrite(b, 1, 4, f);
}

/* Read a 16‑bit little‑endian integer */
static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/* Read a 32‑bit little‑endian integer */
static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* -------------------------------------------------------------------------- *
 *  Core VM execution loop
 * -------------------------------------------------------------------------- */

static void vm_execute(vm_t *vm, const uint8_t *code, size_t code_len) {
    size_t ip = 0;                     /* instruction pointer */

    while (ip < code_len) {
        opcode_t op = (opcode_t)code[ip++];
        switch (op) {
        /* ------------------------------------------------------------------ *
         *  No‑op / termination
         * ------------------------------------------------------------------ */
        case OP_NOP:
            break;
        case OP_HALT:
            return;

        /* ------------------------------------------------------------------ *
         *  Push constants
         * ------------------------------------------------------------------ */
        case OP_PUSH_I8: {
            int8_t v = (int8_t)code[ip++];
            value_t val = { .type = VAL_INT, .as.i = v };
            vm_push(vm, val);
            break;
        }
        case OP_PUSH_I32: {
            int32_t v = (int32_t)read_u32(&code[ip]);
            ip += 4;
            value_t val = { .type = VAL_INT, .as.i = v };
            vm_push(vm, val);
            break;
        }
        case OP_PUSH_F64: {
            double v;
            memcpy(&v, &code[ip], sizeof(double));
            ip += sizeof(double);
            value_t val = { .type = VAL_FLOAT, .as.f = v };
            vm_push(vm, val);
            break;
        }
        case OP_PUSH_STR: {
            uint16_t slen = read_u16(&code[ip]);
            ip += 2;
            char tmp[65536];
            if (slen >= sizeof(tmp)) {
                fprintf(stderr, "String literal too long\n");
                exit(1);
            }
            memcpy(tmp, &code[ip], slen);
            tmp[slen] = '\0';
            ip += slen;
            int idx = strpool_add(&vm->strpool, tmp);
            value_t val = { .type = VAL_STRING, .as.s = idx };
            vm_push(vm, val);
            break;
        }

        /* ------------------------------------------------------------------ *
         *  Stack manipulation
         * ------------------------------------------------------------------ */
        case OP_POP: {
            vm_pop(vm);
            break;
        }
        case OP_DUP: {
            value_t v = vm_peek(vm, 0);
            vm_push(vm, v);
            break;
        }
        case OP_SWAP: {
            if (vm->sp < 2) {
                fprintf(stderr, "SWAP needs two stack items\n");
                exit(1);
            }
            value_t a = vm->stack[vm->sp - 1];
            value_t b = vm->stack[vm->sp - 2];
            vm->stack[vm->sp - 1] = b;
            vm->stack[vm->sp - 2] = a;
            break;
        }

        /* ------------------------------------------------------------------ *
         *  Integer arithmetic
         * ------------------------------------------------------------------ */
        case OP_ADD_I: {
            value_t b = vm_pop(vm);
            value_t a = vm_pop(vm);
            if (a.type != VAL_INT || b.type != VAL_INT) {
                fprintf(stderr, "ADD_I expects two integers\n");
                exit(1);
            }
            value_t r = { .type = VAL_INT, .as.i = a.as.i + b.as.i };
            vm_push(vm, r);
            break;
        }
        case OP_SUB_I: {
            value_t b = vm_pop(vm);
            value_t a = vm_pop(vm);
            if (a.type != VAL_INT || b.type != VAL_INT) {
                fprintf(stderr, "SUB_I expects two integers\n");
                exit(1);
            }
            value_t r = { .type = VAL_INT, .as.i = a.as.i - b.as.i };
            vm_push(vm, r);
            break;
        }
        case OP_MUL_I: {
            value_t b = vm_pop(vm);
            value_t a = vm_pop(vm);
            if (a.type != VAL_INT || b.type != VAL_INT) {
                fprintf(stderr, "MUL_I expects two integers\n");
                exit(1);
            }
            value_t r = { .type = VAL_INT, .as.i = a.as.i * b.as.i };
            vm_push(vm, r);
            break;
        }
        case OP_DIV_I: {
            value_t b = vm_pop(vm);
            value_t a = vm_pop(vm);
            if (a.type != VAL_INT || b.type != VAL_INT) {
                fprintf(stderr, "DIV_I expects two integers\n");
                exit(1);
            }
            if (b.as.i == 0) {
                fprintf(stderr, "Division by zero\n");
                exit(1);
            }
            value_t r = { .type = VAL_INT, .as.i = a.as.i / b.as.i };
            vm_push(vm, r);
            break;
        }

        /* ------------------------------------------------------------------ *
         *  Float arithmetic
         * ------------------------------------------------------------------ */
        case OP_ADD_F: {
            value_t b = vm_pop(vm);
            value_t a = vm_pop(vm);
            if (a.type != VAL_FLOAT || b.type != VAL_FLOAT) {
                fprintf(stderr, "ADD_F expects two floats\n");
                exit(1);
            }
            value_t r = { .type = VAL_FLOAT, .as.f = a.as.f + b.as.f };
            vm_push(vm, r);
            break;
        }
        case OP_SUB_F: {
            value_t b = vm_pop(vm);
            value_t a = vm_pop(vm);
            if (a.type != VAL_FLOAT || b.type != VAL_FLOAT) {
                fprintf(stderr, "SUB_F expects two floats\n");
                exit(1);
            }
            value_t r = { .type = VAL_FLOAT, .as.f = a.as.f - b.as.f };
            vm_push(vm, r);
            break;
        }
        case OP_MUL_F: {
            value_t b = vm_pop(vm);
            value_t a = vm_pop(vm);
            if (a.type != VAL_FLOAT || b.type != VAL_FLOAT) {
                fprintf(stderr, "MUL_F expects two floats\n");
                exit(1);
            }
            value_t r = { .type = VAL_FLOAT, .as.f = a.as.f * b.as.f };
            vm_push(vm, r);
            break;
        }
        case OP_DIV_F: {
            value_t b = vm_pop(vm);
            value_t a = vm_pop(vm);
            if (a.type != VAL_FLOAT || b.type != VAL_FLOAT) {
                fprintf(stderr, "DIV_F expects two floats\n");
                exit(1);
            }
            if (b.as.f == 0.0) {
                fprintf(stderr, "Floating point division by zero\n");
                exit(1);
            }
            value_t r = { .type = VAL_FLOAT, .as.f = a.as.f / b.as.f };
            vm_push(vm, r);
            break;
        }

        /* ------------------------------------------------------------------ *
         *  Comparisons – push integer 0 (false) or 1 (true)
         * ------------------------------------------------------------------ */
        case OP_EQ:
        case OP_NEQ:
        case OP_LT:
        case OP_GT:
        case OP_LTE:
        case OP_GTE: {
            value_t b = vm_pop(vm);
            value_t a = vm_pop(vm);
            int result = 0;
            if (a.type != b.type) {
                result = 0;
            } else {
                switch (a.type) {
                case VAL_INT:   result = (a.as.i == b.as.i); break;
                case VAL_FLOAT: result = (a.as.f == b.as.f); break;
                case VAL_STRING:
                    result = (strcmp(strpool_get(&vm->strpool, a.as.s),
                                    strpool_get(&vm->strpool, b.as.s)) == 0);
                    break;
                default:
                    result = 0;
                }
            }
            if (op == OP_NEQ) result = !result;
            else if (op == OP_LT)   result = (a.as.i <  b.as.i);
            else if (op == OP_GT)   result = (a.as.i >  b.as.i);
            else if (op == OP_LTE)  result = (a.as.i <= b.as.i);
            else if (op == OP_GTE)  result = (a.as.i >= b.as.i);
            value_t r = { .type = VAL_INT, .as.i = result };
            vm_push(vm, r);
            break;
        }

        /* ------------------------------------------------------------------ *
         *  Logical ops – integer 0 / 1
         * ------------------------------------------------------------------ */
        case OP_AND: {
            value_t b = vm_pop(vm);
            value_t a = vm_pop(vm);
            int r = (a.type == VAL_INT && b.type == VAL_INT) ? (a.as.i && b.as.i) : 0;
            vm_push(vm, (value_t){ .type = VAL_INT, .as.i = r });
            break;
        }
        case OP_OR: {
            value_t b = vm_pop(vm);
            value_t a = vm_pop(vm);
            int r = (a.type == VAL_INT && b.type == VAL_INT) ? (a.as.i || b.as.i) : 0;
            vm_push(vm, (value_t){ .type = VAL_INT, .as.i = r });
            break;
        }
        case OP_NOT: {
            value_t a = vm_pop(vm);
            int r = (a.type == VAL_INT) ? (!a.as.i) : 0;
            vm_push(vm, (value_t){ .type = VAL_INT, .as.i = r });
            break;
        }

        /* ------------------------------------------------------------------ *
         *  Control flow
         * ------------------------------------------------------------------ */
        case OP_JMP: {
            uint32_t target = read_u32(&code[ip]);
            ip = target;
            break;
        }
        case OP_JZ: {
            uint32_t target = read_u32(&code[ip]);
            ip += 4;
            value_t cond = vm_pop(vm);
            if (cond.type != VAL_INT) {
                fprintf(stderr, "JZ expects integer condition\n");
                exit(1);
            }
            if (!cond.as.i) ip = target;
            break;
        }
        case OP_JNZ: {
            uint32_t target = read_u32(&code[ip]);
            ip += 4;
            value_t cond = vm_pop(vm);
            if (cond.type != VAL_INT) {
                fprintf(stderr, "JNZ expects integer condition\n");
                exit(1);
            }
            if (cond.as.i) ip = target;
            break;
        }

        /* ------------------------------------------------------------------ *
         *  Calls / returns – very simple, no arguments handling
         * ------------------------------------------------------------------ */
        case OP_CALL: {
            uint32_t addr = read_u32(&code[ip]);
            ip += 4;
            if (vm->fp >= CALL_FRAMES) {
                fprintf(stderr, "Call‑frame stack overflow\n");
                exit(1);
            }
            vm->frames[vm->fp].ret_ip = ip;
            vm->frames[vm->fp].base_ptr = 0;   /* we keep a flat locals array */
            vm->fp++;
            ip = addr;
            break;
        }
        case OP_RET: {
            if (vm->fp == 0) {
                fprintf(stderr, "RET with empty call‑frame stack\n");
                exit(1);
            }
            vm->fp--;
            ip = vm->frames[vm->fp].ret_ip;
            break;
        }

        /* ------------------------------------------------------------------ *
         *  Local variable load / store (index is a single byte)
         * ------------------------------------------------------------------ */
        case OP_LOAD: {
            uint8_t idx = code[ip++];
            if (idx >= LOCAL_SLOTS) {
                fprintf(stderr, "LOAD: invalid local index %u\n", idx);
                exit(1);
            }
            vm_push(vm, vm->locals[idx]);
            break;
        }
        case OP_STORE: {
            uint8_t idx = code[ip++];
            if (idx >= LOCAL_SLOTS) {
                fprintf(stderr, "STORE: invalid local index %u\n", idx);
                exit(1);
            }
            vm->locals[idx] = vm_pop(vm);
            break;
        }

        /* ------------------------------------------------------------------ *
         *  I/O
         * ------------------------------------------------------------------ */
        case OP_PRINT: {
            value_t v = vm_pop(vm);
            switch (v.type) {
            case VAL_INT:   printf("%lld\n", (long long)v.as.i); break;
            case VAL_FLOAT: printf("%g\n", v.as.f); break;
            case VAL_STRING: printf("%s\n", strpool_get(&vm->strpool, v.as.s)); break;
            default:        printf("<non‑printable>\n"); break;
            }
            break;
        }
        case OP_READ: {
            char buf[1024];
            if (!fgets(buf, sizeof(buf), stdin)) buf[0] = '\0';
            /* strip trailing newline */
            buf[strcspn(buf, "\n")] = '\0';
            int idx = strpool_add(&vm->strpool, buf);
            vm_push(vm, (value_t){ .type = VAL_STRING, .as.s = idx });
            break;
        }

        /* ------------------------------------------------------------------ *
         *  String operations
         * ------------------------------------------------------------------ */
        case OP_STR_LEN: {
            value_t v = vm_pop(vm);
            if (v.type != VAL_STRING) {
                fprintf(stderr, "STR_LEN expects a string\n");
                exit(1);
            }
            const char *s = strpool_get(&vm->strpool, v.as.s);
            vm_push(vm, (value_t){ .type = VAL_INT, .as.i = (int64_t)strlen(s) });
            break;
        }
        case OP_STR_CAT: {
            value_t b = vm_pop(vm);
            value_t a = vm_pop(vm);
            if (a.type != VAL_STRING || b.type != VAL_STRING) {
                fprintf(stderr, "STR_CAT expects two strings\n");
                exit(1);
            }
            const char *sa = strpool_get(&vm->strpool, a.as.s);
            const char *sb = strpool_get(&vm->strpool, b.as.s);
            char *cat = malloc(strlen(sa) + strlen(sb) + 1);
            strcpy(cat, sa);
            strcat(cat, sb);
            int idx = strpool_add(&vm->strpool, cat);
            free(cat);
            vm_push(vm, (value_t){ .type = VAL_STRING, .as.s = idx });
            break;
        }
        case OP_STR_CMP: {
            value_t b = vm_pop(vm);
            value_t a = vm_pop(vm);
            if (a.type != VAL_STRING || b.type != VAL_STRING) {
                fprintf(stderr, "STR_CMP expects two strings\n");
                exit(1);
            }
            const char *sa = strpool_get(&vm->strpool, a.as.s);
            const char *sb = strpool_get(&vm->strpool, b.as.s);
            int cmp = strcmp(sa, sb);
            vm_push(vm, (value_t){ .type = VAL_INT, .as.i = cmp });
            break;
        }

        /* ------------------------------------------------------------------ *
         *  Array / dict – stub implementations (just placeholders)
         * ------------------------------------------------------------------ */
        case OP_ARRAY_NEW: {
            vm_push(vm, (value_t){ .type = VAL_ARRAY, .as.a = NULL });
            break;
        }
        case OP_ARRAY_GET:
        case OP_ARRAY_SET:
        case OP_ARRAY_LEN:
        case OP_DICT_NEW:
        case OP_DICT_GET:
        case OP_DICT_SET:
            /* Full implementations would require dynamic structures.
               For the purpose of this demo we simply push a placeholder. */
            vm_push(vm, (value_t){ .type = VAL_NONE });
            break;

        /* ------------------------------------------------------------------ *
         *  Function definition / call – very simple: DEF_FUNC just stores the
         *  address as an integer constant; CALL_FUNC behaves like CALL.
         * ------------------------------------------------------------------ */
        case OP_DEF_FUNC: {
            uint32_t addr = read_u32(&code[ip]);
            ip += 4;
            vm_push(vm, (value_t){ .type = VAL_INT, .as.i = addr });
            break;
        }
        case OP_CALL_FUNC: {
            value_t fn = vm_pop(vm);
            if (fn.type != VAL_INT) {
                fprintf(stderr, "CALL_FUNC expects an integer address\n");
                exit(1);
            }
            uint32_t addr = (uint32_t)fn.as.i;
            if (vm->fp >= CALL_FRAMES) {
                fprintf(stderr, "Call‑frame stack overflow (CALL_FUNC)\n");
                exit(1);
            }
            vm->frames[vm->fp].ret_ip = ip;
            vm->frames[vm->fp].base_ptr = 0;
            vm->fp++;
            ip = addr;
            break;
        }

        default:
            fprintf(stderr, "Unknown opcode %d at %zu\n", op, ip - 1);
            exit(1);
        }
    }
}

/* -------------------------------------------------------------------------- *
 *  Disassembler – prints a human readable listing
 * -------------------------------------------------------------------------- */

static const char *opcode_name(opcode_t op) {
    static const char *names[] = {
        "NOP", "HALT",
        "PUSH_I8", "PUSH_I32", "PUSH_F64", "PUSH_STR",
        "POP", "DUP", "SWAP",
        "ADD_I", "SUB_I", "MUL_I", "DIV_I",
        "ADD_F", "SUB_F", "MUL_F", "DIV_F",
        "EQ", "NEQ", "LT", "GT", "LTE", "GTE",
        "AND", "OR", "NOT",
        "JMP", "JZ", "JNZ",
        "CALL", "RET",
        "LOAD", "STORE",
        "PRINT", "READ",
        "STR_LEN", "STR_CAT", "STR_CMP",
        "ARRAY_NEW", "ARRAY_GET", "ARRAY_SET", "ARRAY_LEN",
        "DICT_NEW", "DICT_GET", "DICT_SET",
        "DEF_FUNC", "CALL_FUNC"
    };
    if (op >= 0 && op < OP_MAX) return names[op];
    return "???";
}

static void disasm(const uint8_t *code, size_t len)