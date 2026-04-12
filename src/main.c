#include "flux.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char** argv) {
    if (argc < 2) { printf("flux-runtime <bytecode.bin>\\n"); return 1; }
    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* bc = malloc(sz); fread(bc, 1, sz, f); fclose(f);
    FluxVM vm;
    flux_vm_init(&vm, bc, sz, 0);
    int64_t rc = flux_vm_execute(&vm);
    printf("Executed %ld cycles, error: %s\\n", (long)rc, flux_vm_error_string(vm.last_error));
    printf("R0=%d R1=%d PC=%u\\n", vm.regs.gp[0], vm.regs.gp[1], vm.regs.pc);
    flux_vm_free(&vm); free(bc);
    return 0;
}
