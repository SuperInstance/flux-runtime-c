/*
 * Comprehensive runtime tests for FLUX C implementation.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include "flux/memory.h"
#include "flux/registers.h"
#include "flux/opcodes.h"
#include "flux/vm.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define RUN(name) do { printf("  %-40s", #name); name(); } while(0)
#define PASS do { printf("PASS\n"); tests_passed++; return; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; return; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); tests_failed++; return; } } while(0)
#define ASSERT_EQ(a, b, msg) do { if ((a) != (b)) { printf("FAIL: %s (got %ld, expected %ld)\n", msg, (long)(a), (long)(b)); tests_failed++; return; } } while(0)
#define ASSERT_EQ_U(a, b, msg) do { if ((a) != (b)) { printf("FAIL: %s (got %lu, expected %lu)\n", msg, (unsigned long)(a), (unsigned long)(b)); tests_failed++; return; } } while(0)
#define ASSERT_STREQ(a, b, msg) do { if (strcmp((a),(b)) != 0) { printf("FAIL: %s (got '%s', expected '%s')\n", msg, (a), (b)); tests_failed++; return; } } while(0)

static void test_memory_init(void) {
    FluxMemManager mm;
    int r = flux_mem_init(&mm);
    ASSERT(r == 0, "flux_mem_init should succeed");
    ASSERT(mm.count == 0, "count should be zero after init");
    PASS;
}

static void test_memory_create(void) {
    FluxMemManager mm;
    flux_mem_init(&mm);
    int r = flux_mem_create(&mm, "heap", 4096, "system");
    ASSERT(r == 0, "flux_mem_create should succeed");
    ASSERT(mm.count == 1, "count should be 1");
    FluxMemRegion* reg = flux_mem_get(&mm, "heap");
    ASSERT(reg != NULL, "region should be found");
    ASSERT(reg->size == 4096, "size matches");
    ASSERT(strcmp(reg->owner, "system") == 0, "owner matches");
    // Try duplicate name
    r = flux_mem_create(&mm, "heap", 8192, "user");
    ASSERT(r != 0, "duplicate create should fail");
    PASS;
}

static void test_memory_read_write(void) {
    FluxMemManager mm;
    flux_mem_init(&mm);
    flux_mem_create(&mm, "mem", 1024, "test");
    FluxMemRegion* reg = flux_mem_get(&mm, "mem");
    ASSERT(reg != NULL, "region exists");

    uint8_t val = 0xAB;
    flux_mem_write_u8(reg, 0, val);
    uint8_t read = flux_mem_read_u8(reg, 0);
    ASSERT(read == 0xAB, "u8 read/write match");

    int32_t i32 = -123456;
    flux_mem_write_i32(reg, 8, i32);
    int32_t i32_read = flux_mem_read_i32(reg, 8);
    ASSERT(i32_read == i32, "i32 read/write match");

    // test generic read/write
    uint8_t buf[10] = {1,2,3,4,5,6,7,8,9,10};
    uint8_t out[10] = {0};
    flux_mem_write(reg, 100, buf, sizeof(buf));
    flux_mem_read(reg, 100, out, sizeof(out));
    ASSERT(memcmp(buf, out, sizeof(buf)) == 0, "block read/write match");
    PASS;
}

static void test_memory_transfer(void) {
    FluxMemManager mm;
    flux_mem_init(&mm);
    flux_mem_create(&mm, "src", 256, "alice");
    FluxMemRegion* src = flux_mem_get(&mm, "src");
    ASSERT(src != NULL, "src region");
    for (int i = 0; i < 256; i++) {
        flux_mem_write_u8(src, i, (uint8_t)i);
    }
    int r = flux_mem_transfer(&mm, "src", "dst", "bob");
    ASSERT(r == 0, "transfer should succeed");
    ASSERT(mm.count == 2, "two regions after transfer");
    FluxMemRegion* dst = flux_mem_get(&mm, "dst");
    ASSERT(dst != NULL, "dst region");
    ASSERT(dst->size == 256, "size copied");
    for (int i = 0; i < 256; i++) {
        uint8_t v = flux_mem_read_u8(dst, i);
        ASSERT(v == (uint8_t)i, "data copied correctly");
    }
    // original still exists
    src = flux_mem_get(&mm, "src");
    ASSERT(src != NULL, "src still present");
    PASS;
}

static void test_memory_destroy(void) {
    FluxMemManager mm;
    flux_mem_init(&mm);
    flux_mem_create(&mm, "temp", 512, "user");
    ASSERT(mm.count == 1, "created");
    int r = flux_mem_destroy(&mm, "temp");
    ASSERT(r == 0, "destroy should succeed");
    ASSERT(mm.count == 0, "count zero after destroy");
    FluxMemRegion* reg = flux_mem_get(&mm, "temp");
    ASSERT(reg == NULL, "region should not be found");
    // destroy non-existing region
    r = flux_mem_destroy(&mm, "none");
    ASSERT(r != 0, "destroy should fail");
    PASS;
}

static void test_registers_init(void) {
    FluxRegFile rf;
    flux_regs_init(&rf);
    for (int i = 0; i < FLUX_GP_COUNT; i++) {
        ASSERT(rf.gp[i] == 0, "GP registers zeroed");
    }
    for (int i = 0; i < FLUX_FP_COUNT; i++) {
        ASSERT(rf.fp[i] == 0.0f, "FP registers zeroed");
    }
    // vec? (optional)
    PASS;
}

static void test_registers_gp_rw(void) {
    FluxRegFile rf;
    flux_regs_init(&rf);
    for (int i = 0; i < FLUX_GP_COUNT; i++) {
        flux_gp_write(&rf, i, i*100);
        int32_t val = flux_gp_read(&rf, i);
        ASSERT(val == i*100, "GP write/read matches");
    }
    // test out-of-bound reads/writes (should be safe)
    int32_t v = flux_gp_read(&rf, 20);
    ASSERT(v == 0, "out-of-bounds read returns 0");
    flux_gp_write(&rf, 20, 123);
    // ensure internal array unchanged? Not needed.
    PASS;
}

static void test_registers_fp_rw(void) {
    FluxRegFile rf;
    flux_regs_init(&rf);
    for (int i = 0; i < FLUX_FP_COUNT; i++) {
        float val = (float)i * 3.14f;
        flux_fp_write(&rf, i, val);
        float r = flux_fp_read(&rf, i);
        ASSERT(r == val, "FP write/read matches");
    }
    PASS;
}

static void test_float_edge_cases(void) {
    FluxRegFile rf;
    flux_regs_init(&rf);
    // INFINITY
    float inf = INFINITY;
    flux_fp_write(&rf, 0, inf);
    float r = flux_fp_read(&rf, 0);
    ASSERT(isinf(r) && r > 0, "INFINITY preserved");
    // -INFINITY
    float neg_inf = -INFINITY;
    flux_fp_write(&rf, 1, neg_inf);
    r = flux_fp_read(&rf, 1);
    ASSERT(isinf(r) && r < 0, "-INFINITY preserved");
    // NaN
    float nan = NAN;
    flux_fp_write(&rf, 2, nan);
    r = flux_fp_read(&rf, 2);
    ASSERT(isnan(r), "NaN preserved");
    // subnormal
    float sub = 1.0e-45f; // denormal for IEEE 754 single
    ASSERT(fpclassify(sub) == FP_SUBNORMAL, "subnormal value");
    flux_fp_write(&rf, 3, sub);
    r = flux_fp_read(&rf, 3);
    ASSERT(r == sub, "subnormal preserved");
    // signed zero
    float pos_zero = 0.0f;
    float neg_zero = -0.0f;
    flux_fp_write(&rf, 4, pos_zero);
    r = flux_fp_read(&rf, 4);
    ASSERT(r == pos_zero && signbit(r) == 0, "positive zero");
    flux_fp_write(&rf, 5, neg_zero);
    r = flux_fp_read(&rf, 5);
    ASSERT(r == neg_zero && signbit(r) != 0, "negative zero");
    PASS;
}

static void test_opcode_names(void) {
    const char* name;
    name = flux_opcode_name(FLUX_HALT);
    ASSERT_STREQ(name, "HALT", "HALT opcode name");
    name = flux_opcode_name(FLUX_NOP);
    ASSERT_STREQ(name, "NOP", "NOP opcode name");
    name = flux_opcode_name(FLUX_INC);
    ASSERT_STREQ(name, "INC", "INC opcode name");
    name = flux_opcode_name(FLUX_DEC);
    ASSERT_STREQ(name, "DEC", "DEC opcode name");
    // test unknown opcode
    name = flux_opcode_name(0xFF);
    ASSERT_STREQ(name, "UNKNOWN", "unknown opcode returns UNKNOWN");
    PASS;
}

static void test_vm_init_free(void) {
    uint8_t dummy_code[] = { FLUX_HALT };
    FluxVM vm;
    int r = flux_vm_init(&vm, dummy_code, sizeof(dummy_code), 4096);
    ASSERT(r == 0, "vm init should succeed");
    ASSERT(vm.bytecode == dummy_code, "bytecode pointer set");
    ASSERT(vm.bytecode_len == sizeof(dummy_code), "bytecode length set");
    ASSERT(vm.regs.gp[0] == 0, "registers zeroed");
    // Ensure memory regions created (stack, heap)
    FluxMemRegion* stack = flux_mem_get(&vm.mem, "stack");
    ASSERT(stack != NULL, "stack region exists");
    ASSERT(stack->size == 4096, "stack size as requested");
    FluxMemRegion* heap = flux_mem_get(&vm.mem, "heap");
    ASSERT(heap != NULL, "heap region exists");
    flux_vm_free(&vm);
    // after free, memory regions freed (cannot assert)
    PASS;
}

static void test_vm_reset(void) {
    uint8_t code[] = { FLUX_HALT };
    FluxVM vm;
    flux_vm_init(&vm, code, sizeof(code), 2048);
    // modify something
    vm.regs.gp[0] = 42;
    vm.cycle_count = 100;
    vm.halted = 1;
    flux_vm_reset(&vm);
    ASSERT(vm.regs.gp[0] == 0, "registers reset");
    ASSERT(vm.cycle_count == 0, "cycle count reset");
    ASSERT(vm.halted == 0, "halted flag reset");
    // memory regions still present
    FluxMemRegion* stack = flux_mem_get(&vm.mem, "stack");
    ASSERT(stack != NULL, "stack region after reset");
    ASSERT(stack->size == 2048, "stack size preserved");
    flux_vm_free(&vm);
    PASS;
}

static void test_vm_error_strings(void) {
    const char* s;
    s = flux_vm_error_string(FLUX_OK);
    ASSERT_STREQ(s, "OK", "FLUX_OK string");
    s = flux_vm_error_string(FLUX_ERR_HALT);
    ASSERT_STREQ(s, "HALT", "FLUX_ERR_HALT string");
    s = flux_vm_error_string(FLUX_ERR_INVALID_OPCODE);
    ASSERT_STREQ(s, "INVALID_OPCODE", "FLUX_ERR_INVALID_OPCODE string");
    s = flux_vm_error_string(FLUX_ERR_DIV_ZERO);
    ASSERT_STREQ(s, "DIV_ZERO", "FLUX_ERR_DIV_ZERO string");
    s = flux_vm_error_string(FLUX_ERR_STACK_OVERFLOW);
    ASSERT_STREQ(s, "STACK_OVERFLOW", "FLUX_ERR_STACK_OVERFLOW string");
    s = flux_vm_error_string(FLUX_ERR_CYCLE_BUDGET);
    ASSERT_STREQ(s, "CYCLE_BUDGET", "FLUX_ERR_CYCLE_BUDGET string");
    s = flux_vm_error_string(FLUX_ERR_MEMORY);
    ASSERT_STREQ(s, "MEMORY", "FLUX_ERR_MEMORY string");
    // unknown error code
    s = flux_vm_error_string((FluxError)99);
    ASSERT_STREQ(s, "UNKNOWN", "unknown error returns UNKNOWN");
    PASS;
}

static void test_vm_simple_execution(void) {
    // Use a simple bytecode: MOVI R0, 42; HALT
    uint8_t code[] = {
        FLUX_MOVI, 0x00, 42, 0x00,
        FLUX_HALT
    };
    FluxVM vm;
    int r = flux_vm_init(&vm, code, sizeof(code), 4096);
    ASSERT(r == 0, "vm init");
    int64_t cycles = flux_vm_execute(&vm);
    ASSERT(vm.regs.gp[0] == 42, "R0 should be 42 after execution");
    ASSERT(vm.halted == 1, "VM should be halted");
    // cycles > 0
    ASSERT(cycles > 0, "cycles consumed");
    flux_vm_free(&vm);
    PASS;
}

static void test_vm_step(void) {
    uint8_t code[] = {
        FLUX_MOVI, 0x00, 1, 0x00,  // MOVI R0,1
        FLUX_INC,  0x00,            // INC R0
        FLUX_HALT
    };
    FluxVM vm;
    flux_vm_init(&vm, code, sizeof(code), 4096);
    int step1 = flux_vm_step(&vm);
    ASSERT(step1 == 0, "step should return 0 (no error)");
    ASSERT(vm.regs.gp[0] == 1, "first instruction executed");
    int step2 = flux_vm_step(&vm);
    ASSERT(step2 == 0, "second step ok");
    ASSERT(vm.regs.gp[0] == 2, "INC executed");
    // third step will encounter HALT; execution may finish with no error
    int step3 = flux_vm_step(&vm);
    // we only care that it doesn't crash
    flux_vm_free(&vm);
    PASS;
}

static void test_vm_set_a2a(void) {
    FluxVM vm;
    memset(&vm, 0, sizeof(vm));
    flux_vm_set_a2a(&vm, NULL);
    ASSERT(vm.a2a_handler == NULL, "handler set to NULL");
    // dummy handler
    int dummy_handler(FluxVM* vm, uint8_t a, const uint8_t* d, uint16_t l) { return 0; }
    flux_vm_set_a2a(&vm, dummy_handler);
    ASSERT(vm.a2a_handler == dummy_handler, "handler set correctly");
    PASS;
}

int main(void) {
    printf("FLUX Runtime C Implementation Tests\n\n");

    RUN(test_memory_init);
    RUN(test_memory_create);
    RUN(test_memory_read_write);
    RUN(test_memory_transfer);
    RUN(test_memory_destroy);
    RUN(test_registers_init);
    RUN(test_registers_gp_rw);
    RUN(test_registers_fp_rw);
    RUN(test_float_edge_cases);
    RUN(test_opcode_names);
    RUN(test_vm_init_free);
    RUN(test_vm_reset);
    RUN(test_vm_error_strings);
    RUN(test_vm_simple_execution);
    RUN(test_vm_step);
    RUN(test_vm_set_a2a);

    printf("\n%d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
