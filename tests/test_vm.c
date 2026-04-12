#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "flux.h"
static int pass=0,fail=0;
#define T(n) printf("  %s... ", n); fflush(stdout)
#define P() do { printf("PASS\n"); pass++; } while(0)
#define F(m) do { printf("FAIL: %s\n", m); fail++; } while(0)
static int a2a_hit=0;
static int acb(FluxVM*v,uint8_t o,const uint8_t*d,uint16_t l){a2a_hit=1;return 0;}

int main(){
printf("FLUX VM Tests\n\n");
FluxVM vm; int n; uint8_t bc[128];

T("nop"); uint8_t t1[]={FLUX_NOP,FLUX_HALT}; flux_vm_init(&vm,t1,2,4096); flux_vm_execute(&vm); flux_vm_free(&vm); P();

T("movi"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=10;bc[n++]=0;bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==10) P(); else F("R0 wrong"); flux_vm_free(&vm);

T("add"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=10;bc[n++]=0; bc[n++]=0x18;bc[n++]=1;bc[n++]=25;bc[n++]=0; bc[n++]=0x20;bc[n++]=0;bc[n++]=1; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==35) P(); else F("R0 wrong"); flux_vm_free(&vm);

T("mul_sub"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=6;bc[n++]=0; bc[n++]=0x18;bc[n++]=1;bc[n++]=7;bc[n++]=0; bc[n++]=0x22;bc[n++]=0;bc[n++]=1; bc[n++]=0x18;bc[n++]=1;bc[n++]=2;bc[n++]=0; bc[n++]=0x21;bc[n++]=0;bc[n++]=1; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==40) P(); else F("R0 wrong"); flux_vm_free(&vm);

T("div_mod"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=17;bc[n++]=0; bc[n++]=0x18;bc[n++]=1;bc[n++]=5;bc[n++]=0; bc[n++]=0x3A;bc[n++]=2;bc[n++]=0; bc[n++]=0x23;bc[n++]=2;bc[n++]=1; bc[n++]=0x3A;bc[n++]=3;bc[n++]=0; bc[n++]=0x24;bc[n++]=3;bc[n++]=1; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[2]==3&&vm.regs.gp[3]==2) P(); else F("div/mod wrong"); flux_vm_free(&vm);

T("neg"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=7;bc[n++]=0; bc[n++]=0x0B;bc[n++]=0; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==-7) P(); else F("neg wrong"); flux_vm_free(&vm);

T("inc_dec"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=10;bc[n++]=0; bc[n++]=0x08;bc[n++]=0; bc[n++]=0x09;bc[n++]=0; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==10) P(); else F("inc_dec wrong"); flux_vm_free(&vm);

T("jz"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=0;bc[n++]=0; bc[n++]=0x3C;bc[n++]=0;bc[n++]=4;bc[n++]=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=99;bc[n++]=0; bc[n++]=0x18;bc[n++]=1;bc[n++]=42;bc[n++]=0; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==0&&vm.regs.gp[1]==42) P(); else F("jz wrong"); flux_vm_free(&vm);

T("jnz"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=5;bc[n++]=0; bc[n++]=0x3D;bc[n++]=0;bc[n++]=4;bc[n++]=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=0;bc[n++]=0; bc[n++]=0x18;bc[n++]=1;bc[n++]=99;bc[n++]=0; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==5&&vm.regs.gp[1]==99) P(); else F("jnz wrong"); flux_vm_free(&vm);

T("push_pop"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=42;bc[n++]=0; bc[n++]=0x0C;bc[n++]=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=0;bc[n++]=0; bc[n++]=0x0D;bc[n++]=1; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==0&&vm.regs.gp[1]==42) P(); else F("push_pop wrong"); flux_vm_free(&vm);

T("dup"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=7;bc[n++]=0; bc[n++]=0x0C;bc[n++]=0; bc[n++]=0x90; bc[n++]=0x0D;bc[n++]=1; bc[n++]=0x0D;bc[n++]=2; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[1]==7&&vm.regs.gp[2]==7) P(); else F("dup wrong"); flux_vm_free(&vm);

T("bitwise"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=0xFF;bc[n++]=0; bc[n++]=0x18;bc[n++]=1;bc[n++]=0x0F;bc[n++]=0; bc[n++]=0x3A;bc[n++]=2;bc[n++]=0; bc[n++]=0x25;bc[n++]=2;bc[n++]=1; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[2]==15) P(); else F("and wrong"); flux_vm_free(&vm);

T("cmp_je"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=10;bc[n++]=0; bc[n++]=0x18;bc[n++]=1;bc[n++]=10;bc[n++]=0; bc[n++]=0x2C;bc[n++]=0;bc[n++]=1; bc[n++]=0x3C;bc[n++]=0;bc[n++]=3;bc[n++]=0; bc[n++]=0x18;bc[n++]=2;bc[n++]=0;bc[n++]=0; bc[n++]=0x18;bc[n++]=2;bc[n++]=1;bc[n++]=0; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[2]==1) P(); else F("cmp_je wrong"); flux_vm_free(&vm);

T("shift"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=1;bc[n++]=0; bc[n++]=0x18;bc[n++]=1;bc[n++]=4;bc[n++]=0; bc[n++]=0x28;bc[n++]=0;bc[n++]=1; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==16) P(); else F("shift wrong"); flux_vm_free(&vm);

T("call_ret"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=9;bc[n++]=0; bc[n++]=0x45;bc[n++]=1;bc[n++]=1;bc[n++]=0; bc[n++]=0x00; bc[n++]=0x08;bc[n++]=0; bc[n++]=0x02;bc[n++]=1; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==10) P(); else {printf("R0=%d\n",vm.regs.gp[0]); F("call_ret wrong");} flux_vm_free(&vm);

T("cast"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=100;bc[n++]=0; bc[n++]=0x36;bc[n++]=0;bc[n++]=0; bc[n++]=0x36;bc[n++]=0;bc[n++]=1; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==100) P(); else F("cast wrong"); flux_vm_free(&vm);

T("float"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=100;bc[n++]=0; bc[n++]=0x9E;bc[n++]=0;bc[n++]=0; /* GP→FP: F0=100.0 */ bc[n++]=0xA3;bc[n++]=0; /* F0=-100.0 */ bc[n++]=0x9E;bc[n++]=0;bc[n++]=1; /* FP→GP: R0=-100 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==-100) P(); else {printf("R0=%d\n",vm.regs.gp[0]); F("float wrong");} flux_vm_free(&vm);

T("a2a"); n=0; bc[n++]=0x50;bc[n++]=1;bc[n++]=0;bc[n++]=0x42;bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_set_a2a(&vm,acb); a2a_hit=0; flux_vm_execute(&vm); if(a2a_hit) P(); else F("a2a not called"); flux_vm_free(&vm);

T("box"); n=0; bc[n++]=0x9F;bc[n++]=5;bc[n++]=0; bc[n++]=0;bc[n++]=0x2A;bc[n++]=0;bc[n++]=0;bc[n++]=0; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.box_table[0].int_val==42) P(); else F("box wrong"); flux_vm_free(&vm);

T("halt"); n=0; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.halted) P(); else F("not halted"); flux_vm_free(&vm);

T("div_zero"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=1;bc[n++]=0; bc[n++]=0x18;bc[n++]=1;bc[n++]=0;bc[n++]=0; bc[n++]=0x23;bc[n++]=0;bc[n++]=1; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.last_error==3) P(); else F("not div_zero"); flux_vm_free(&vm);

T("cycle_budget"); bc[0]=0x43;bc[1]=1;bc[2]=(uint8_t)(-4&0xFF);bc[3]=(uint8_t)((-4>>8)&0xFF); flux_vm_init(&vm,bc,4,4096); vm.max_cycles=100; flux_vm_execute(&vm); if(vm.last_error==11) P(); else F("not cycle_budget"); flux_vm_free(&vm);

printf("\n%d/%d tests passed\n",pass,pass+fail);
return fail;
}
