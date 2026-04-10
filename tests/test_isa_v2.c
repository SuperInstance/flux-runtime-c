#include <stdio.h>
#include <assert.h>
#include "../src/isa_v2.h"

void test_movi_halt() {
    ISA2VM vm;
    uint8_t code[] = { ISA2_MOVI, 0, 42, 0, ISA2_HALT, 0, 0, 0 };
    int32_t r = isa2_execute(&vm, code, sizeof(code));
    assert(r == 42);
    printf("  PASS movi+halt: R0=%d\n", r);
}

void test_add() {
    ISA2VM vm;
    uint8_t code[] = {
        ISA2_MOVI, 0, 3, 0,
        ISA2_MOVI, 1, 4, 0,
        ISA2_IADD, 0, 0, 1,
        ISA2_HALT, 0, 0, 0
    };
    int32_t r = isa2_execute(&vm, code, sizeof(code));
    assert(r == 7);
    printf("  PASS add: 3+4=%d\n", r);
}

void test_mul() {
    ISA2VM vm;
    uint8_t code[] = {
        ISA2_MOVI, 0, 5, 0,
        ISA2_MOVI, 1, 6, 0,
        ISA2_IMUL, 0, 0, 1,
        ISA2_HALT, 0, 0, 0
    };
    int32_t r = isa2_execute(&vm, code, sizeof(code));
    assert(r == 30);
    printf("  PASS mul: 5*6=%d\n", r);
}

void test_sub_div() {
    ISA2VM vm;
    uint8_t code[] = {
        ISA2_MOVI, 0, 20, 0,
        ISA2_MOVI, 1, 8, 0,
        ISA2_ISUB, 2, 0, 1,
        ISA2_IDIV, 3, 2, 1,
        ISA2_MOV, 0, 3, 0,
        ISA2_HALT, 0, 0, 0
    };
    int32_t r = isa2_execute(&vm, code, sizeof(code));
    assert(r == 1);
    printf("  PASS sub/div: (20-8)/8=%d\n", r);
}

void test_inc_dec() {
    ISA2VM vm;
    uint8_t code[] = {
        ISA2_MOVI, 0, 10, 0,
        ISA2_INC, 0, 0, 0,
        ISA2_INC, 0, 0, 0,
        ISA2_DEC, 0, 0, 0,
        ISA2_HALT, 0, 0, 0
    };
    int32_t r = isa2_execute(&vm, code, sizeof(code));
    assert(r == 11);
    printf("  PASS inc/dec: 10+2-1=%d\n", r);
}

void test_push_pop() {
    ISA2VM vm;
    uint8_t code[] = {
        ISA2_MOVI, 0, 99, 0,
        ISA2_PUSH, 0, 0, 0,
        ISA2_MOVI, 0, 0, 0,
        ISA2_POP, 0, 0, 0,
        ISA2_HALT, 0, 0, 0
    };
    int32_t r = isa2_execute(&vm, code, sizeof(code));
    assert(r == 99);
    printf("  PASS push/pop: %d\n", r);
}

void test_factorial() {
    // factorial(5) = 120
    // Loop at pc=8: IMUL, DEC, JNZ back to 8
    ISA2VM vm;
    uint8_t code[] = {
        ISA2_MOVI, 0, 1, 0,      // 0: R0 = 1
        ISA2_MOVI, 1, 5, 0,      // 4: R1 = 5
        ISA2_IMUL, 0, 0, 1,      // 8: R0 *= R1
        ISA2_DEC,  1, 0, 0,      // 12: R1--
        ISA2_JNZ,  1, 0xF4, 0xFF,// 16: if R1!=0, jump -12 → pc=20-12=8
        ISA2_HALT, 0, 0, 0       // 20
    };
    // Trace: R0=1*5=5(R1=4), 5*4=20(R1=3), 20*3=60(R1=2), 60*2=120(R1=1), 120*1=120(R1=0)
    int32_t r = isa2_execute(&vm, code, sizeof(code));
    assert(r == 120);
    printf("  PASS factorial(5)=%d\n", r);
}

void test_fibonacci() {
    // fibonacci(10) = 55
    // R0=fib_n, R1=fib_n-1, R2=counter
    ISA2VM vm;
    uint8_t code[] = {
        ISA2_MOVI, 0, 0, 0,      // 0: R0 = 0
        ISA2_MOVI, 1, 1, 0,      // 4: R1 = 1
        ISA2_MOVI, 2, 10, 0,     // 8: R2 = 10
        // loop at 12:
        ISA2_MOV,  3, 0, 0,      // 12: R3 = R0
        ISA2_IADD, 0, 0, 1,      // 16: R0 = R0 + R1
        ISA2_MOV,  1, 3, 0,      // 20: R1 = R3 (old R0)
        ISA2_DEC,  2, 0, 0,      // 24: R2--
        ISA2_JNZ,  2, 0xEC, 0xFF,// 28: if R2!=0, jump -20 → pc=32-20=12
        ISA2_HALT, 0, 0, 0       // 32
    };
    int32_t r = isa2_execute(&vm, code, sizeof(code));
    assert(r == 55);
    printf("  PASS fibonacci(10)=%d\n", r);
}

void test_negative_immediate() {
    ISA2VM vm;
    uint8_t code[] = {
        ISA2_MOVI, 0, 0xFB, 0xFF, // R0 = -5
        ISA2_MOVI, 1, 10, 0,
        ISA2_IADD, 0, 0, 1,
        ISA2_HALT, 0, 0, 0
    };
    int32_t r = isa2_execute(&vm, code, sizeof(code));
    assert(r == 5);
    printf("  PASS negative: -5+10=%d\n", r);
}

void test_jmp_forward() {
    ISA2VM vm;
    uint8_t code[] = {
        ISA2_MOVI, 0, 0, 0,     // 0
        ISA2_JMP, 0, 8, 0,      // 4: jump +8 → 16
        ISA2_MOVI, 0, 99, 0,    // 8: skipped
        ISA2_HALT, 0, 0, 0,     // 12: skipped
        ISA2_MOVI, 0, 42, 0,    // 16
        ISA2_HALT, 0, 0, 0      // 20
    };
    int32_t r = isa2_execute(&vm, code, sizeof(code));
    assert(r == 42);
    printf("  PASS jmp forward: %d\n", r);
}

int main() {
    printf("ISA v2 Tests:\n");
    test_movi_halt();
    test_add();
    test_mul();
    test_sub_div();
    test_inc_dec();
    test_push_pop();
    test_factorial();
    test_fibonacci();
    test_negative_immediate();
    test_jmp_forward();
    printf("\nAll 10 ISA v2 tests passed!\n");
    return 0;
}
