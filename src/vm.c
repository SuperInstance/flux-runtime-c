/*
 * FLUX Runtime — Micro-VM Interpreter (C11, zero deps, ARM64 safe)
 * Copyright (c) 2024 SuperInstance (DiGennaro et al.), MIT License
 * C rewrite by Lucineer (DiGennaro et al.)
 */
#include "flux/vm.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static inline uint8_t f8(FluxVM* v) { return v->bytecode[v->regs.pc++]; }
static inline uint8_t gr8(FluxVM* v) { uint8_t r=v->bytecode[v->regs.pc++]; return r<FLUX_GP_COUNT?r:0; }
static inline int16_t fi16(FluxVM* v) { uint8_t l=v->bytecode[v->regs.pc++], h=v->bytecode[v->regs.pc++]; return (int16_t)(l|(h<<8)); }
static inline uint16_t fu16(FluxVM* v) { uint8_t l=v->bytecode[v->regs.pc++], h=v->bytecode[v->regs.pc++]; return (uint16_t)(l|(h<<8)); }

static inline void sf(FluxVM* v, int32_t r) { v->flag_zero=(r==0); v->flag_sign=(r<0); }
static inline void scf(FluxVM* v, int32_t a, int32_t b) { v->flag_zero=(a==b); v->flag_sign=(a<b); }

static inline int spush(FluxVM* v, int32_t val) {
    FluxMemRegion* s = flux_mem_get(&v->mem, "stack");
    if (!s || v->regs.sp < 4) return 4;
    v->regs.sp -= 4; flux_mem_write_i32(s, v->regs.sp, val); return 0;
}
static inline int32_t spop(FluxVM* v) {
    FluxMemRegion* s = flux_mem_get(&v->mem, "stack");
    int32_t val = s ? flux_mem_read_i32(s, v->regs.sp) : 0;
    v->regs.sp += 4; return val;
}

#define ERR(e) do { v->last_error=(e); v->running=0; return -(int64_t)(e); } while(0)
#define GPR (v->regs.gp)

int flux_vm_init(FluxVM* v, const uint8_t* bc, uint32_t len, uint32_t ms) {
    if (!ms) ms = 65536;
    memset(v, 0, sizeof(*v));
    v->bytecode = bc; v->bytecode_len = len; v->max_cycles = 10000000;
    flux_regs_init(&v->regs); flux_mem_init(&v->mem);
    flux_mem_create(&v->mem, "stack", ms, "system");
    flux_mem_create(&v->mem, "heap", ms, "system");
    v->regs.sp = ms; GPR[11] = (int32_t)ms;
    return 0;
}
void flux_vm_free(FluxVM* v) { flux_mem_free(&v->mem); }
void flux_vm_reset(FluxVM* v) {
    uint32_t ms = 0;
    FluxMemRegion* s = flux_mem_get(&v->mem, "stack");
    if (s) ms = (uint32_t)s->size;
    flux_mem_free(&v->mem); flux_regs_init(&v->regs); flux_mem_init(&v->mem);
    if (ms) { flux_mem_create(&v->mem,"stack",ms,"system"); flux_mem_create(&v->mem,"heap",ms,"system"); v->regs.sp=ms; GPR[11]=(int32_t)ms; }
    v->cycle_count=v->box_count=v->frame_count=0; v->running=v->halted=0;
}

const char* flux_vm_error_string(FluxError e) {
    switch(e) {
        case FLUX_OK: return "OK"; case FLUX_ERR_HALT: return "HALT";
        case FLUX_ERR_INVALID_OPCODE: return "INVALID_OPCODE"; case FLUX_ERR_DIV_ZERO: return "DIV_ZERO";
        case FLUX_ERR_STACK_OVERFLOW: return "STACK_OVERFLOW"; case FLUX_ERR_CYCLE_BUDGET: return "CYCLE_BUDGET";
        default: return "UNKNOWN";
    }
}

int64_t flux_vm_execute(FluxVM* v) {
    v->running = 1; v->halted = 0;
    uint8_t op, rd, rs1, rs2; int16_t imm; uint16_t len;
    const uint8_t* data; FluxMemRegion* h;

    while (v->running && v->cycle_count < v->max_cycles) {
        op = v->bytecode[v->regs.pc++];
        v->cycle_count++;

        switch(op) {
        case FLUX_NOP: break;
        case FLUX_MOV: rd=gr8(v); rs1=gr8(v); GPR[rd]=GPR[rs1]; break;
        case FLUX_LOAD: rd=gr8(v); rs1=f8(v); { FluxMemRegion* s=flux_mem_get(&v->mem,"stack"); if(s) GPR[rd]=flux_mem_read_i32(s,rs1*4); } break;
        case FLUX_STORE: rd=f8(v); rs1=gr8(v); { FluxMemRegion* s=flux_mem_get(&v->mem,"stack"); if(s) flux_mem_write_i32(s,rd*4,GPR[rs1]); } break;
        case FLUX_JMP: rd=f8(v); imm=fi16(v); v->regs.pc+=imm; break;
        case FLUX_JZ: rd=gr8(v); imm=fi16(v); { if(GPR[rd]==0) v->regs.pc+=imm; } break;
        case FLUX_JNZ: rd=gr8(v); imm=fi16(v); { if(GPR[rd]!=0) v->regs.pc+=imm; } break;
        case FLUX_JLT: rd=gr8(v); imm=fi16(v); { if(GPR[rd]<0) v->regs.pc+=imm; } break;
        case FLUX_JGT: rd=gr8(v); imm=fi16(v); { if(GPR[rd]>0) v->regs.pc+=imm; } break;
        case FLUX_CALL: rd=f8(v); imm=fi16(v); if(v->frame_count>=256) ERR(4); v->frame_stack[v->frame_count++]=v->regs.pc; v->regs.pc+=imm; break;

        case FLUX_ADD: rd=gr8(v); rs1=gr8(v); { int32_t r=GPR[rd]+GPR[rs1]; sf(v,r); GPR[rd]=r; } break;
        case FLUX_SUB: rd=gr8(v); rs1=gr8(v); { int32_t r=GPR[rd]-GPR[rs1]; sf(v,r); GPR[rd]=r; } break;
        case FLUX_MUL: rd=gr8(v); rs1=gr8(v); { int32_t r=GPR[rd]*GPR[rs1]; sf(v,r); GPR[rd]=r; } break;
        case FLUX_DIV: rd=gr8(v); rs1=gr8(v); if(!GPR[rs1]) ERR(3); GPR[rd]=GPR[rd]/GPR[rs1]; break;
        case FLUX_MOD: rd=gr8(v); rs1=gr8(v); if(!GPR[rs1]) ERR(3); GPR[rd]=GPR[rd]%GPR[rs1]; break;
        case FLUX_NEG: rd=gr8(v); { int32_t r=-GPR[rd]; sf(v,r); GPR[rd]=r; } break;
        case FLUX_INC: rd=gr8(v); { int32_t r=GPR[rd]+1; sf(v,r); GPR[rd]=r; } break;
        case FLUX_DEC: rd=gr8(v); { int32_t r=GPR[rd]-1; sf(v,r); GPR[rd]=r; } break;

        case FLUX_AND: rd=gr8(v); rs1=gr8(v); GPR[rd]&=GPR[rs1]; break;
        case FLUX_OR:  rd=gr8(v); rs1=gr8(v); GPR[rd]|=GPR[rs1]; break;
        case FLUX_XOR: rd=gr8(v); rs1=gr8(v); GPR[rd]^=GPR[rs1]; break;
        case FLUX_NOT: rd=gr8(v); GPR[rd]=~GPR[rd]; break;
        case FLUX_SHL: rd=gr8(v); rs1=gr8(v); GPR[rd]<<=GPR[rs1]; break;
        case FLUX_SHR: rd=gr8(v); rs1=gr8(v); GPR[rd]>>=GPR[rs1]; break;

        case FLUX_CMP_EQ: rd=gr8(v); rs1=gr8(v); scf(v,GPR[rd],GPR[rs1]); break;
        case FLUX_CMP_LT: rd=gr8(v); rs1=gr8(v); scf(v,GPR[rd],GPR[rs1]); break;
        case FLUX_CMP_GT: rd=gr8(v); rs1=gr8(v); scf(v,GPR[rd],GPR[rs1]); break;
        case FLUX_CMP_NE: rd=gr8(v); rs1=gr8(v); scf(v,GPR[rd],GPR[rs1]); break;

        case FLUX_PUSH: rd=gr8(v); { int rc=spush(v,GPR[rd]); if(rc) ERR(rc); } break;
        case FLUX_POP: rd=gr8(v); GPR[rd]=spop(v); break;
        case FLUX_DUP: { int32_t val=spop(v); if(spush(v,val)) ERR(4); if(spush(v,val)) ERR(4); } break;
        case FLUX_SWP: { FluxMemRegion* s=flux_mem_get(&v->mem,"stack"); if(s){int32_t a=flux_mem_read_i32(s,v->regs.sp),b=flux_mem_read_i32(s,v->regs.sp+4); flux_mem_write_i32(s,v->regs.sp,b); flux_mem_write_i32(s,v->regs.sp+4,a);} } break;
        case FLUX_RET: rd=f8(v); if(!v->frame_count) ERR(5); v->regs.pc=v->frame_stack[--v->frame_count]; break;
        case FLUX_LOOP: rd=gr8(v); rs1=gr8(v); GPR[rd]+=GPR[rs1]; break;

        case FLUX_JAL: rd=gr8(v); if(v->frame_count>=256) ERR(4); v->frame_stack[v->frame_count++]=v->regs.pc; v->regs.pc=(uint32_t)GPR[rd]; break;
        case FLUX_TAILCALL: rd=gr8(v); v->regs.pc=(uint32_t)GPR[rd]; break;
        case FLUX_MOVI: rd=gr8(v); imm=fi16(v); GPR[rd]=imm; break;
        case FLUX_ADDI: rd=gr8(v); imm=fi16(v); { int32_t r=GPR[rd]+imm; sf(v,r); GPR[rd]=r; } break;

        case FLUX_SWAP: { FluxMemRegion* s=flux_mem_get(&v->mem,"stack"); if(s){int32_t a=flux_mem_read_i32(s,v->regs.sp),b=flux_mem_read_i32(s,v->regs.sp+4); flux_mem_write_i32(s,v->regs.sp,b); flux_mem_write_i32(s,v->regs.sp+4,a);} } break;
        case FLUX_TEST: rd=gr8(v); rs1=gr8(v); sf(v,GPR[rd]&GPR[rs1]); break;
        case FLUX_SETCC: rd=gr8(v); rs1=f8(v); GPR[rd]=(rs1==0)?v->flag_zero:(rs1==1)?v->flag_sign:0; break;
        case FLUX_ENTER: rd=f8(v); if(v->frame_count>=256) ERR(4); v->frame_stack[v->frame_count++]=v->regs.sp; v->regs.sp-=rd*4; break;
        case FLUX_LEAVE: rd=f8(v); if(!v->frame_count) ERR(5); v->regs.sp=v->frame_stack[--v->frame_count]+rd*4; break;
        case FLUX_ALLOCA: rd=gr8(v); rs1=gr8(v); GPR[rd]+=GPR[rs1]; break;

        case FLUX_REGION_CREATE: { len=fu16(v); data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; uint8_t nl=data[0]; char nm[64]={0}; memcpy(nm,data+1,nl<63?nl:63); uint32_t sz; memcpy(&sz,data+1+nl,4); flux_mem_create(&v->mem,nm,sz,"agent"); } break;
        case FLUX_REGION_DESTROY: { len=fu16(v); data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; char nm[64]={0}; memcpy(nm,data,len<63?len:63); flux_mem_destroy(&v->mem,nm); } break;
        case FLUX_REGION_TRANSFER: { len=fu16(v); data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; char s[32]={0},d[32]={0}; uint8_t sl=data[0]; memcpy(s,data+1,sl<31?sl:31); uint8_t dl=data[1+sl]; memcpy(d,data+2+sl,dl<31?dl:31); flux_mem_transfer(&v->mem,s,d,"agent"); } break;
        case FLUX_MEMCOPY: { len=fu16(v); data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; uint32_t a,b,c; memcpy(&a,data,4); memcpy(&b,data+4,4); memcpy(&c,data+8,4); h=flux_mem_get(&v->mem,"heap"); if(h&&c>0){uint8_t*t=malloc(c); if(t){flux_mem_read(h,b,t,c); flux_mem_write(h,a,t,c); free(t);}} } break;
        case FLUX_MEMSET: { len=fu16(v); data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; uint32_t a,n; uint8_t val; memcpy(&a,data,4); memcpy(&n,data+4,4); val=data[8]; h=flux_mem_get(&v->mem,"heap"); if(h&&a<h->size){size_t s=n<(h->size-a)?n:(h->size-a); memset(h->data+a,val,s);} } break;
        case FLUX_MEMCMP: { len=fu16(v); data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; uint32_t a,b,c; memcpy(&a,data,4); memcpy(&b,data+4,4); memcpy(&c,data+8,4); h=flux_mem_get(&v->mem,"heap"); GPR[0]=0; if(h&&a+c<=h->size&&b+c<=h->size) GPR[0]=memcmp(h->data+a,h->data+b,c); } break;

        case FLUX_CAST: rd=gr8(v); rs1=gr8(v); if(rs1==0) v->regs.fp[rd]=(float)GPR[rd]; else GPR[rd]=(int32_t)v->regs.fp[rd]; break;
        case FLUX_BOX: { len=fu16(v); data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; if(v->box_count>=64) ERR(12); FluxBox*b=&v->box_table[v->box_count++]; b->type_tag=data[0]; memcpy(&b->int_val,data+1,4); memcpy(&b->float_val,data+1,4); GPR[0]=v->box_count-1; } break;
        case FLUX_UNBOX: { len=fu16(v); data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; int id=data[0]; if(id<0||id>=v->box_count) ERR(6); FluxBox*b=&v->box_table[id]; if(b->type_tag==0) GPR[0]=b->int_val; else if(b->type_tag==1) v->regs.fp[0]=b->float_val; else GPR[0]=b->int_val?1:0; } break;
        case FLUX_CHECK_TYPE: rd=gr8(v); rs1=f8(v); GPR[0]=(rd<v->box_count&&v->box_table[rd].type_tag==rs1); break;
        case FLUX_CHECK_BOUNDS: rd=gr8(v); rs1=gr8(v); GPR[0]=(GPR[rd]>=0&&GPR[rd]<GPR[rs1]); break;

        case FLUX_FADD: rd=gr8(v); rs1=gr8(v); v->regs.fp[rd]+=v->regs.fp[rs1]; break;
        case FLUX_FSUB: rd=gr8(v); rs1=gr8(v); v->regs.fp[rd]-=v->regs.fp[rs1]; break;
        case FLUX_FMUL: rd=gr8(v); rs1=gr8(v); v->regs.fp[rd]*=v->regs.fp[rs1]; break;
        case FLUX_FDIV: rd=gr8(v); rs1=gr8(v); if(!v->regs.fp[rs1]) ERR(3); v->regs.fp[rd]/=v->regs.fp[rs1]; break;
        case FLUX_FMIN: rd=gr8(v); rs1=gr8(v); { float a=v->regs.fp[rd],b=v->regs.fp[rs1]; v->regs.fp[rd]=a<b?a:b; } break;
        case FLUX_FMAX: rd=gr8(v); rs1=gr8(v); { float a=v->regs.fp[rd],b=v->regs.fp[rs1]; v->regs.fp[rd]=a>b?a:b; } break;
        case FLUX_FNEG: rd=gr8(v); v->regs.fp[rd]=-v->regs.fp[rd]; break;
        case FLUX_FABS: rd=gr8(v); v->regs.fp[rd]=fabsf(v->regs.fp[rd]); break;
        case FLUX_FEQ: rd=gr8(v); rs1=gr8(v); GPR[rd]=(v->regs.fp[rd]==v->regs.fp[rs1]); break;
        case FLUX_FLT: rd=gr8(v); rs1=gr8(v); GPR[rd]=(v->regs.fp[rd]<v->regs.fp[rs1]); break;
        case FLUX_FLE: rd=gr8(v); rs1=gr8(v); GPR[rd]=(v->regs.fp[rd]<=v->regs.fp[rs1]); break;
        case FLUX_FGT: rd=gr8(v); rs1=gr8(v); GPR[rd]=(v->regs.fp[rd]>v->regs.fp[rs1]); break;
        case FLUX_FGE: rd=gr8(v); rs1=gr8(v); GPR[rd]=(v->regs.fp[rd]>=v->regs.fp[rs1]); break;
        case FLUX_LOAD8: { rd=gr8(v); rs1=gr8(v); h=flux_mem_get(&v->mem,"heap"); GPR[rd]=h?(int32_t)flux_mem_read_u8(h,GPR[rs1]):0; } break;
        case FLUX_STORE8: { rd=gr8(v); rs1=gr8(v); h=flux_mem_get(&v->mem,"heap"); if(h) flux_mem_write_u8(h,GPR[rd],(uint8_t)GPR[rs1]); } break;

        case FLUX_VLOAD: { len=fu16(v); data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; uint8_t r=data[0]; uint16_t off; memcpy(&off,data+1,2); h=flux_mem_get(&v->mem,"heap"); if(h) flux_mem_read(h,off,v->regs.vec[r],16); } break;
        case FLUX_VSTORE: { len=fu16(v); data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; uint8_t r=data[0]; uint16_t off; memcpy(&off,data+1,2); h=flux_mem_get(&v->mem,"heap"); if(h) flux_mem_write(h,off,v->regs.vec[r],16); } break;
        case FLUX_VADD: { rd=f8(v); rs1=f8(v); for(int i=0;i<4;i++){int32_t a,b; memcpy(&a,v->regs.vec[rd]+i*4,4); memcpy(&b,v->regs.vec[rs1]+i*4,4); a+=b; memcpy(v->regs.vec[rd]+i*4,&a,4);} } break;
        case FLUX_VSUB: { rd=f8(v); rs1=f8(v); for(int i=0;i<4;i++){int32_t a,b; memcpy(&a,v->regs.vec[rd]+i*4,4); memcpy(&b,v->regs.vec[rs1]+i*4,4); a-=b; memcpy(v->regs.vec[rd]+i*4,&a,4);} } break;
        case FLUX_VMUL: { rd=f8(v); rs1=f8(v); for(int i=0;i<4;i++){int32_t a,b; memcpy(&a,v->regs.vec[rd]+i*4,4); memcpy(&b,v->regs.vec[rs1]+i*4,4); a*=b; memcpy(v->regs.vec[rd]+i*4,&a,4);} } break;
        case FLUX_VDIV: { rd=f8(v); rs1=f8(v); for(int i=0;i<4;i++){int32_t a,b; memcpy(&a,v->regs.vec[rd]+i*4,4); memcpy(&b,v->regs.vec[rs1]+i*4,4); if(b){a/=b; memcpy(v->regs.vec[rd]+i*4,&a,4);}} } break;
        case FLUX_VFMA: { rd=f8(v); rs1=f8(v); rs2=f8(v); for(int i=0;i<4;i++){float a,b,c; memcpy(&a,v->regs.vec[rd]+i*4,4); memcpy(&b,v->regs.vec[rs1]+i*4,4); memcpy(&c,v->regs.vec[rs2]+i*4,4); float r=a+b*c; memcpy(v->regs.vec[rd]+i*4,&r,4);} } break;

        case FLUX_TELL:
        case FLUX_DELEGATE_RESULT:
        case FLUX_REPORT_STATUS:
        case FLUX_REQUEST_OVERRIDE:
        case FLUX_REDUCE:
        case FLUX_DECLARE_INTENT:
        case FLUX_ASSERT_GOAL:
        case FLUX_VERIFY_OUTCOME:
        case FLUX_EXPLAIN_FAILURE:
        case FLUX_SET_PRIORITY:
        case FLUX_TRUST_CHECK:
        case FLUX_TRUST_UPDATE:
        case FLUX_TRUST_QUERY:
        case FLUX_REVOKE_TRUST:
        case FLUX_CAP_REQUIRE:
        case FLUX_CAP_REQUEST:
        case FLUX_CAP_GRANT:
        case FLUX_CAP_REVOKE:
        case FLUX_BARRIER:
        case FLUX_SYNC_CLOCK:
        case FLUX_FORMATION_UPDATE:
        { len=fu16(v); data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; if(v->a2a_handler) v->a2a_handler(v,op,data,len); } break;

        case FLUX_EMERGENCY_STOP:
        case FLUX_HALT:
            v->halted=1; v->running=0; v->last_error=1; return (int64_t)v->cycle_count;

        case FLUX_YIELD: break;
        case FLUX_RESOURCE_ACQUIRE: { len=fu16(v); data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; if(len>0) v->resources[data[0]]=1; GPR[0]=1; } break;
        case FLUX_RESOURCE_RELEASE: { len=fu16(v); data=&v->bytecode[v->regs.pc]; v->regs.pc+=len; if(len>0) v->resources[data[0]]=0; } break;
        case FLUX_DEBUG_BREAK: break;

        default: ERR(2);
        }
    }
    v->running = 0;
    if (v->cycle_count >= v->max_cycles && !v->halted) v->last_error = 11;
    return (int64_t)v->cycle_count;
}

int flux_vm_step(FluxVM* v) {
    if (!v->running) v->running = 1;
    if (v->cycle_count >= v->max_cycles) return 11;
    uint32_t sm = v->max_cycles; v->max_cycles = v->cycle_count + 1;
    int64_t rc = flux_vm_execute(v);
    v->max_cycles = sm;
    return rc < 0 ? (int)(-rc) : 0;
}

void flux_vm_set_a2a(FluxVM* v, FluxA2AHandler h) { v->a2a_handler = h; }
