#include "../src/flux_vm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %-50s", #name);
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { FAIL(#a " != " #b); return; } } while(0)

void test_init() {
    TEST(init);
    FluxVM vm;
    flux_vm_init(&vm);
    ASSERT_EQ(vm.halted, false);
    ASSERT_EQ(vm.pc, 0);
    ASSERT_EQ(vm.cycles, 0);
    ASSERT_EQ(vm.sp, FLUX_STACK_SIZE);
    PASS();
}

void test_halt() {
    TEST(halt);
    FluxVM vm;
    flux_vm_init(&vm);
    uint8_t bc[] = { OP_HALT };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.halted, true);
    ASSERT_EQ(vm.cycles, 1);
    PASS();
}

void test_nop() {
    TEST(nop);
    FluxVM vm;
    flux_vm_init(&vm);
    uint8_t bc[] = { OP_NOP, OP_HALT };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.halted, true);
    ASSERT_EQ(vm.cycles, 2);
    PASS();
}

void test_movi() {
    TEST(movi_imm8);
    FluxVM vm;
    flux_vm_init(&vm);
    uint8_t bc[] = { OP_MOVI, 0, 42, OP_HALT };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.gp[0], 42);
    PASS();
}

void test_movi_negative() {
    TEST(movi_negative);
    FluxVM vm;
    flux_vm_init(&vm);
    uint8_t bc[] = { OP_MOVI, 0, (uint8_t)(-5), OP_HALT };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.gp[0], -5);
    PASS();
}

void test_movi16() {
    TEST(movi16);
    FluxVM vm;
    flux_vm_init(&vm);
    // MOVI16 R0, 1000 (0x03E8)
    uint8_t bc[] = { OP_MOVI16, 0, 0xE8, 0x03, OP_HALT };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.gp[0], 1000);
    PASS();
}

void test_movi16_negative() {
    TEST(movi16_negative);
    FluxVM vm;
    flux_vm_init(&vm);
    // MOVI16 R0, -32768 (0x8000)
    uint8_t bc[] = { OP_MOVI16, 0, 0x00, 0x80, OP_HALT };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.gp[0], -32768);
    PASS();
}

void test_add() {
    TEST(add);
    FluxVM vm;
    flux_vm_init(&vm);
    uint8_t bc[] = {
        OP_MOVI, 0, 10,     // R0 = 10
        OP_MOVI, 1, 20,     // R1 = 20
        OP_ADD, 2, 0, 1,    // R2 = R0 + R1
        OP_HALT
    };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.gp[2], 30);
    PASS();
}

void test_sub() {
    TEST(sub);
    FluxVM vm;
    flux_vm_init(&vm);
    uint8_t bc[] = {
        OP_MOVI, 0, 50,     // R0 = 50
        OP_MOVI, 1, 20,     // R1 = 20
        OP_SUB, 2, 0, 1,    // R2 = R0 - R1
        OP_HALT
    };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.gp[2], 30);
    PASS();
}

void test_mul() {
    TEST(mul);
    FluxVM vm;
    flux_vm_init(&vm);
    uint8_t bc[] = {
        OP_MOVI, 0, 7,
        OP_MOVI, 1, 6,
        OP_MUL, 2, 0, 1,
        OP_HALT
    };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.gp[2], 42);
    PASS();
}

void test_div() {
    TEST(div);
    FluxVM vm;
    flux_vm_init(&vm);
    uint8_t bc[] = {
        OP_MOVI, 0, 42,
        OP_MOVI, 1, 7,
        OP_DIV, 2, 0, 1,
        OP_HALT
    };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.gp[2], 6);
    PASS();
}

void test_inc_dec() {
    TEST(inc_dec);
    FluxVM vm;
    flux_vm_init(&vm);
    uint8_t bc[] = {
        OP_MOVI, 0, 10,
        OP_INC, 0,          // R0 = 11
        OP_INC, 0,          // R0 = 12
        OP_DEC, 0,          // R0 = 11
        OP_HALT
    };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.gp[0], 11);
    PASS();
}

void test_push_pop() {
    TEST(push_pop);
    FluxVM vm;
    flux_vm_init(&vm);
    uint8_t bc[] = {
        OP_MOVI, 0, 42,
        OP_PUSH, 0,
        OP_MOVI, 0, 0,      // Clear R0
        OP_POP, 1,           // R1 = popped value
        OP_HALT
    };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.gp[1], 42);
    PASS();
}

void test_jnz_loop() {
    TEST(jnz_loop_count_to_5);
    FluxVM vm;
    flux_vm_init(&vm);
    // R0 = 5 (counter), R1 = 0 (result)
    // loop: R1 += 1, R0--, JNZ R0 loop
    uint8_t bc[] = {
        OP_MOVI, 0, 5,       // R0 = 5
        OP_MOVI, 1, 0,       // R1 = 0
        // loop:
        OP_INC, 1,           // R1++
        OP_DEC, 0,           // R0--
        OP_JNZ, 0, (uint8_t)(-4 & 0xFF), 0,  // JNZ R0, -4 (back to INC)
        OP_HALT
    };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.gp[0], 0);
    ASSERT_EQ(vm.gp[1], 5);
    PASS();
}

void test_loop_instruction() {
    TEST(loop_instruction);
    FluxVM vm;
    flux_vm_init(&vm);
    // R0 = 5, R1 = 0
    // loop: R1 += R0, LOOP R0 back 2 instructions
    uint8_t bc[] = {
        OP_MOVI, 0, 5,       // R0 = 5
        OP_MOVI, 1, 0,       // R1 = 0
        // loop:
        OP_ADD, 1, 1, 0,     // R1 += R0
        OP_LOOP, 0, 0x04, 0x00,  // LOOP R0, 4 (jump back 4 bytes)
        OP_HALT
    };
    flux_vm_execute(&vm, bc, sizeof(bc));
    // 5 + 4 + 3 + 2 + 1 = 15
    ASSERT_EQ(vm.gp[1], 15);
    PASS();
}

void test_addi_subi() {
    TEST(addi_subi);
    FluxVM vm;
    flux_vm_init(&vm);
    uint8_t bc[] = {
        OP_MOVI, 0, 10,      // R0 = 10
        OP_ADDI, 0, 5,       // R0 = 15
        OP_SUBI, 0, 3,       // R0 = 12
        OP_HALT
    };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.gp[0], 12);
    PASS();
}

void test_cmp_eq() {
    TEST(cmp_eq);
    FluxVM vm;
    flux_vm_init(&vm);
    uint8_t bc[] = {
        OP_MOVI, 0, 5,
        OP_MOVI, 1, 5,
        OP_CMP_EQ, 2, 0, 1,  // R2 = (R0 == R1)
        OP_HALT
    };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.gp[2], 1);
    PASS();
}

void test_stripconf() {
    TEST(stripconf);
    FluxVM vm;
    flux_vm_init(&vm);
    uint8_t bc[] = {
        OP_STRIPCONF, 3,     // Strip confidence for next 3 ops
        OP_MOVI, 0, 42,
        OP_MOVI, 1, 10,
        OP_ADD, 2, 0, 1,
        OP_HALT
    };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.stripconf_remaining, 0);
    ASSERT_EQ(vm.gp[2], 52);
    PASS();
}

void test_fibonacci() {
    TEST(fibonacci_10);
    FluxVM vm;
    flux_vm_init(&vm);
    // Compute fibonacci(10) = 55
    uint8_t bc[] = {
        OP_MOVI, 0, 1,        // R0 = 1 (a)
        OP_MOVI, 1, 1,        // R1 = 1 (b)
        OP_MOVI, 2, 10,       // R2 = 10 (count)
        // loop:
        OP_ADD, 3, 0, 1,      // R3 = a + b
        OP_MOV, 0, 1, 0,      // R0 = b (a = b)
        OP_MOV, 1, 3, 0,      // R1 = R3 (b = R3)
        OP_DEC, 2,            // R2--
        OP_JNZ, 2, (uint8_t)(-14 & 0xFF), 0,  // JNZ R2, -14 (back to ADD)
        OP_HALT
    };
    flux_vm_execute(&vm, bc, sizeof(bc));
    ASSERT_EQ(vm.gp[1], 144); // fib(11) = 144 (10 iterations from fib(1),fib(2))
    PASS();
}

int main() {
    printf("\nFLUX Unified VM Tests (FORMAT_A-G)\n");
    printf("===================================\n\n");
    
    test_init();
    test_halt();
    test_nop();
    test_movi();
    test_movi_negative();
    test_movi16();
    test_movi16_negative();
    test_add();
    test_sub();
    test_mul();
    test_div();
    test_inc_dec();
    test_push_pop();
    test_jnz_loop();
    test_loop_instruction();
    test_addi_subi();
    test_cmp_eq();
    test_stripconf();
    test_fibonacci();
    
    printf("\n===================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
