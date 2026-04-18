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

T("addi"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=10;bc[n++]=0; bc[n++]=0x19;bc[n++]=0;bc[n++]=5;bc[n++]=0; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==15) P(); else F("addi wrong"); flux_vm_free(&vm);

T("addi_negative"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=10;bc[n++]=0; bc[n++]=0x19;bc[n++]=0;bc[n++]=(uint8_t)(-3&0xFF);bc[n++]=(uint8_t)((-3>>8)&0xFF); bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==7) P(); else {printf("R0=%d\n",vm.regs.gp[0]); F("addi_negative wrong");} flux_vm_free(&vm);

T("neg_div"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=(uint8_t)(-17&0xFF);bc[n++]=(uint8_t)((-17>>8)&0xFF); bc[n++]=0x18;bc[n++]=1;bc[n++]=5;bc[n++]=0; bc[n++]=0x23;bc[n++]=0;bc[n++]=1; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==-3) P(); else {printf("R0=%d exp=-3\n",vm.regs.gp[0]); F("neg_div wrong");} flux_vm_free(&vm);

T("neg_mod"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=(uint8_t)(-17&0xFF);bc[n++]=(uint8_t)((-17>>8)&0xFF); bc[n++]=0x18;bc[n++]=1;bc[n++]=5;bc[n++]=0; bc[n++]=0x24;bc[n++]=0;bc[n++]=1; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==-2) P(); else {printf("R0=%d exp=-2\n",vm.regs.gp[0]); F("neg_mod wrong");} flux_vm_free(&vm);

T("mod_zero"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=10;bc[n++]=0; bc[n++]=0x18;bc[n++]=1;bc[n++]=0;bc[n++]=0; bc[n++]=0x24;bc[n++]=0;bc[n++]=1; bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.last_error==3) P(); else F("mod_zero not caught"); flux_vm_free(&vm);

T("inc_16bit_boundary"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=0xFF;bc[n++]=0x7F; /* R0 = 32767 */ bc[n++]=0x08;bc[n++]=0; /* INC R0 → 32768 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==32768) P(); else {printf("R0=%d exp=32768\n",vm.regs.gp[0]); F("inc_16bit_boundary wrong");} flux_vm_free(&vm);

T("dec_16bit_boundary"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=0x00;bc[n++]=0x80; /* R0 = -32768 */ bc[n++]=0x09;bc[n++]=0; /* DEC R0 → -32769 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==-32769) P(); else {printf("R0=%d exp=-32769\n",vm.regs.gp[0]); F("dec_16bit_boundary wrong");} flux_vm_free(&vm);

T("shr_negative"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=(uint8_t)(-16&0xFF);bc[n++]=(uint8_t)((-16>>8)&0xFF); /* R0 = -16 */ bc[n++]=0x18;bc[n++]=1;bc[n++]=2;bc[n++]=0; /* R1 = 2 */ bc[n++]=0x29;bc[n++]=0;bc[n++]=1; /* SHR R0, R1 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==-4) P(); else {printf("R0=%d exp=-4\n",vm.regs.gp[0]); F("shr_negative wrong");} flux_vm_free(&vm);

T("or_xor_not"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=0x0F;bc[n++]=0; /* R0=15 */ bc[n++]=0x18;bc[n++]=1;bc[n++]=0xF0;bc[n++]=0; /* R1=240 */ bc[n++]=0x3A;bc[n++]=2;bc[n++]=0; /* MOV R2, R0 */ bc[n++]=0x26;bc[n++]=2;bc[n++]=1; /* OR R2, R1 → R2=15|240=255 */ bc[n++]=0x27;bc[n++]=3;bc[n++]=2; /* XOR R3, R2 → R3=0^255=255 */ bc[n++]=0x27;bc[n++]=3;bc[n++]=0; /* XOR R3, R0 → R3=255^15=240 */ bc[n++]=0x0A;bc[n++]=3; /* NOT R3 → R3=~240=-241 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[2]==255&&vm.regs.gp[3]==-241) P(); else {printf("R2=%d R3=%d\n",vm.regs.gp[2],vm.regs.gp[3]); F("or_xor_not wrong");} flux_vm_free(&vm);

T("mov_reg"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=42;bc[n++]=0; bc[n++]=0x3A;bc[n++]=5;bc[n++]=0; /* MOV R5, R0 */ bc[n++]=0x3A;bc[n++]=2;bc[n++]=5; /* MOV R2, R5 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==42&&vm.regs.gp[2]==42&&vm.regs.gp[5]==42) P(); else F("mov_reg wrong"); flux_vm_free(&vm);

T("flags_zero"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=0;bc[n++]=0; bc[n++]=0x08;bc[n++]=0; /* INC R0 → R0=1 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(!vm.flag_zero&&vm.flag_sign==0) P(); else F("flags_zero wrong"); flux_vm_free(&vm);

T("flags_after_sub_zero"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=1;bc[n++]=0; bc[n++]=0x18;bc[n++]=1;bc[n++]=1;bc[n++]=0; /* R1=1 */ bc[n++]=0x21;bc[n++]=0;bc[n++]=1; /* SUB R0, R1 → R0=0 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.flag_zero==1&&vm.flag_sign==0) P(); else {printf("z=%d s=%d\n",vm.flag_zero,vm.flag_sign); F("flags_after_sub_zero wrong");} flux_vm_free(&vm);

T("jmp_forward"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=0;bc[n++]=0; /* 0: R0=0 */ bc[n++]=0x43;bc[n++]=0;bc[n++]=8;bc[n++]=0; /* 4: JMP +8 → PC=12 */ bc[n++]=0x18;bc[n++]=0;bc[n++]=99;bc[n++]=0; /* 8: skipped */ bc[n++]=0x18;bc[n++]=1;bc[n++]=77;bc[n++]=0; /* 12: skipped */ bc[n++]=0x18;bc[n++]=0;bc[n++]=55;bc[n++]=0; /* 16: R0=55 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==55&&vm.regs.gp[1]==0) P(); else {printf("R0=%d R1=%d\n",vm.regs.gp[0],vm.regs.gp[1]); F("jmp_forward wrong");} flux_vm_free(&vm);

T("jlt_taken"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=(uint8_t)(-5&0xFF);bc[n++]=(uint8_t)((-5>>8)&0xFF); /* R0=-5 */ bc[n++]=0x3E;bc[n++]=0;bc[n++]=4;bc[n++]=0; /* JLT R0, +4 → taken */ bc[n++]=0x18;bc[n++]=1;bc[n++]=99;bc[n++]=0; /* skipped */ bc[n++]=0x18;bc[n++]=1;bc[n++]=42;bc[n++]=0; /* R1=42 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==-5&&vm.regs.gp[1]==42) P(); else F("jlt_taken wrong"); flux_vm_free(&vm);

T("jlt_not_taken"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=5;bc[n++]=0; /* R0=5 */ bc[n++]=0x3E;bc[n++]=0;bc[n++]=4;bc[n++]=0; /* JLT R0, +4 → not taken (5≥0) */ bc[n++]=0x18;bc[n++]=1;bc[n++]=99;bc[n++]=0; /* R1=99 (executed) */ bc[n++]=0x00; /* HALT */ bc[n++]=0x18;bc[n++]=1;bc[n++]=42;bc[n++]=0; /* R1=42 (skipped) */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[1]==99) P(); else F("jlt_not_taken wrong"); flux_vm_free(&vm);

T("jgt_taken"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=5;bc[n++]=0; /* R0=5 */ bc[n++]=0x3F;bc[n++]=0;bc[n++]=4;bc[n++]=0; /* JGT R0, +4 → taken */ bc[n++]=0x18;bc[n++]=1;bc[n++]=99;bc[n++]=0; /* skipped */ bc[n++]=0x18;bc[n++]=1;bc[n++]=42;bc[n++]=0; /* R1=42 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[1]==42) P(); else F("jgt_taken wrong"); flux_vm_free(&vm);

T("multi_push_pop"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=10;bc[n++]=0; bc[n++]=0x18;bc[n++]=1;bc[n++]=20;bc[n++]=0; bc[n++]=0x18;bc[n++]=2;bc[n++]=30;bc[n++]=0; bc[n++]=0x0C;bc[n++]=0; /* PUSH R0 */ bc[n++]=0x0C;bc[n++]=1; /* PUSH R1 */ bc[n++]=0x0C;bc[n++]=2; /* PUSH R2 */ bc[n++]=0x18;bc[n++]=3;bc[n++]=0;bc[n++]=0; bc[n++]=0x18;bc[n++]=4;bc[n++]=0;bc[n++]=0; bc[n++]=0x18;bc[n++]=5;bc[n++]=0;bc[n++]=0; bc[n++]=0x0D;bc[n++]=5; /* POP R5=30 */ bc[n++]=0x0D;bc[n++]=4; /* POP R4=20 */ bc[n++]=0x0D;bc[n++]=3; /* POP R3=10 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[3]==10&&vm.regs.gp[4]==20&&vm.regs.gp[5]==30) P(); else {printf("R3=%d R4=%d R5=%d\n",vm.regs.gp[3],vm.regs.gp[4],vm.regs.gp[5]); F("multi_push_pop wrong");} flux_vm_free(&vm);

T("reg_bounds_clamp"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=42;bc[n++]=0; /* R0=42 */ bc[n++]=0x3A;bc[n++]=0x20;bc[n++]=0; /* MOV R32, R0 → should clamp to R0 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==42) P(); else F("reg_bounds_clamp: R0 corrupted"); flux_vm_free(&vm);

T("cmp_lt_flags"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=3;bc[n++]=0; bc[n++]=0x18;bc[n++]=1;bc[n++]=7;bc[n++]=0; bc[n++]=0x2D;bc[n++]=0;bc[n++]=1; /* CMP_LT R0,R1 → 3<7 → zero=0, sign=1 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.flag_zero==0&&vm.flag_sign==1) P(); else {printf("z=%d s=%d\n",vm.flag_zero,vm.flag_sign); F("cmp_lt_flags wrong");} flux_vm_free(&vm);

T("cmp_gt_flags"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=7;bc[n++]=0; bc[n++]=0x18;bc[n++]=1;bc[n++]=3;bc[n++]=0; bc[n++]=0x2E;bc[n++]=0;bc[n++]=1; /* CMP_GT R0,R1 → 7>3 → zero=0, sign=0 */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.flag_zero==0&&vm.flag_sign==0) P(); else {printf("z=%d s=%d\n",vm.flag_zero,vm.flag_sign); F("cmp_gt_flags wrong");} flux_vm_free(&vm);

T("loop_factorial"); /* Compute 5! = 120 using LOOP */
n=0; bc[n++]=0x18;bc[n++]=3;bc[n++]=5;bc[n++]=0; /* R3=5 (counter) */ bc[n++]=0x18;bc[n++]=4;bc[n++]=1;bc[n++]=0; /* R4=1 (result) */
/* loop: */ bc[n++]=0x22;bc[n++]=4;bc[n++]=3; /* MUL R4, R3 */ bc[n++]=0x09;bc[n++]=3; /* DEC R3 */
/* JNZ R3, back to MUL: offset = -(2+2) = -4 from end of JNZ */
bc[n++]=0x3D;bc[n++]=3;bc[n++]=(uint8_t)(-9&0xFF);bc[n++]=(uint8_t)((-9>>8)&0xFF); /* JNZ R3, -9 → back to MUL */
bc[n++]=0x00;
flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[4]==120) P(); else {printf("R4=%d exp=120\n",vm.regs.gp[4]); F("loop_factorial wrong");} flux_vm_free(&vm);

T("fibonacci_10"); /* Fibonacci(10)=55 using R0,R1,R2 */
n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=0;bc[n++]=0; /* R0=0 */ bc[n++]=0x18;bc[n++]=1;bc[n++]=1;bc[n++]=0; /* R1=1 */ bc[n++]=0x18;bc[n++]=2;bc[n++]=10;bc[n++]=0; /* R2=10 */
/* loop: */ bc[n++]=0x3A;bc[n++]=3;bc[n++]=0; /* MOV R3, R0 */ bc[n++]=0x20;bc[n++]=0;bc[n++]=1; /* ADD R0, R1 */ bc[n++]=0x3A;bc[n++]=1;bc[n++]=3; /* MOV R1, R3 */ bc[n++]=0x09;bc[n++]=2; /* DEC R2 */
/* JNZ R2, back to MOV: */
bc[n++]=0x3D;bc[n++]=2;bc[n++]=(uint8_t)(-15&0xFF);bc[n++]=(uint8_t)((-15>>8)&0xFF); /* JNZ R2, -15 → back to MOV */
bc[n++]=0x00;
flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==55) P(); else {printf("R0=%d exp=55\n",vm.regs.gp[0]); F("fibonacci_10 wrong");} flux_vm_free(&vm);

T("movi_large"); n=0; bc[n++]=0x18;bc[n++]=0;bc[n++]=0x00;bc[n++]=0x80; /* R0 = -32768 (0x8000) */ bc[n++]=0x00; flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==-32768) P(); else {printf("R0=%d exp=-32768\n",vm.regs.gp[0]); F("movi_large wrong");} flux_vm_free(&vm);

T("nested_call_ret"); /* Call a function twice via CALL/RET */
n=0;
bc[n++]=0x18;bc[n++]=0;bc[n++]=5;bc[n++]=0; /* MOVI R0, 5 (offset 0) */
bc[n++]=0x45;bc[n++]=1;bc[n++]=5;bc[n++]=0; /* CALL R1, +5 (offset 4): saves 8, PC=8+5=13 */
bc[n++]=0x45;bc[n++]=1;bc[n++]=1;bc[n++]=0; /* CALL R1, +1 (offset 8): saves 12, PC=12+1=13 */
bc[n++]=0x00; /* HALT (offset 12) */
bc[n++]=0x08;bc[n++]=0; /* INC R0 (offset 13) */
bc[n++]=0x02;bc[n++]=0; /* RET (offset 15) */
flux_vm_init(&vm,bc,n,4096); flux_vm_execute(&vm); if(vm.regs.gp[0]==7) P(); else {printf("R0=%d exp=7\n",vm.regs.gp[0]); F("nested_call_ret wrong");} flux_vm_free(&vm);

printf("\n%d/%d tests passed\n",pass,pass+fail);
return fail;
}
