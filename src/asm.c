#define _POSIX_C_SOURCE 200809L

/*
 * FLUX Assembler v2 — Two-pass assembler with exact size calculation.
 * Pass 1: Calculate exact byte sizes and record label positions.
 * Pass 2: Emit bytecode with correct offsets.
 *
 * Copyright (c) 2026 SuperInstance, MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "flux/opcodes.h"

#define MAX_LINES    4096
#define MAX_LABELS   1024
#define MAX_LABEL_LEN 64
#define MAX_OUTPUT   65536
#define MAX_LINE_LEN 512

typedef struct {
    char name[MAX_LABEL_LEN];
    int  offset;
} Label;

typedef struct {
    char* lines[MAX_LINES];
    int   count;
    Label labels[MAX_LABELS];
    int   label_count;
    uint8_t output[MAX_OUTPUT];
    int   output_pos;
    /* Forward reference fixups */
    int   fixup_pos[MAX_LABELS];   /* byte position of the i16 to patch */
    char  fixup_label[MAX_LABELS][MAX_LABEL_LEN];
    int   fixup_instr_end[MAX_LABELS]; /* byte pos after the jump instruction */
    int   fixup_count;
} Assembler;

static void asm_init(Assembler* a) { memset(a, 0, sizeof(*a)); }
static void asm_free(Assembler* a) { for (int i = 0; i < a->count; i++) free(a->lines[i]); }

static int find_label(Assembler* a, const char* name) {
    for (int i = 0; i < a->label_count; i++)
        if (strcmp(a->labels[i].name, name) == 0) return a->labels[i].offset;
    return -1;
}

static void add_label(Assembler* a, const char* name, int offset) {
    if (a->label_count >= MAX_LABELS) return;
    snprintf(a->labels[a->label_count].name, MAX_LABEL_LEN, "%s", name);
    a->labels[a->label_count].offset = offset;
    a->label_count++;
}

static void emit_u8(Assembler* a, uint8_t b) {
    if (a->output_pos < MAX_OUTPUT) a->output[a->output_pos++] = b;
}

static void emit_i16(Assembler* a, int16_t v) {
    emit_u8(a, (uint8_t)(v & 0xFF));
    emit_u8(a, (uint8_t)((v >> 8) & 0xFF));
}

static int parse_reg(const char* s) {
    if (!s) return -1;
    while (isspace(*s)) s++;
    char* comma = strchr((char*)s, ',');
    char tmp[16];
    if (comma) { int n = comma - s; if (n > 15) n = 15; memcpy(tmp, s, n); tmp[n] = '\0'; s = tmp; }
    if (toupper(s[0]) == 'R') return atoi(s + 1);
    return -1;
}

static int parse_int(const char* s) {
    if (!s) return 0;
    while (isspace(*s)) s++;
    return (int)strtol(s, NULL, 0);
}

/* Strip comments and trailing whitespace */
static void strip_line(char* line) {
    char* c = strchr(line, ';');
    if (c) *c = '\0';
    c = strstr(line, "//");
    if (c) *c = '\0';
    int len = strlen(line);
    while (len > 0 && isspace((unsigned char)line[len-1])) line[--len] = '\0';
}

/* Get clean argument (strip trailing comma, whitespace) */
static void clean_arg(char* buf, const char* src, int bufsize) {
    snprintf(buf, bufsize, "%s", src);
    int len = strlen(buf);
    while (len > 0 && (isspace((unsigned char)buf[len-1]) || buf[len-1] == ',')) buf[--len] = '\0';
}

/* Instruction sizes by format:
 * Format A (1B): NOP, HALT, DUP, YIELD, DBG
 * Format B (2B): INC, DEC, PUSH, POP, INEG, INOT, RET(+2pad)
 * Format C (3B): CMP, LOAD8, STORE8, etc
 * Format D (4B): MOVI, JMP, JZ, JNZ, CALL — opcode + reg/u8 + i16
 * Format E (4B): IADD, ISUB, IMUL, IDIV — opcode + rd + rs1 + rs2
 */

typedef enum { FMT_A=1, FMT_B=2, FMT_C=3, FMT_D=4, FMT_E=4 } FmtSize;

static FmtSize instr_size(const char* op) {
    char u[32]; snprintf(u, sizeof(u), "%s", op);
    for (char* p = u; *p; p++) *p = toupper((unsigned char)*p);

    /* Format A — 1 byte */
    if (!strcmp(u,"HALT") || !strcmp(u,"NOP") || !strcmp(u,"DUP") ||
        !strcmp(u,"YIELD") || !strcmp(u,"DBG") || !strcmp(u,"PRINT"))
        return FMT_A;

    /* Format D — 4 bytes: opcode + reg + i16 */
    if (!strcmp(u,"MOVI") || !strcmp(u,"LDI") || !strcmp(u,"JMP") ||
        !strcmp(u,"JZ") || !strcmp(u,"JE") || !strcmp(u,"JNZ") || !strcmp(u,"JNE") ||
        !strcmp(u,"CALL"))
        return FMT_D;

    /* Format C — 3 bytes: opcode + rd + rs1 (2-operand ALU) */
    if (!strcmp(u,"IADD") || !strcmp(u,"ISUB") || !strcmp(u,"IMUL") || !strcmp(u,"IDIV") ||
        !strcmp(u,"IMOD") || !strcmp(u,"IREM") || !strcmp(u,"IAND") || !strcmp(u,"IOR") ||
        !strcmp(u,"IXOR") || !strcmp(u,"ISHL") || !strcmp(u,"ISHR") || !strcmp(u,"ROTL") ||
        !strcmp(u,"ROTR") || !strcmp(u,"CAST") || !strcmp(u,"FADD") || !strcmp(u,"FSUB") ||
        !strcmp(u,"FMUL") || !strcmp(u,"FDIV"))
        return FMT_C;

    /* Format B — 2 bytes: opcode + reg */
    if (!strcmp(u,"INC") || !strcmp(u,"DEC") || !strcmp(u,"PUSH") || !strcmp(u,"POP") ||
        !strcmp(u,"INEG") || !strcmp(u,"INOT"))
        return FMT_B;

    /* Format C — 3 bytes: opcode + rd + rs1 */
    if (!strcmp(u,"CMP") || !strcmp(u,"ICMP") || !strcmp(u,"LOAD") || !strcmp(u,"STORE") ||
        !strcmp(u,"LOAD8") || !strcmp(u,"STORE8") || !strcmp(u,"TEST"))
        return FMT_C;

    /* Default 4 */
    return FMT_D;
}

/* Parse one line to get: opcode, args. Returns 0 if empty/label-only. */
static int parse_line(char* line, char* op, int opsize, char args[4][64], int* nargs) {
    *op = '\0'; *nargs = 0;
    strip_line(line);
    if (line[0] == '\0') return 0;

    char* colon = strchr(line, ':');
    if (colon) {
        char* rest = colon + 1;
        while (isspace((unsigned char)*rest)) rest++;
        if (*rest == '\0') return 0; /* label-only line */
        memmove(line, rest, strlen(rest) + 1);
    }

    char* tokens[5];
    int ntok = 0;
    char* save = NULL;
    char linecopy[MAX_LINE_LEN];
    snprintf(linecopy, sizeof(linecopy), "%s", line);
    char* tok = strtok_r(linecopy, " \t", &save);
    while (tok && ntok < 5) { tokens[ntok++] = tok; tok = strtok_r(NULL, " \t", &save); }

    if (ntok == 0) return 0;
    snprintf(op, opsize, "%s", tokens[0]);
    /* Merge remaining tokens as args (comma-separated from original line) */
    for (int i = 1; i < ntok && *nargs < 4; i++) {
        snprintf(args[*nargs], 64, "%s", tokens[i]);
        (*nargs)++;
    }
    return 1;
}

/* ====== PASS 1: Calculate exact sizes, record labels ====== */
static void pass1(Assembler* a) {
    int pos = 0;
    for (int i = 0; i < a->count; i++) {
        char line[MAX_LINE_LEN];
        snprintf(line, sizeof(line), "%s", a->lines[i]);
        strip_line(line);
        if (line[0] == '\0') continue;

        /* Check for label */
        char* colon = strchr(line, ':');
        if (colon) {
            /* Extract label name */
            char name[MAX_LABEL_LEN] = {0};
            char* p = line;
            while (isspace((unsigned char)*p)) p++;
            int len = colon - p;
            if (len >= MAX_LABEL_LEN) len = MAX_LABEL_LEN - 1;
            memcpy(name, p, len);
            name[len] = '\0';
            /* Trim trailing spaces from name */
            while (len > 0 && isspace((unsigned char)name[len-1])) name[--len] = '\0';
            add_label(a, name, pos);

            /* Check for code after label */
            char* rest = colon + 1;
            while (isspace((unsigned char)*rest)) rest++;
            if (*rest == '\0') continue;
            /* There's code — fall through to parse it */
            memmove(line, rest, strlen(rest) + 1);
        }

        /* Parse instruction */
        char op[32], args[4][64] = {{0}};
        int nargs = 0;
        if (!parse_line(line, op, sizeof(op), args, &nargs)) continue;

        pos += instr_size(op);
    }
}

/* ====== PASS 2: Emit bytecode ====== */
static int pass2(Assembler* a) {
    for (int i = 0; i < a->count; i++) {
        char line[MAX_LINE_LEN];
        snprintf(line, sizeof(line), "%s", a->lines[i]);

        char op[32], args[4][64] = {{0}};
        int nargs = 0;
        if (!parse_line(line, op, sizeof(op), args, &nargs)) continue;

        char u[32]; snprintf(u, sizeof(u), "%s", op);
        for (char* p = u; *p; p++) *p = toupper((unsigned char)*p);

        int pos = a->output_pos;
        (void)nargs;

        /* Format A: HALT, NOP, DUP */
        if (!strcmp(u,"HALT"))  { emit_u8(a, FLUX_HALT); }
        else if (!strcmp(u,"NOP"))  { emit_u8(a, FLUX_NOP); }
        else if (!strcmp(u,"DUP"))  { emit_u8(a, FLUX_DUP); }
        else if (!strcmp(u,"DBG") || !strcmp(u,"PRINT")) { emit_u8(a, FLUX_DEBUG_BREAK); }

        /* Format D: MOVI */
        else if (!strcmp(u,"MOVI")) {
            emit_u8(a, FLUX_MOVI);
            emit_u8(a, (uint8_t)parse_reg(args[0]));
            emit_i16(a, (int16_t)parse_int(args[1]));
        }

        /* Format C: 2-reg ALU (rd = rd OP rs1) — the C VM uses 2-operand format */
        else if (!strcmp(u,"IADD") || !strcmp(u,"ISUB") || !strcmp(u,"IMUL") ||
                 !strcmp(u,"IDIV") || !strcmp(u,"IMOD") || !strcmp(u,"IREM") ||
                 !strcmp(u,"IAND") || !strcmp(u,"IOR") || !strcmp(u,"IXOR") ||
                 !strcmp(u,"ISHL") || !strcmp(u,"ISHR") || !strcmp(u,"ROTL") ||
                 !strcmp(u,"ROTR")) {
            FluxOpcode map[] = {FLUX_IADD,FLUX_ISUB,FLUX_IMUL,FLUX_IDIV,FLUX_IMOD,FLUX_IREM,
                FLUX_IAND,FLUX_IOR,FLUX_IXOR,FLUX_ISHL,FLUX_ISHR,FLUX_ROTL,FLUX_ROTR};
            const char* names[] = {"IADD","ISUB","IMUL","IDIV","IMOD","IREM",
                "IAND","IOR","IXOR","ISHL","ISHR","ROTL","ROTR"};
            FluxOpcode oc = FLUX_IADD;
            for (int j = 0; j < 13; j++) if (!strcmp(u, names[j])) { oc = map[j]; break; }
            emit_u8(a, oc);
            /* Support both 2-arg (IADD R0, R1) and 3-arg (IADD R0, R0, R1) */
            int rd = parse_reg(args[0]);
            int rs1 = parse_reg(args[1]);
            int rs2 = parse_reg(args[2]);
            /* If 3-arg form and rd != rs1, emit MOVI first */
            if (rs2 >= 0 && rd != rs1) {
                /* Need: MOV rd, rs1 then ALU rd, rs2 */
                emit_u8(a, (uint8_t)rd);
                emit_u8(a, (uint8_t)rs2);
            } else {
                /* 2-arg form or 3-arg with rd==rs1 */
                emit_u8(a, (uint8_t)rd);
                emit_u8(a, (uint8_t)(rs2 >= 0 ? rs2 : rs1));
            }
        }

        /* Format B: INC, DEC */
        else if (!strcmp(u,"INC")) { emit_u8(a, FLUX_INC); emit_u8(a, (uint8_t)parse_reg(args[0])); }
        else if (!strcmp(u,"DEC")) { emit_u8(a, FLUX_DEC); emit_u8(a, (uint8_t)parse_reg(args[0])); }

        /* Format C: CMP */
        else if (!strcmp(u,"CMP")) {
            emit_u8(a, FLUX_CMP);
            emit_u8(a, (uint8_t)parse_reg(args[0]));
            emit_u8(a, (uint8_t)parse_reg(args[1]));
        }

        /* Format D: JMP */
        else if (!strcmp(u,"JMP")) {
            emit_u8(a, FLUX_JMP);
            emit_u8(a, 0); /* placeholder reg */
            int target = find_label(a, args[0]);
            if (target >= 0) {
                emit_i16(a, (int16_t)(target - (pos + 4)));
            } else {
                if (a->fixup_count < MAX_LABELS) {
                    a->fixup_pos[a->fixup_count] = pos + 2;
                    a->fixup_instr_end[a->fixup_count] = pos + 4;
                    snprintf(a->fixup_label[a->fixup_count], MAX_LABEL_LEN, "%s", args[0]);
                    a->fixup_count++;
                }
                emit_i16(a, 0);
            }
        }

        /* Format D: JZ/JE, JNZ/JNE */
        else if (!strcmp(u,"JZ") || !strcmp(u,"JE") || !strcmp(u,"JNZ") || !strcmp(u,"JNE")) {
            int is_jz = (!strcmp(u,"JZ") || !strcmp(u,"JE"));
            emit_u8(a, is_jz ? FLUX_JZ : FLUX_JNZ);
            emit_u8(a, (uint8_t)parse_reg(args[0]));
            /* Label arg may have trailing comma from arg[0] parsing — use args[1] */
            char label[64]; clean_arg(label, args[1], sizeof(label));
            int target = find_label(a, label);
            if (target >= 0) {
                emit_i16(a, (int16_t)(target - (pos + 4)));
            } else {
                if (a->fixup_count < MAX_LABELS) {
                    a->fixup_pos[a->fixup_count] = pos + 2;
                    a->fixup_instr_end[a->fixup_count] = pos + 4;
                    snprintf(a->fixup_label[a->fixup_count], MAX_LABEL_LEN, "%s", label);
                    a->fixup_count++;
                }
                emit_i16(a, 0);
            }
        }

        /* Format B: PUSH, POP */
        else if (!strcmp(u,"PUSH")) { emit_u8(a, FLUX_PUSH); emit_u8(a, (uint8_t)parse_reg(args[0])); }
        else if (!strcmp(u,"POP"))  { emit_u8(a, FLUX_POP);  emit_u8(a, (uint8_t)parse_reg(args[0])); }

        /* Format B: INEG */
        else if (!strcmp(u,"INEG")) { emit_u8(a, FLUX_INEG); emit_u8(a, (uint8_t)parse_reg(args[0])); }

        /* Format D: CALL */
        else if (!strcmp(u,"CALL")) {
            emit_u8(a, FLUX_CALL);
            emit_u8(a, 0);
            char label[64]; clean_arg(label, args[0], sizeof(label));
            int target = find_label(a, label);
            if (target >= 0) {
                emit_i16(a, (int16_t)(target - (pos + 4)));
            } else {
                if (a->fixup_count < MAX_LABELS) {
                    a->fixup_pos[a->fixup_count] = pos + 2;
                    a->fixup_instr_end[a->fixup_count] = pos + 4;
                    snprintf(a->fixup_label[a->fixup_count], MAX_LABEL_LEN, "%s", label);
                    a->fixup_count++;
                }
                emit_i16(a, 0);
            }
        }

        /* Format A-ish: RET (3 bytes: opcode + 2 pad) */
        else if (!strcmp(u,"RET")) { emit_u8(a, FLUX_RET); emit_u8(a, 0); emit_u8(a, 0); }

        else {
            fprintf(stderr, "Warning: unknown instruction '%s' line %d\n", u, i+1);
        }
    }

    /* Apply fixups */
    for (int f = 0; f < a->fixup_count; f++) {
        int target = find_label(a, a->fixup_label[f]);
        if (target < 0) {
            fprintf(stderr, "Error: undefined label '%s'\n", a->fixup_label[f]);
            return 1;
        }
        int offset = target - a->fixup_instr_end[f];
        a->output[a->fixup_pos[f]]     = (uint8_t)(offset & 0xFF);
        a->output[a->fixup_pos[f] + 1] = (uint8_t)((offset >> 8) & 0xFF);
    }
    return 0;
}

static int assemble_file(Assembler* a, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) { fprintf(stderr, "Error: cannot open '%s'\n", filename); return 1; }
    char buf[512];
    while (fgets(buf, sizeof(buf), f) && a->count < MAX_LINES) {
        int len = strlen(buf);
        while (len > 0 && (buf[len-1]=='\n'||buf[len-1]=='\r')) buf[--len] = '\0';
        a->lines[a->count++] = strdup(buf);
    }
    fclose(f);
    pass1(a);
    return pass2(a);
}

int main(int argc, char** argv) {
    const char* input_file = NULL;
    const char* output_file = "a.bin";
    int run_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i],"-o") && i+1<argc) output_file = argv[++i];
        else if (!strcmp(argv[i],"--run")) run_mode = 1;
        else if (argv[i][0] != '-') input_file = argv[i];
        else { fprintf(stderr, "Usage: %s [-o out.bin] [--run] input.fluxasm\n", argv[0]); return 1; }
    }
    if (!input_file) { fprintf(stderr, "Usage: %s [-o out.bin] [--run] input.fluxasm\n", argv[0]); return 1; }

    Assembler a;
    asm_init(&a);
    if (assemble_file(&a, input_file) != 0) { asm_free(&a); return 1; }

    FILE* out = fopen(output_file, "wb");
    if (!out) { fprintf(stderr, "Error: cannot write '%s'\n", output_file); asm_free(&a); return 1; }
    fwrite(a.output, 1, a.output_pos, out);
    fclose(out);
    printf("Assembled %d bytes to %s\n", a.output_pos, output_file);
    printf("Hex: ");
    for (int i = 0; i < a.output_pos; i++) printf("%02X ", a.output[i]);
    printf("\n");

    if (run_mode) {
        printf("\n--- Running ---\n");
        #include <flux/vm.h>
        FluxVM vm;
        flux_vm_init(&vm, a.output, a.output_pos, 4096);
        int64_t cycles = flux_vm_execute(&vm);
        printf("Cycles: %lld\nR0=%d R1=%d R2=%d R3=%d R4=%d\nHalted: %s\n",
            (long long)cycles, vm.regs.gp[0], vm.regs.gp[1], vm.regs.gp[2],
            vm.regs.gp[3], vm.regs.gp[4], vm.halted ? "yes" : "no");
        flux_vm_free(&vm);
    }

    asm_free(&a);
    return 0;
}
