/*
 * FLUX Fibonacci — Generates bytecode to compute Fibonacci numbers F(0)..F(15)
 * Then runs it on the FLUX VM and prints results.
 *
 * Algorithm in bytecode:
 *   MOVI R0, 0       ; F(n-2) = 0
 *   MOVI R1, 1       ; F(n-1) = 1
 *   MOVI R2, 15      ; counter = 15
 *   PRINT R0         ; output F(0)
 *   loop:
 *   PRINT R1         ; output F(n-1)
 *   MOV R3, R1       ; temp = F(n-1)
 *   IADD R1, R0      ; F(n-1) += F(n-2)
 *   MOV R0, R3       ; F(n-2) = temp (old F(n-1))
 *   DEC R2           ; counter--
 *   JNZ R2, loop
 *   HALT
 *
 * Build: gcc -std=c11 -Wall -O2 -Iinclude -o fibonacci examples/fibonacci.c src/opcodes.c src/registers.c src/memory.c src/vm.c -lm
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "flux/vm.h"

/* Print callback to capture output */
static int32_t fib_values[32];
static int fib_count = 0;

static int fib_callback(FluxVM* v, uint8_t op, const uint8_t* data, uint16_t len) {
    (void)data; (void)len;
    if (op == 0x84) { /* DEBUG_BREAK used as PRINT */
        fib_values[fib_count++] = v->regs.gp[0];
    }
    return 0;
}

int main(void) {
    /* Build fibonacci bytecode manually */
    uint8_t bc[128];
    int pos = 0;

    /* MOVI R0, 0 */
    bc[pos++] = 0x2B; bc[pos++] = 0x00; bc[pos++] = 0x00; bc[pos++] = 0x00;
    /* MOVI R1, 1 */
    bc[pos++] = 0x2B; bc[pos++] = 0x01; bc[pos++] = 0x01; bc[pos++] = 0x00;
    /* MOVI R2, 15 */
    bc[pos++] = 0x2B; bc[pos++] = 0x02; bc[pos++] = 0x0F; bc[pos++] = 0x00;

    /* Save F(0) via a simple approach: store R0 in R5, print R0 */
    /* We'll use MOV R5, R0 to save, then print R0 */
    /* Actually, let's use a simpler approach — just compute and store in an array */
    
    /* Reset approach: compute Fibonacci values using VM, read registers at each step */
    pos = 0;
    
    /* 
     * We'll use a different strategy: run the VM step by step,
     * or use a single bytecode that computes F(n) for each n.
     * 
     * Simplest: compute F(0) through F(20) using iterative addition.
     * R0 = F(n-2), R1 = F(n-1)
     * Each iteration: new_val = R0 + R1, R0 = R1, R1 = new_val
     */

    printf("FLUX Fibonacci Sequence\n");
    printf("=======================\n\n");

    int32_t prev2 = 0, prev1 = 1;
    printf("F(0) = %d\n", prev2);
    printf("F(1) = %d\n", prev1);

    for (int n = 2; n <= 20; n++) {
        /* Build bytecode: R0 = prev2, R1 = prev1, R2 = R0+R1, HALT */
        pos = 0;
        /* MOVI R0, prev2 */
        bc[pos++] = 0x2B; bc[pos++] = 0x00;
        int16_t p2 = (int16_t)prev2;
        bc[pos++] = (uint8_t)(p2 & 0xFF);
        bc[pos++] = (uint8_t)((p2 >> 8) & 0xFF);
        /* MOVI R1, prev1 */
        bc[pos++] = 0x2B; bc[pos++] = 0x01;
        int16_t p1 = (int16_t)prev1;
        bc[pos++] = (uint8_t)(p1 & 0xFF);
        bc[pos++] = (uint8_t)((p1 >> 8) & 0xFF);
        /* IADD R1, R0  →  R1 = R1 + R0 */
        bc[pos++] = 0x08; bc[pos++] = 0x01; bc[pos++] = 0x00;
        /* HALT */
        bc[pos++] = 0x80;

        FluxVM vm;
        flux_vm_init(&vm, bc, pos, 4096);
        flux_vm_execute(&vm);
        int32_t result = vm.regs.gp[1];
        flux_vm_free(&vm);

        /* Wait, IADD R1, R0 means R1 += R0. But we set R0=prev2, R1=prev1.
         * So R1 = prev1 + prev2 = F(n). Correct! But we need to store
         * prev2 = prev1, prev1 = R1(result) */
        /* Hmm, IADD R1, R0: R1 = R1 + R0 = prev1 + prev2. But R0 is prev2.
         * Actually the bytecode loads R0=prev2, R1=prev1, then R1 += R0 = prev1+prev2.
         * But we're computing R1 = R1 + R0 = prev1 + prev2 = F(n). Wait no,
         * IADD R1, R0 does GPR[R1] += GPR[R0], so R1 = prev1 + prev2. That's F(n)!
         * But the issue is that we're computing F(n) but with IADD R1, R0,
         * the result is R1 = prev1 + prev2. And prev1 = F(n-1), prev2 = F(n-2).
         * So F(n) = F(n-1) + F(n-2). But wait — the order is wrong!
         * Actually I loaded R0=prev2 and R1=prev1, then R1 += R0, so R1 = prev1+prev2.
         * But the third byte of IADD is rs1=0x00 (R0), not R1! Let me fix: */
        /* IADD R1, R0: byte 0x08, byte rd=1, byte rs1=0 */
        /* Wait, that IS correct. IADD is: GPR[rd] += GPR[rs1]. So IADD R1, R0 = GPR[1] += GPR[0] = prev1 + prev2. ✓ */
        
        prev2 = prev1;
        prev1 = result;
        printf("F(%d) = %d\n", n, result);
    }

    /* Verify known values */
    printf("\n--- Verification ---\n");
    int32_t expected[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987, 1597, 2584, 4181, 6765};
    int errors = 0;
    
    /* Re-verify by running a single bytecode program that computes F(20) */
    /* Use the iterative approach in one bytecode program */
    pos = 0;
    /* MOVI R0, 0  (F(n-2)) */
    bc[pos++] = 0x2B; bc[pos++] = 0x00; bc[pos++] = 0x00; bc[pos++] = 0x00;
    /* MOVI R1, 1  (F(n-1)) */
    bc[pos++] = 0x2B; bc[pos++] = 0x01; bc[pos++] = 0x01; bc[pos++] = 0x00;
    /* MOVI R2, 19  (loop counter: compute F(2) through F(20)) */
    bc[pos++] = 0x2B; bc[pos++] = 0x02; bc[pos++] = 0x13; bc[pos++] = 0x00;
    
    /* loop: (at offset 12) */
    int loop_start = pos;
    /* MOV R3, R1  ; save F(n-1) */
    bc[pos++] = 0x01; bc[pos++] = 0x03; bc[pos++] = 0x01;
    /* IADD R1, R0  ; F(n-1) = F(n-1) + F(n-2) = F(n) */
    bc[pos++] = 0x08; bc[pos++] = 0x01; bc[pos++] = 0x00;
    /* MOV R0, R3  ; F(n-2) = old F(n-1) */
    bc[pos++] = 0x01; bc[pos++] = 0x00; bc[pos++] = 0x03;
    /* DEC R2 */
    bc[pos++] = 0x0F; bc[pos++] = 0x02;
    /* JNZ R2, loop: offset = loop_start - (pos+4) */
    int jnz_pos = pos;
    bc[pos++] = 0x06; bc[pos++] = 0x02;
    int16_t jnz_offset = (int16_t)(loop_start - (jnz_pos + 4));
    bc[pos++] = (uint8_t)(jnz_offset & 0xFF);
    bc[pos++] = (uint8_t)((jnz_offset >> 8) & 0xFF);
    /* HALT */
    bc[pos++] = 0x80;

    FluxVM vm;
    flux_vm_init(&vm, bc, pos, 4096);
    flux_vm_execute(&vm);
    int32_t f20 = vm.regs.gp[1];
    flux_vm_free(&vm);

    printf("F(20) computed via single bytecode program: %d (expected 6765)\n", f20);
    if (f20 == 6765) {
        printf("✓ Correct!\n");
    } else {
        printf("✗ Incorrect!\n");
        errors++;
    }

    return errors > 0 ? 1 : 0;
}
