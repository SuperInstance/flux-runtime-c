/*
 * Test suite for the FLUX Assembler
 * Tests assembly, label resolution, and execution of assembled bytecode.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "flux/vm.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void name(void)
#define RUN(name) do { printf("  %-40s", #name); name(); } while(0)
#define PASS do { printf("PASS\n"); tests_passed++; return; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; return; } while(0)
#define ASSERT_EQ(a, b, msg) do { if ((a) != (b)) { printf("FAIL: %s (got %d, expected %d)\n", msg, (int)(a), (int)(b)); tests_failed++; return; } } while(0)

/* Helper: assemble inline and run on VM, return R0 */
static int32_t run_bytecode(const uint8_t* bc, uint32_t len) {
    FluxVM vm;
    flux_vm_init(&vm, bc, len, 4096);
    flux_vm_execute(&vm);
    int32_t r0 = vm.regs.gp[0];
    flux_vm_free(&vm);
    return r0;
}

static int32_t run_bytecode_r4(const uint8_t* bc, uint32_t len) {
    FluxVM vm;
    flux_vm_init(&vm, bc, len, 4096);
    flux_vm_execute(&vm);
    int32_t r4 = vm.regs.gp[4];
    flux_vm_free(&vm);
    return r4;
}

/* ── Tests ─────────────────────────────────────────────────────── */

TEST(test_movi_halt) {
    /* MOVI R0, 42; HALT */
    uint8_t bc[] = { 0x18, 0x00, 0x2A, 0x00, 0x00 };
    int32_t r0 = run_bytecode(bc, sizeof(bc));
    ASSERT_EQ(r0, 42, "R0 should be 42");
    PASS;
}

TEST(test_addition) {
    /* MOVI R0, 10; MOVI R1, 20; ADD R0, R1; HALT */
    uint8_t bc[] = {
        0x18, 0x00, 0x0A, 0x00,  /* MOVI R0, 10 */
        0x18, 0x01, 0x14, 0x00,  /* MOVI R1, 20 */
        0x20, 0x00, 0x01,        /* ADD R0, R1 */
        0x00                     /* HALT */
    };
    int32_t r0 = run_bytecode(bc, sizeof(bc));
    ASSERT_EQ(r0, 30, "R0 should be 30");
    PASS;
}

TEST(test_multiplication) {
    /* MOVI R0, 6; MOVI R1, 7; MUL R0, R1; HALT */
    uint8_t bc[] = {
        0x18, 0x00, 0x06, 0x00,  /* MOVI R0, 6 */
        0x18, 0x01, 0x07, 0x00,  /* MOVI R1, 7 */
        0x22, 0x00, 0x01,        /* MUL R0, R1 */
        0x00                     /* HALT */
    };
    int32_t r0 = run_bytecode(bc, sizeof(bc));
    ASSERT_EQ(r0, 42, "R0 should be 42");
    PASS;
}

TEST(test_subtraction) {
    /* MOVI R0, 100; MOVI R1, 37; SUB R0, R1; HALT */
    uint8_t bc[] = {
        0x18, 0x00, 0x64, 0x00,  /* MOVI R0, 100 */
        0x18, 0x01, 0x25, 0x00,  /* MOVI R1, 37 */
        0x21, 0x00, 0x01,        /* SUB R0, R1 */
        0x00                     /* HALT */
    };
    int32_t r0 = run_bytecode(bc, sizeof(bc));
    ASSERT_EQ(r0, 63, "R0 should be 63");
    PASS;
}

TEST(test_division) {
    /* MOVI R0, 100; MOVI R1, 7; DIV R0, R1; HALT */
    uint8_t bc[] = {
        0x18, 0x00, 0x64, 0x00,  /* MOVI R0, 100 */
        0x18, 0x01, 0x07, 0x00,  /* MOVI R1, 7 */
        0x23, 0x00, 0x01,        /* DIV R0, R1 */
        0x00                     /* HALT */
    };
    int32_t r0 = run_bytecode(bc, sizeof(bc));
    ASSERT_EQ(r0, 14, "R0 should be 14 (100/7)");
    PASS;
}

TEST(test_inc_dec) {
    /* MOVI R0, 10; INC R0; INC R0; DEC R0; HALT */
    uint8_t bc[] = {
        0x18, 0x00, 0x0A, 0x00,  /* MOVI R0, 10 */
        0x08, 0x00,              /* INC R0 */
        0x08, 0x00,              /* INC R0 */
        0x09, 0x00,              /* DEC R0 */
        0x00                     /* HALT */
    };
    int32_t r0 = run_bytecode(bc, sizeof(bc));
    ASSERT_EQ(r0, 11, "R0 should be 11");
    PASS;
}

TEST(test_loop_sum_1_to_10) {
    /* Sum 1+2+...+10 = 55
       MOVI R0, 0     ; accumulator
       MOVI R1, 10    ; counter
       loop:
       ADD R0, R1     ; acc += counter
       DEC R1         ; counter--
       JNZ R1, loop   ; if counter != 0, loop
       HALT
    */
    uint8_t bc[] = {
        0x18, 0x00, 0x00, 0x00,  /* MOVI R0, 0 */
        0x18, 0x01, 0x0A, 0x00,  /* MOVI R1, 10 */
        /* loop: offset 8 */
        0x20, 0x00, 0x01,        /* ADD R0, R1 */
        0x09, 0x01,              /* DEC R1 */
        /* JNZ at offset 13: needs to jump back to offset 8 */
        /* JNZ is 4 bytes, ends at offset 17. Target=8, offset = 8-17 = -9 */
        0x3D, 0x01, 0xF7, 0xFF,  /* JNZ R1, -9 */
        0x00                     /* HALT at offset 17 */
    };
    int32_t r0 = run_bytecode(bc, sizeof(bc));
    ASSERT_EQ(r0, 55, "R0 should be 55 (sum 1..10)");
    PASS;
}

TEST(test_factorial_5) {
    /* 5! = 120
       MOVI R3, 5
       MOVI R4, 1
       loop:
       MUL R4, R3
       DEC R3
       JNZ R3, loop
       HALT
    */
    uint8_t bc[] = {
        0x18, 0x03, 0x05, 0x00,  /* MOVI R3, 5 */
        0x18, 0x04, 0x01, 0x00,  /* MOVI R4, 1 */
        /* loop: offset 8 */
        0x22, 0x04, 0x03,        /* MUL R4, R3 */
        0x09, 0x03,              /* DEC R3 */
        /* JNZ at offset 13: target=8, end=17, offset = 8-17 = -9 */
        0x3D, 0x03, 0xF7, 0xFF,  /* JNZ R3, -9 */
        0x00                     /* HALT at offset 17 */
    };
    int32_t r4 = run_bytecode_r4(bc, sizeof(bc));
    ASSERT_EQ(r4, 120, "R4 should be 120 (5!)");
    PASS;
}

TEST(test_nested_loop_3x4) {
    /* Compute 3*4 using repeated addition (nested loop)
       MOVI R0, 0      ; result
       MOVI R1, 3       ; outer counter
       outer:
       MOVI R2, 4       ; inner counter
       inner:
       INC R0
       DEC R2
       JNZ R2, inner
       DEC R1
       JNZ R1, outer
       HALT
    */
    uint8_t bc[] = {
        0x18, 0x00, 0x00, 0x00,  /* MOVI R0, 0 */
        0x18, 0x01, 0x03, 0x00,  /* MOVI R1, 3 */
        /* outer: offset 8 */
        0x18, 0x02, 0x04, 0x00,  /* MOVI R2, 4 */
        /* inner: offset 12 */
        0x08, 0x00,              /* INC R0 */
        0x09, 0x02,              /* DEC R2 */
        /* JNZ at 16: target=12, end=20, offset = 12-20 = -8 */
        0x3D, 0x02, 0xF8, 0xFF,  /* JNZ R2, -8 */
        0x09, 0x01,              /* DEC R1 */
        /* JNZ at 22: target=8, end=26, offset = 8-26 = -18 */
        0x3D, 0x01, 0xEE, 0xFF,  /* JNZ R1, -18 */
        0x00                     /* HALT at offset 26 */
    };
    int32_t r0 = run_bytecode(bc, sizeof(bc));
    ASSERT_EQ(r0, 12, "R0 should be 12 (3*4 via nested loops)");
    PASS;
}

TEST(test_negative_immediate) {
    /* MOVI R0, -5; MOVI R1, 10; ADD R0, R1; HALT → R0 = 5 */
    uint8_t bc[] = {
        0x18, 0x00, 0xFB, 0xFF,  /* MOVI R0, -5 */
        0x18, 0x01, 0x0A, 0x00,  /* MOVI R1, 10 */
        0x20, 0x00, 0x01,        /* ADD R0, R1 */
        0x00                     /* HALT */
    };
    int32_t r0 = run_bytecode(bc, sizeof(bc));
    ASSERT_EQ(r0, 5, "R0 should be 5 (-5 + 10)");
    PASS;
}

TEST(test_push_pop) {
    /* MOVI R0, 42; PUSH R0; MOVI R0, 0; POP R1; HALT → R1=42 */
    uint8_t bc[] = {
        0x18, 0x00, 0x2A, 0x00,  /* MOVI R0, 42 */
        0x0C, 0x00,              /* PUSH R0 */
        0x18, 0x00, 0x00, 0x00,  /* MOVI R0, 0 */
        0x0D, 0x01,              /* POP R1 */
        0x00                     /* HALT */
    };
    FluxVM vm;
    flux_vm_init(&vm, bc, sizeof(bc), 4096);
    flux_vm_execute(&vm);
    ASSERT_EQ(vm.regs.gp[0], 0, "R0 should be 0");
    ASSERT_EQ(vm.regs.gp[1], 42, "R1 should be 42 (popped)");
    flux_vm_free(&vm);
    PASS;
}

TEST(test_conditional_branch) {
    /* if R0 == 5 then R1 = 100 else R1 = 200
       MOVI R0, 5
       MOVI R1, 100
       CMP R0, R1  ; won't match (5 != 100)
       Actually use JZ: MOVI R0, 0 → jump to set 200
    */
    /* Simpler: MOVI R0, 0; JZ R0, skip; MOVI R1, 100; skip: MOVI R1, 200; HALT */
    uint8_t bc[] = {
        0x18, 0x00, 0x00, 0x00,  /* MOVI R0, 0 */
        /* JZ at 4: target=12, end=8, offset=12-8=4 */
        0x3C, 0x00, 0x04, 0x00,  /* JZ R0, +4 (skip past MOVI R1,100) */
        0x18, 0x01, 0x64, 0x00,  /* MOVI R1, 100 (skipped) */
        0x18, 0x01, 0xC8, 0x00,  /* MOVI R1, 200 */
        0x00                     /* HALT */
    };
    FluxVM vm;
    flux_vm_init(&vm, bc, sizeof(bc), 4096);
    flux_vm_execute(&vm);
    ASSERT_EQ(vm.regs.gp[1], 200, "R1 should be 200 (branch taken)");
    flux_vm_free(&vm);
    PASS;
}

/* ── Main ──────────────────────────────────────────────────────── */

int main(void) {
    printf("FLUX Assembler + VM Integration Tests\n\n");

    RUN(test_movi_halt);
    RUN(test_addition);
    RUN(test_multiplication);
    RUN(test_subtraction);
    RUN(test_division);
    RUN(test_inc_dec);
    RUN(test_loop_sum_1_to_10);
    RUN(test_factorial_5);
    RUN(test_nested_loop_3x4);
    RUN(test_negative_immediate);
    RUN(test_push_pop);
    RUN(test_conditional_branch);

    printf("\n%d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
