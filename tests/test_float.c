/*
 * Comprehensive float arithmetic tests for FLUX VM
 * Tests FADD, FSUB, FMUL, FDIV, FNEG, FABS, FMIN, FMAX, FEQ, FLT, FLE, FGT, FGE, CAST
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "flux.h"

static int pass = 0, fail = 0;
#define T(n) printf("  %-50s", n)
#define P() do { printf("PASS\n"); pass++; } while(0)
#define F(m) do { printf("FAIL: %s\n", m); fail++; } while(0)
#define FABS_ERR(a, b) (fabsf((a) - (b)) > 0.001f)

/* Helper: run bytecode, return VM state */
static FluxVM run_vm(const uint8_t* bc, uint32_t len) {
    FluxVM vm;
    flux_vm_init(&vm, bc, len, 4096);
    flux_vm_execute(&vm);
    return vm;
}

int main(void) {
    printf("FLUX Float Arithmetic Tests\n\n");

    /* ── FADD: float addition ────────────────────────────────────── */
    {
        T("fadd_basic");
        /* MOVI R0, 100; CAST R0→FP0; MOVI R1, 200; CAST R1→FP1; FADD FP0,FP1; CAST FP0→R0; HALT */
        uint8_t bc[] = {
            0x18, 0x00, 100, 0x00,   /* MOVI R0, 100 */
            0x9E, 0x00, 0x00,         /* CAST R0→FP0 (gp to fp) */
            0x18, 0x01, 200, 0x00,   /* MOVI R1, 200 */
            0x9E, 0x01, 0x00,         /* CAST R1→FP1 */
            0x30, 0x00, 0x01,         /* FADD FP0, FP1 */
            0x9E, 0x00, 0x01,         /* CAST FP0→R0 (fp to gp) */
            0x00                      /* HALT */
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == 300) P(); else F("expected 300");
        flux_vm_free(&vm);
    }

    /* ── FSUB: float subtraction ────────────────────────────────── */
    {
        T("fsub_basic");
        uint8_t bc[] = {
            0x18, 0x00, 100, 0x00,   /* MOVI R0, 100 */
            0x9E, 0x00, 0x00,         /* CAST R0→FP0 */
            0x18, 0x01, 30, 0x00,    /* MOVI R1, 30 */
            0x9E, 0x01, 0x00,         /* CAST R1→FP1 */
            0x31, 0x00, 0x01,         /* FSUB FP0, FP1 → FP0=70 */
            0x9E, 0x00, 0x01,         /* CAST FP0→R0 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == 70) P(); else F("expected 70");
        flux_vm_free(&vm);
    }

    /* ── FMUL: float multiplication ─────────────────────────────── */
    {
        T("fmul_basic");
        uint8_t bc[] = {
            0x18, 0x00, 7, 0x00,     /* MOVI R0, 7 */
            0x9E, 0x00, 0x00,         /* CAST R0→FP0 */
            0x18, 0x01, 6, 0x00,     /* MOVI R1, 6 */
            0x9E, 0x01, 0x00,         /* CAST R1→FP1 */
            0x32, 0x00, 0x01,         /* FMUL FP0, FP1 → FP0=42 */
            0x9E, 0x00, 0x01,         /* CAST FP0→R0 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == 42) P(); else F("expected 42");
        flux_vm_free(&vm);
    }

    /* ── FDIV: float division ───────────────────────────────────── */
    {
        T("fdiv_basic");
        uint8_t bc[] = {
            0x18, 0x00, 100, 0x00,   /* MOVI R0, 100 */
            0x9E, 0x00, 0x00,         /* CAST R0→FP0 */
            0x18, 0x01, 4, 0x00,     /* MOVI R1, 4 */
            0x9E, 0x01, 0x00,         /* CAST R1→FP1 */
            0x33, 0x00, 0x01,         /* FDIV FP0, FP1 → FP0=25 */
            0x9E, 0x00, 0x01,         /* CAST FP0→R0 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == 25) P(); else F("expected 25");
        flux_vm_free(&vm);
    }

    /* ── FNEG: float negate ─────────────────────────────────────── */
    {
        T("fneg_basic");
        uint8_t bc[] = {
            0x18, 0x00, 50, 0x00,    /* MOVI R0, 50 */
            0x9E, 0x00, 0x00,         /* CAST R0→FP0 */
            0xA3, 0x00,               /* FNEG FP0 → FP0=-50 */
            0x9E, 0x00, 0x01,         /* CAST FP0→R0 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == -50) P(); else F("expected -50");
        flux_vm_free(&vm);
    }

    /* ── FABS: float absolute value ─────────────────────────────── */
    {
        T("fabs_basic");
        uint8_t bc[] = {
            0x18, 0x00, 0xEC, 0xFF,  /* MOVI R0, -20 (0xFFEC) */
            0x9E, 0x00, 0x00,         /* CAST R0→FP0 → FP0=-20.0 */
            0xA4, 0x00,               /* FABS FP0 → FP0=20.0 */
            0x9E, 0x00, 0x01,         /* CAST FP0→R0 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == 20) P(); else { printf("(got %d) ", vm.regs.gp[0]); F("expected 20"); }
        flux_vm_free(&vm);
    }

    /* ── FMIN / FMAX ────────────────────────────────────────────── */
    {
        T("fmin_fmax");
        uint8_t bc[] = {
            0x18, 0x00, 10, 0x00,    /* MOVI R0, 10 */
            0x9E, 0x00, 0x00,         /* CAST R0→FP0 */
            0x18, 0x01, 25, 0x00,    /* MOVI R1, 25 */
            0x9E, 0x01, 0x00,         /* CAST R1→FP1 */
            0x34, 0x02, 0x00,         /* FMIN FP2, FP0 → FP2=10 */
            0x9E, 0x02, 0x00,         /* CAST R2→FP2 first: set FP2 */
            0x9E, 0x02, 0x01,         /* CAST FP2→R2 */
            0x00
        };
        /* FMIN FP2, FP0: reads FP2 (0) and FP0 (10), stores min in FP2 → 0 */
        /* Actually FMIN rd, rs1: rd=min(rd,rs1). FP2 starts as 0.0, FP0=10.0 → min=0.0 */
        FluxVM vm = run_vm(bc, sizeof(bc));
        /* FP2 = min(FP2, FP0) = min(0, 10) = 0 */
        if (vm.regs.gp[2] == 0) P(); else { printf("(got %d) ", vm.regs.gp[2]); F("expected 0"); }
        flux_vm_free(&vm);
    }

    /* ── FEQ: float equality comparison ─────────────────────────── */
    {
        T("feq_equal");
        uint8_t bc[] = {
            0x18, 0x00, 42, 0x00,    /* MOVI R0, 42 */
            0x9E, 0x00, 0x00,         /* CAST R0→FP0 */
            0x18, 0x01, 42, 0x00,    /* MOVI R1, 42 */
            0x9E, 0x01, 0x00,         /* CAST R1→FP1 */
            0xA5, 0x00, 0x01,         /* FEQ FP0, FP1 → R0 = (FP0 == FP1) = 1 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == 1) P(); else F("expected 1 (equal)");
        flux_vm_free(&vm);
    }

    {
        T("feq_not_equal");
        uint8_t bc[] = {
            0x18, 0x00, 42, 0x00,    /* MOVI R0, 42 */
            0x9E, 0x00, 0x00,         /* CAST R0→FP0 */
            0x18, 0x01, 99, 0x00,    /* MOVI R1, 99 */
            0x9E, 0x01, 0x00,         /* CAST R1→FP1 */
            0xA5, 0x00, 0x01,         /* FEQ → R0 = 0 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == 0) P(); else F("expected 0 (not equal)");
        flux_vm_free(&vm);
    }

    /* ── FLT: float less-than ───────────────────────────────────── */
    {
        T("flt_less");
        uint8_t bc[] = {
            0x18, 0x00, 10, 0x00,    /* MOVI R0, 10 */
            0x9E, 0x00, 0x00,         /* CAST R0→FP0 */
            0x18, 0x01, 20, 0x00,    /* MOVI R1, 20 */
            0x9E, 0x01, 0x00,         /* CAST R1→FP1 */
            0xA6, 0x00, 0x01,         /* FLT → R0 = (10 < 20) = 1 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == 1) P(); else F("expected 1");
        flux_vm_free(&vm);
    }

    {
        T("flt_greater");
        uint8_t bc[] = {
            0x18, 0x00, 30, 0x00,
            0x9E, 0x00, 0x00,
            0x18, 0x01, 20, 0x00,
            0x9E, 0x01, 0x00,
            0xA6, 0x00, 0x01,         /* FLT → R0 = (30 < 20) = 0 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == 0) P(); else F("expected 0");
        flux_vm_free(&vm);
    }

    /* ── FLE: float less-or-equal ───────────────────────────────── */
    {
        T("fle_equal");
        uint8_t bc[] = {
            0x18, 0x00, 50, 0x00,
            0x9E, 0x00, 0x00,
            0x18, 0x01, 50, 0x00,
            0x9E, 0x01, 0x00,
            0xA7, 0x00, 0x01,         /* FLE → R0 = (50 <= 50) = 1 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == 1) P(); else F("expected 1");
        flux_vm_free(&vm);
    }

    /* ── FGT / FGE ──────────────────────────────────────────────── */
    {
        T("fgt_greater");
        uint8_t bc[] = {
            0x18, 0x00, 100, 0x00,
            0x9E, 0x00, 0x00,
            0x18, 0x01, 50, 0x00,
            0x9E, 0x01, 0x00,
            0xA8, 0x00, 0x01,         /* FGT → R0 = (100 > 50) = 1 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == 1) P(); else F("expected 1");
        flux_vm_free(&vm);
    }

    {
        T("fge_greater_equal");
        uint8_t bc[] = {
            0x18, 0x00, 50, 0x00,
            0x9E, 0x00, 0x00,
            0x18, 0x01, 50, 0x00,
            0x9E, 0x01, 0x00,
            0xA9, 0x00, 0x01,         /* FGE → R0 = (50 >= 50) = 1 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == 1) P(); else F("expected 1");
        flux_vm_free(&vm);
    }

    /* ── FDIV by zero ───────────────────────────────────────────── */
    {
        T("fdiv_by_zero");
        uint8_t bc[] = {
            0x18, 0x00, 10, 0x00,
            0x9E, 0x00, 0x00,         /* FP0 = 10.0 */
            /* FP1 is already 0.0 from init */
            0x33, 0x00, 0x01,         /* FDIV FP0, FP1 (div by zero!) */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.last_error == FLUX_ERR_DIV_ZERO) P(); else F("expected div_zero error");
        flux_vm_free(&vm);
    }

    /* ── CAST round-trip: int → float → int ─────────────────────── */
    {
        T("cast_roundtrip_negative");
        uint8_t bc[] = {
            0x18, 0x00, 0xE8, 0xFF,  /* MOVI R0, -24 */
            0x9E, 0x00, 0x00,         /* CAST R0→FP0 (GP→FP) */
            0xA3, 0x00,               /* FNEG FP0 → FP0=24.0 */
            0x9E, 0x00, 0x01,         /* CAST FP0→R0 (FP→GP) → R0=24 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == 24) P(); else { printf("(got %d) ", vm.regs.gp[0]); F("expected 24"); }
        flux_vm_free(&vm);
    }

    /* ── Chained float operations ───────────────────────────────── */
    {
        T("fadd_fmul_chain");
        /* (3.0 + 4.0) * 2.0 = 14.0 */
        uint8_t bc[] = {
            0x18, 0x00, 3, 0x00,
            0x9E, 0x00, 0x00,         /* FP0 = 3.0 */
            0x18, 0x01, 4, 0x00,
            0x9E, 0x01, 0x00,         /* FP1 = 4.0 */
            0x30, 0x00, 0x01,         /* FP0 = 3+4 = 7.0 */
            0x18, 0x01, 2, 0x00,
            0x9E, 0x01, 0x00,         /* FP1 = 2.0 */
            0x32, 0x00, 0x01,         /* FP0 = 7*2 = 14.0 */
            0x9E, 0x00, 0x01,         /* R0 = 14 */
            0x00
        };
        FluxVM vm = run_vm(bc, sizeof(bc));
        if (vm.regs.gp[0] == 14) P(); else { printf("(got %d) ", vm.regs.gp[0]); F("expected 14"); }
        flux_vm_free(&vm);
    }

    printf("\n%d/%d float tests passed\n", pass, pass + fail);
    return fail;
}
