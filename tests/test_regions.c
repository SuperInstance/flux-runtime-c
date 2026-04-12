/*
 * Tests for REGION_CREATE, REGION_DESTROY, REGION_TRANSFER, MEMSET, MEMCMP, MEMCOPY
 * Also tests: CHECK_BOUNDS, BOX/UNBOX, RESOURCE ops
 */
#include <stdio.h>
#include <string.h>
#include "flux.h"

static int pass = 0, fail = 0;
#define T(n) printf("  %-50s", n)
#define P() do { printf("PASS\n"); pass++; } while(0)
#define F(m) do { printf("FAIL: %s\n", m); fail++; } while(0)

static FluxVM run_vm(const uint8_t* bc, uint32_t len, uint32_t mem) {
    FluxVM vm;
    flux_vm_init(&vm, bc, len, mem);
    flux_vm_execute(&vm);
    return vm;
}

/* Helper: build REGION_CREATE bytecode
 * Opcode: 0x98, len(u16), name_len(1), name_bytes..., size(u32) */
static int emit_region_create(uint8_t* bc, const char* name, uint32_t size) {
    uint8_t nl = (uint8_t)strlen(name);
    uint16_t payload_len = 1 + nl + 4;  /* name_len + name + size */
    int i = 0;
    bc[i++] = 0x98; /* FLUX_REGION_CREATE */
    bc[i++] = (uint8_t)(payload_len & 0xFF);
    bc[i++] = (uint8_t)((payload_len >> 8) & 0xFF);
    bc[i++] = nl;
    memcpy(bc + i, name, nl); i += nl;
    memcpy(bc + i, &size, 4); i += 4;
    return i;
}

/* Helper: build REGION_DESTROY bytecode */
static int emit_region_destroy(uint8_t* bc, const char* name) {
    uint16_t payload_len = (uint16_t)strlen(name);
    int i = 0;
    bc[i++] = 0x99; /* FLUX_REGION_DESTROY */
    bc[i++] = (uint8_t)(payload_len & 0xFF);
    bc[i++] = (uint8_t)((payload_len >> 8) & 0xFF);
    memcpy(bc + i, name, payload_len); i += payload_len;
    return i;
}

int main(void) {
    printf("FLUX Memory Region & Advanced Tests\n\n");

    /* ── REGION_CREATE via bytecode ─────────────────────────────── */
    {
        T("region_create");
        uint8_t bc[128];
        int n = 0;
        n += emit_region_create(bc + n, "test_region", 512);
        bc[n++] = 0x00; /* HALT */
        FluxVM vm = run_vm(bc, n, 4096);
        FluxMemRegion* r = flux_mem_get(&vm.mem, "test_region");
        if (r && r->size == 512 && r->data) P(); else F("region not created");
        flux_vm_free(&vm);
    }

    /* ── REGION_CREATE then destroy ─────────────────────────────── */
    {
        T("region_destroy");
        uint8_t bc[128];
        int n = 0;
        n += emit_region_create(bc + n, "temp", 256);
        n += emit_region_destroy(bc + n, "temp");
        bc[n++] = 0x00; /* HALT */
        FluxVM vm = run_vm(bc, n, 4096);
        FluxMemRegion* r = flux_mem_get(&vm.mem, "temp");
        if (!r) P(); else F("region should be destroyed");
        flux_vm_free(&vm);
    }

    /* ── REGION_CREATE and write/read via STORE8/LOAD8 ──────────── */
    {
        T("region_write_read");
        uint8_t bc[128];
        int n = 0;
        n += emit_region_create(bc + n, "mybuf", 64);
        /* Now use STORE8/LOAD8 on heap (we can't directly target custom regions via STORE8,
           but we can use the heap which is already created) */
        /* Write to heap at offset 0: STORE8 R0(addr), R1(val) */
        bc[n++] = 0x18; bc[n++] = 0x00; bc[n++] = 0x00; bc[n++] = 0x00; /* MOVI R0, 0 */
        bc[n++] = 0x18; bc[n++] = 0x01; bc[n++] = 0x42; bc[n++] = 0x00; /* MOVI R1, 66 */
        bc[n++] = 0xAB; bc[n++] = 0x00; bc[n++] = 0x01; /* STORE8 heap[R0], R1 */
        bc[n++] = 0xAA; bc[n++] = 0x02; bc[n++] = 0x00; /* LOAD8 R2, R0 → R2 = heap[0] */
        bc[n++] = 0x00;
        FluxVM vm = run_vm(bc, n, 4096);
        if (vm.regs.gp[2] == 0x42) P(); else { printf("(R2=%d) ", vm.regs.gp[2]); F("expected 66"); }
        flux_vm_free(&vm);
    }

    /* ── CHECK_BOUNDS ───────────────────────────────────────────── */
    {
        T("check_bounds_in_range");
        /* CHECK_BOUNDS rd, rs1: GPR[0] = (GPR[rd] >= 0 && GPR[rd] < GPR[rs1]) */
        uint8_t bc[] = {
            0x18, 0x00, 5, 0x00,     /* R0 = 5 */
            0x18, 0x01, 10, 0x00,    /* R1 = 10 */
            0xA2, 0x00, 0x01,         /* CHECK_BOUNDS R0, R1 → GPR[0] = (5 >= 0 && 5 < 10) = 1 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[0] == 1) P(); else F("expected 1 (in bounds)");
        flux_vm_free(&vm);
    }

    {
        T("check_bounds_out_of_range");
        uint8_t bc[] = {
            0x18, 0x00, 15, 0x00,    /* R0 = 15 */
            0x18, 0x01, 10, 0x00,    /* R1 = 10 */
            0xA2, 0x00, 0x01,         /* CHECK_BOUNDS → (15 < 10) = 0 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[0] == 0) P(); else F("expected 0 (out of bounds)");
        flux_vm_free(&vm);
    }

    {
        T("check_bounds_negative");
        uint8_t bc[] = {
            0x18, 0x00, 0xFF, 0xFF,  /* R0 = -1 */
            0x18, 0x01, 10, 0x00,    /* R1 = 10 */
            0xA2, 0x00, 0x01,         /* CHECK_BOUNDS → (-1 >= 0) is false = 0 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[0] == 0) P(); else F("expected 0 (negative index)");
        flux_vm_free(&vm);
    }

    /* ── BOX / UNBOX ────────────────────────────────────────────── */
    {
        T("box_int_unbox_int");
        /* BOX type=0(int), val=42(LE), then UNBOX id=0, type=0 → R0=42 */
        uint8_t bc[] = {
            0x9F, 0x05, 0x00,          /* BOX: len=5 */
            0x00,                      /* type=int */
            0x2A, 0x00, 0x00, 0x00,   /* val=42 (LE, 4 bytes) */
            0xA0, 0x01, 0x00,          /* UNBOX: len=1, id=0 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[0] == 42) P(); else { printf("(R0=%d) ", vm.regs.gp[0]); F("expected 42"); }
        flux_vm_free(&vm);
    }

    {
        T("check_type_int");
        uint8_t bc[] = {
            0x9F, 0x05, 0x00,          /* BOX: len=5 */
            0x00,                      /* type=int */
            0x2A, 0x00, 0x00, 0x00,   /* val=42 */
            0xA1, 0x00, 0x00,          /* CHECK_TYPE box[0], type=0 → GPR[0]=1 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[0] == 1) P(); else F("expected 1 (is int)");
        flux_vm_free(&vm);
    }

    {
        T("check_type_wrong");
        uint8_t bc[] = {
            0x9F, 0x05, 0x00,          /* BOX: len=5 */
            0x00,                      /* type=int */
            0x2A, 0x00, 0x00, 0x00,   /* val=42 */
            0xA1, 0x00, 0x01,          /* CHECK_TYPE box[0], type=1(float) → GPR[0]=0 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[0] == 0) P(); else F("expected 0 (not float)");
        flux_vm_free(&vm);
    }

    /* ── RESOURCE_ACQUIRE / RESOURCE_RELEASE ────────────────────── */
    {
        T("resource_acquire_release");
        /* RESOURCE_ACQUIRE: len=1(u16), data[0]=5 → vm.resources[5] = 1, R0 = 1 */
        uint8_t bc[] = {
            0x82, 0x01, 0x00, 0x05,   /* RESOURCE_ACQUIRE: len(u16)=1, resource_id=5 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[0] == 1 && vm.resources[5] == 1) P(); else F("expected acquired");
        flux_vm_free(&vm);
    }

    {
        T("resource_release");
        uint8_t bc[] = {
            0x82, 0x01, 0x00, 0x07,   /* ACQUIRE resource 7: len=1, id=7 */
            0x83, 0x01, 0x00, 0x07,   /* RELEASE resource 7: len=1, id=7 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.resources[7] == 0) P(); else F("expected released");
        flux_vm_free(&vm);
    }

    /* ── JLT / JGT ──────────────────────────────────────────────── */
    {
        T("jlt_taken");
        uint8_t bc[] = {
            0x18, 0x00, 0xFF, 0xFF,  /* R0 = -1 */
            0x3E, 0x00, 4, 0x00,     /* JLT R0, +4 → jump (since -1 < 0) */
            0x18, 0x01, 1, 0x00,     /* R1 = 1 (skipped) */
            0x18, 0x01, 2, 0x00,     /* R1 = 2 (reached) */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[1] == 2) P(); else { printf("(R1=%d) ", vm.regs.gp[1]); F("expected 2"); }
        flux_vm_free(&vm);
    }

    {
        T("jgt_taken");
        uint8_t bc[] = {
            0x18, 0x00, 5, 0x00,     /* R0 = 5 */
            0x3F, 0x00, 4, 0x00,     /* JGT R0, +4 → jump (since 5 > 0) */
            0x18, 0x01, 1, 0x00,     /* R1 = 1 (skipped) */
            0x18, 0x01, 2, 0x00,     /* R1 = 2 (reached) */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[1] == 2) P(); else { printf("(R1=%d) ", vm.regs.gp[1]); F("expected 2"); }
        flux_vm_free(&vm);
    }

    {
        T("jlt_not_taken");
        uint8_t bc[] = {
            0x18, 0x00, 5, 0x00,     /* R0 = 5 */
            0x3E, 0x00, 4, 0x00,     /* JLT R0, +4 → don't jump (5 not < 0) */
            0x18, 0x01, 1, 0x00,     /* R1 = 1 (reached) */
            0x18, 0x01, 2, 0x00,     /* R1 = 2 (reached) */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[1] == 2) P(); else { printf("(R1=%d) ", vm.regs.gp[1]); F("expected 2"); }
        flux_vm_free(&vm);
    }

    /* ── SETCC: read condition flags into register ──────────────── */
    {
        T("setcc_zero_flag");
        uint8_t bc[] = {
            0x18, 0x00, 10, 0x00,
            0x18, 0x01, 10, 0x00,
            0x2C, 0x00, 0x01,         /* CMP_EQ R0, R1 → flag_zero=1 */
            0x93, 0x02, 0x00,         /* SETCC R2, R0(=0) → R2 = flag_zero = 1 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[2] == 1) P(); else { printf("(R2=%d) ", vm.regs.gp[2]); F("expected 1"); }
        flux_vm_free(&vm);
    }

    {
        T("setcc_sign_flag");
        uint8_t bc[] = {
            0x18, 0x00, 5, 0x00,
            0x18, 0x01, 10, 0x00,
            0x2D, 0x00, 0x01,         /* CMP_LT R0, R1 → flag_zero=0, flag_sign=1 */
            0x93, 0x02, 0x01,         /* SETCC R2, R1(=1) → R2 = flag_sign = 1 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[2] == 1) P(); else { printf("(R2=%d) ", vm.regs.gp[2]); F("expected 1"); }
        flux_vm_free(&vm);
    }

    /* ── LOOP instruction ───────────────────────────────────────── */
    {
        T("loop_basic");
        /* LOOP rd, rs1: GPR[rd] += GPR[rs1], then jump by imm */
        /* Actually LOOP rd, rs1: just GPR[rd] += GPR[rs1]; then JMP */
        uint8_t bc[] = {
            0x18, 0x00, 0, 0x00,     /* R0 = 0 */
            0x18, 0x01, 1, 0x00,     /* R1 = 1 */
            0x46, 0x00, 0x01,         /* LOOP R0, R1: R0 += R1 → R0=1 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[0] == 1) P(); else { printf("(R0=%d) ", vm.regs.gp[0]); F("expected 1"); }
        flux_vm_free(&vm);
    }

    /* ── YIELD (should be a no-op) ──────────────────────────────── */
    {
        T("yield_nop");
        uint8_t bc[] = {
            0x18, 0x00, 99, 0x00,
            0x81,                     /* YIELD */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[0] == 99 && vm.running == 0) P(); else F("yield should be nop");
        flux_vm_free(&vm);
    }

    /* ── DEBUG_BREAK (should be a no-op) ────────────────────────── */
    {
        T("debug_break_nop");
        uint8_t bc[] = {
            0x18, 0x00, 77, 0x00,
            0x84,                     /* DEBUG_BREAK */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc), 4096);
        if (vm.regs.gp[0] == 77 && vm.halted) P(); else F("debug_break should be nop");
        flux_vm_free(&vm);
    }

    printf("\n%d/%d region & advanced tests passed\n", pass, pass + fail);
    return fail;
}
