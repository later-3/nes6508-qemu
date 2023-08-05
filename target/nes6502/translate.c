/*
 * QEMU AVR CPU
 *
 * Copyright (c) 2019-2020 Michael Rolnik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/lgpl-2.1.html>
 */

#include "qemu/osdep.h"
#include "qemu/qemu-print.h"
#include "tcg/tcg.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "tcg/tcg-op.h"
#include "exec/cpu_ldst.h"
#include "exec/helper-proto.h"
#include "exec/helper-gen.h"
#include "exec/log.h"
#include "exec/translator.h"
#include "exec/gen-icount.h"
#include "exec/address-spaces.h"

/*
 *  Define if you want a BREAK instruction translated to a breakpoint
 *  Active debugging connection is assumed
 *  This is for
 *  https://github.com/seharris/qemu-avr-tests/tree/master/instruction-tests
 *  tests
 */
#undef BREAKPOINT_ON_BREAK

static TCGv cpu_pc;

static TCGv cpu_A;
static TCGv cpu_X;
static TCGv cpu_Y;

static TCGv cpu_Cf;
static TCGv cpu_Zf;
static TCGv cpu_Nf;
static TCGv cpu_Vf;
static TCGv cpu_Sf;
static TCGv cpu_Hf;
static TCGv cpu_Tf;
static TCGv cpu_If;

static TCGv cpu_carry_flag;
static TCGv cpu_zero_flag;
static TCGv cpu_interrupt_flag;
static TCGv cpu_decimal_flag;
static TCGv cpu_break_flag;
static TCGv cpu_overflow_flag;
static TCGv cpu_negative_flag;
static TCGv cpu_stack_point;
static TCGv op_address;
static TCGv op_value;

static TCGv cpu_rampD;
static TCGv cpu_rampX;
static TCGv cpu_rampY;
static TCGv cpu_rampZ;

static TCGv cpu_r[NUMBER_OF_CPU_REGISTERS];
static TCGv cpu_eind;
static TCGv cpu_sp;

static TCGv cpu_skip;

static const char reg_names[NUMBER_OF_CPU_REGISTERS][8] = {
    "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
};
#define REG(x) (cpu_r[x])

#define DISAS_JUMP    DISAS_TARGET_0
#define DISAS_UPDATE  DISAS_TARGET_1
#define DISAS_EXIT    DISAS_TARGET_2

typedef struct DisasContext DisasContext;

/* This is the state at translation time. */
struct DisasContext {
    DisasContextBase base;

    CPUNES6502State *env;
    CPUState *cs;

    target_long npc;
    uint32_t opcode;

    /* Routine used to access memory */
    int memidx;

    TCGv skip_var0;
    TCGv skip_var1;
    TCGCond skip_cond;
};

void avr_cpu_tcg_init(void)
{
    int i;

#define NES6502_REG_OFFS(x) offsetof(CPUNES6502State, x)
    cpu_pc = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(pc_w), "pc");
    cpu_Cf = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregC), "Cf");
    cpu_Zf = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregZ), "Zf");
    cpu_Nf = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregN), "Nf");
    cpu_Vf = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregV), "Vf");
    cpu_Sf = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregS), "Sf");
    cpu_Hf = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregH), "Hf");
    cpu_Tf = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregT), "Tf");
    cpu_If = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregI), "If");
    cpu_rampD = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(rampD), "rampD");
    cpu_rampX = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(rampX), "rampX");
    cpu_rampY = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(rampY), "rampY");
    cpu_rampZ = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(rampZ), "rampZ");
    cpu_eind = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(eind), "eind");
    cpu_sp = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sp), "sp");
    cpu_skip = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(skip), "skip");

    cpu_A = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(reg_A), "reg_A");
    cpu_X = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(reg_X), "reg_X");
    cpu_Y = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(reg_X), "reg_Y");
    cpu_carry_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(carry_flag), "carry_flag");
    cpu_zero_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(zero_flag), "zero_flag");
    cpu_interrupt_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(interrupt_flag), "interrupt_flag");
    cpu_decimal_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(decimal_flag), "decimal_flag");
    cpu_break_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(break_flag), "break_flag");
    cpu_overflow_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(overflow_flag), "overflow_flag");
    cpu_negative_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(negative_flag), "negative_flag");
    cpu_stack_point = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(stack_point), "stack_point");
    cpu_stack_point = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(stack_point), "op_address");
    cpu_stack_point = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(stack_point), "op_value");

    for (i = 0; i < NUMBER_OF_CPU_REGISTERS; i++) {
        cpu_r[i] = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(r[i]),
                                          reg_names[i]);
    }
#undef NES6502_REG_OFFS
}

/* decoder helper */
static uint32_t decode_insn_load_bytes(DisasContext *ctx, uint32_t insn,
                           int i, int n)
{
    while (++i <= n) {
        uint8_t b = cpu_ldub_code(ctx->env, ctx->npc++);
        insn |= b << (32 - i * 8);
    }
    return insn;
}

static uint32_t decode_insn_load(DisasContext *ctx);
static bool decode_insn(DisasContext *ctx, uint32_t insn);
#include "decode-insn.c.inc"

// CPU Addressing Modes
static void cpu_address_zero_page(uint8_t addr)
{
    tcg_gen_movi_tl(op_address, addr);
    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_zero_page_x(uint8_t imm)
{
    TCGv tmp = tcg_temp_new();
    tcg_gen_addi_tl(tmp, cpu_X, imm);
    tcg_gen_andi_tl(op_address, tmp, 0xFF);

    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_zero_page_y(uint8_t imm)
{
    TCGv tmp = tcg_temp_new();
    tcg_gen_addi_tl(tmp, cpu_Y, imm);
    tcg_gen_andi_tl(op_address, tmp, 0xFF);

    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_absolute(uint16_t addr)
{
    tcg_gen_movi_tl(op_address, addr);
    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_absolute_x(uint16_t addr)
{
    tcg_gen_addi_tl(op_address, cpu_X, addr);
    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_absolute_y(uint16_t addr)
{
    tcg_gen_addi_tl(op_address, cpu_Y, addr);
    tcg_gen_andi_tl(op_address, op_address, 0xFFFF);
    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_relative(uint8_t imm)
{
    int addr = imm;
    if (addr & 0x80)
        addr -= 0x100;
    tcg_gen_movi_tl(op_address, imm);
    tcg_gen_add_tl(op_address, op_address, cpu_pc);
}

static void cpu_address_indirect(uint8_t addr)
{
    if ((addr & 0xFF) == 0xFF) {
        // Buggy code
        addr &= 0xFF00;
        TCGv tmp = tcg_temp_new();
        tcg_gen_movi_tl(tmp, addr);
        tcg_gen_qemu_ld_tl(tmp, tmp, 0, MO_UB);
        tcg_gen_shli_tl(tmp, tmp, 8);
        tcg_gen_qemu_ld_tl(op_address, addr, 0, MO_UB);
        tcg_gen_add_tl(op_address, op_address, tmp);
    }
    else {
        // Normal code
        tcg_gen_movi_tl(op_address, addr);
        tcg_gen_qemu_ld_tl(op_address, op_address, 0, MO_UW);
    }
}

static void cpu_address_indirect_x(uint8_t imm)
{

    TCGv tmp = tcg_temp_new();
    tcg_gen_movi_tl(tmp, imm);

    tcg_gen_add_tl(tmp, tmp, cpu_X);
    TCGv addr2 = tcg_temp_new();

    tcg_gen_andi_tl(addr2, tmp, 0xFF);
    tcg_gen_qemu_ld_tl(addr2, tmp, 0, MO_UB);


    tcg_gen_addi_tl(tmp, tmp, 1);
    TCGv addr1 = tcg_temp_new();

    tcg_gen_andi_tl(addr1, tmp, 0xFF);
    tcg_gen_qemu_ld_tl(addr1, tmp, 0, MO_UB);
    tcg_gen_shli_tl(addr1, addr1, 8);

    tcg_gen_or_tl(op_address, addr1, addr2);

    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}


static void cpu_address_indirect_y(uint8_t imm)
{

    TCGv tmp = tcg_temp_new();
    tcg_gen_movi_tl(tmp, imm);

    TCGv addr2 = tcg_temp_new();
    tcg_gen_qemu_ld_tl(addr2, tmp, 0, MO_UB);

    tcg_gen_addi_tl(tmp, tmp, 1);
    TCGv addr1 = tcg_temp_new();
    tcg_gen_andi_tl(addr1, tmp, 0xFF);
    tcg_gen_qemu_ld_tl(addr1, tmp, 0, MO_UB);
    tcg_gen_shli_tl(addr1, addr1, 8);

    tcg_gen_or_tl(op_address, addr1, addr2);
    tcg_gen_add_tl(op_address, op_address, cpu_Y);
    tcg_gen_andi_tl(op_address, op_address, 0xFFFF);

    tcg_gen_qemu_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_flag_set(TCGv flag, TCGv res, int value)
{
    tcg_gen_setcondi_tl(TCG_COND_EQ, res, flag, value);
}

static void cpu_modify_flag(TCGv flag, TCGv res)
{
    tcg_gen_setcondi_tl(TCG_COND_EQ, flag, res, 1);
}

static void cpu_update_zn_flags(TCGv r)
{
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_zero_flag, r, 0);
    tcg_gen_setcondi_tl(TCG_COND_GE, cpu_negative_flag, r, 127);
}

/*
 * Arithmetic Instructions
 */

static void trans_ADC_common(void)
{
    TCGv res = tcg_temp_new();
    cpu_flag_set(cpu_carry_flag, res, 1);

    TCGv r = tcg_temp_new();
    tcg_gen_add_tl(r, cpu_A, op_value);
    tcg_gen_add_tl(r, r, res);

    TCGv t = tcg_temp_new();
    tcg_gen_andi_tl(t, r, 0x100);
    tcg_gen_not_tl(t, t);
    tcg_gen_not_tl(t, t);
    cpu_modify_flag(cpu_carry_flag, t);
    
    TCGv t1 = tcg_temp_new();
    TCGv t2 = tcg_temp_new();

    tcg_gen_xor_tl(t1, cpu_A, op_value);
    tcg_gen_xor_tl(t2, cpu_A, r);

    TCGv t3 = tcg_temp_new();
    tcg_gen_andc_tl(t3, t2, t1);
    tcg_gen_andi_tl(t3, t3, 0x80);
    tcg_gen_not_tl(t3, t3);
    tcg_gen_not_tl(t3, t3);
    
    cpu_modify_flag(cpu_overflow_flag, t3);

    tcg_gen_andi_tl(cpu_A, r, 0xFF);
    cpu_update_zn_flags(cpu_A);
}

static bool trans_ADC_IM(DisasContext *ctx, arg_ADC_IM *a)
{
    tcg_gen_movi_tl(op_value, a->imm);
    trans_ADC_common();
    return true;
}

static bool trans_ADC_ZEROPAGE(DisasContext *ctx, arg_ADC_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    trans_ADC_common();
    return true;
}

static bool trans_ADC_ZEROPAGE_X(DisasContext *ctx, arg_ADC_ZEROPAGE_X *a)
{
    cpu_address_zero_page(a->imm);
    trans_ADC_common();
    return true;
}

static bool trans_ADC_ABSOLUTE(DisasContext *ctx, arg_ADC_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr1 << 8 | a->addr2);
    trans_ADC_common();
    return true;
}

static bool trans_ADC_ABSOLUTE_X(DisasContext *ctx, arg_ADC_ABSOLUTE_X *a)
{
    cpu_address_absolute_x(a->addr1 << 8 | a->addr2);
    trans_ADC_common();
    return true;
}

static bool trans_ADC_ABSOLUTE_Y(DisasContext *ctx, arg_ADC_ABSOLUTE_Y *a)
{
    cpu_address_absolute_y(a->addr1 << 8 | a->addr2);
    trans_ADC_common();
    return true;
}

static bool trans_ADC_INDIRECT_X(DisasContext *ctx, arg_ADC_INDIRECT_X *a)
{
    cpu_address_indirect_x(a->imm);
    trans_ADC_common();
    return true;
}

static bool trans_ADC_INDIRECT_Y(DisasContext *ctx, arg_ADC_INDIRECT_Y *a)
{
    cpu_address_indirect_y(a->imm);
    trans_ADC_common();
    return true;
}

static void trans_SBC_common(void)
{
    TCGv res = tcg_temp_new();
    cpu_flag_set(cpu_carry_flag, res, 0);

    TCGv r = tcg_temp_new();
    tcg_gen_sub_tl(r, cpu_A, op_value);
    tcg_gen_sub_tl(r, r, res);

    TCGv t = tcg_temp_new();
    tcg_gen_andi_tl(t, r, 0x100);
    tcg_gen_not_tl(t, t);
    cpu_modify_flag(cpu_carry_flag, t);
    
    TCGv t1 = tcg_temp_new();
    TCGv t2 = tcg_temp_new();

    tcg_gen_xor_tl(t1, cpu_A, op_value);
    tcg_gen_xor_tl(t2, cpu_A, r);

    TCGv t3 = tcg_temp_new();
    tcg_gen_and_tl(t3, t2, t1);
    tcg_gen_andi_tl(t3, t3, 0x80);
    tcg_gen_not_tl(t3, t3);
    tcg_gen_not_tl(t3, t3);
    
    cpu_modify_flag(cpu_overflow_flag, t3);

    tcg_gen_andi_tl(cpu_A, r, 0xFF);
    cpu_update_zn_flags(cpu_A);
}


static bool trans_SBC_IM(DisasContext *ctx, arg_SBC_IM *a)
{
    tcg_gen_movi_tl(op_value, a->imm);
    trans_SBC_common();
    return true;
}

static bool trans_SBC_ZEROPAGE(DisasContext *ctx, arg_SBC_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    trans_SBC_common();
    return true;
}

static bool trans_SBC_ZEROPAGE_X(DisasContext *ctx, arg_SBC_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    trans_SBC_common();
    return true;
}

static bool trans_SBC_ABSOLUTE(DisasContext *ctx, arg_SBC_ABSOLUTE *a)
{
    cpu_address_absolute( (a->addr1 << 8) | a->addr2);
    trans_SBC_common();
    return true;
}

static bool trans_SBC_ABSOLUTE_X(DisasContext *ctx, arg_SBC_ABSOLUTE_X *a)
{
    cpu_address_absolute_x( (a->addr1 << 8) | a->addr2);
    trans_SBC_common();
    return true;
}

static bool trans_SBC_ABSOLUTE_Y(DisasContext *ctx, arg_SBC_ABSOLUTE_Y *a)
{
    cpu_address_absolute_x( (a->addr1 << 8) | a->addr2);
    trans_SBC_common();
    return true;
}

static bool trans_SBC_INDIRECT_X(DisasContext *ctx, arg_SBC_INDIRECT_X *a)
{
    cpu_address_indirect_x(a->imm);
    trans_SBC_common();
    return true;
}

static bool trans_SBC_INDIRECT_Y(DisasContext *ctx, arg_SBC_INDIRECT_Y *a)
{
    cpu_address_indirect_y(a->imm);
    trans_SBC_common();
    return true;
}


/*
 * Branch Instructions
 */


static void cpu_branch(DisasContext *ctx, TCGCond cond, TCGv value, int arg, uint8_t addr)
{
    TCGLabel *t, *done;

    t = gen_new_label();
    done = gen_new_label();
    tcg_gen_brcondi_i32(cond, value, arg, t);
    gen_goto_tb(ctx, 0, ctx->base.pc_next);
    tcg_gen_br(done);
    gen_set_label(t);
    gen_goto_tb(ctx, 1, ctx->npc + addr);
    gen_set_label(done);
}

static bool trans_BCC(DisasContext *ctx, arg_BCC *a)
{
    cpu_address_relative(a->imm);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_carry_flag, tmp, 0);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, a->imm);
    return true;
}

static bool trans_BCS(DisasContext *ctx, arg_BCS *a)
{
    cpu_address_relative(a->imm);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_carry_flag, tmp, 1);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, a->imm);
    return true;
}

static bool trans_BEQ(DisasContext *ctx, arg_BEQ *a)
{
    cpu_address_relative(a->imm);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_zero_flag, tmp, 1);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, a->imm); 
    return true;
}

static bool trans_BMI(DisasContext *ctx, arg_BMI *a)
{
    cpu_address_relative(a->imm);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_negative_flag, tmp, 1);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, a->imm);
    return true;
}

static bool trans_BVS(DisasContext *ctx, arg_BVS *a)
{
    cpu_address_relative(a->imm);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_overflow_flag, tmp, 1);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, a->imm);
    return true;
}

static bool trans_BNE(DisasContext *ctx, arg_BNE *a)
{
    cpu_address_relative(a->imm);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_zero_flag, tmp, 0);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, a->imm);
    return true;
}

static bool trans_BPL(DisasContext *ctx, arg_BPL *a)
{
    cpu_address_relative(a->imm);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_negative_flag, tmp, 0);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, a->imm);
    return true;
}

static bool trans_BVC(DisasContext *ctx, arg_BVC *a)
{
    cpu_address_relative(a->imm);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_overflow_flag, tmp, 0);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, a->imm);
    return true;
}

static bool trans_JMP_ABSOLUTE(DisasContext *ctx, arg_JMP_ABSOLUTE *a)
{
    cpu_address_absolute( (a->addr1 << 8) | a->addr2);

    tcg_gen_mov_i32(cpu_pc, op_address);
    ctx->base.is_jmp = DISAS_JUMP;
    return true;
}

static bool trans_JMP_INDIRECT(DisasContext *ctx, arg_JMP_INDIRECT *a)
{
    return true;
}

static bool trans_JSR_ABSOLUTE(DisasContext *ctx, arg_JSR_ABSOLUTE *a)
{
    return true;
}

static bool trans_RTS(DisasContext *ctx, arg_RTS *a)
{
    return true;
}

static bool trans_RTI(DisasContext *ctx, arg_RTI *a)
{
    return true;
}

static void gen_goto_tb(DisasContext *ctx, int n, target_ulong dest)
{
    const TranslationBlock *tb = ctx->base.tb;

    if (translator_use_goto_tb(&ctx->base, dest)) {
        tcg_gen_goto_tb(n);
        tcg_gen_movi_i32(cpu_pc, dest);
        tcg_gen_exit_tb(tb, n);
    } else {
        tcg_gen_movi_i32(cpu_pc, dest);
        tcg_gen_lookup_and_goto_ptr();
    }
    ctx->base.is_jmp = DISAS_NORETURN;
}

/*
 * Data Transfer Instructions
 */

static bool trans_LDA_IM(DisasContext *ctx, arg_LDA_IM *a)
{
    tcg_gen_movi_i32(cpu_A, a->imm);
    return true;
}

static bool trans_LDA_ZEROPAGE(DisasContext *ctx, arg_LDA_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_LDA_ZEROPAGE_X(DisasContext *ctx, arg_LDA_ZEROPAGE_X *a)
{
    return true;
}

static bool trans_LDA_ABSOLUTE(DisasContext *ctx, arg_LDA_ABSOLUTE *a)
{
    return true;
}

static bool trans_LDA_ABSOLUTE_X(DisasContext *ctx, arg_LDA_ABSOLUTE_X *a)
{
    return true;
}

static bool trans_LDA_ABSOLUTE_Y(DisasContext *ctx, arg_LDA_ABSOLUTE_Y *a)
{
    return true;
}

static bool trans_LDA_INDIRECT_X(DisasContext *ctx, arg_LDA_INDIRECT_X *a)
{
    return true;
}

static bool trans_LDA_INDIRECT_Y(DisasContext *ctx, arg_LDA_INDIRECT_Y *a)
{
    return true;
}


static bool trans_LDX_IM(DisasContext *ctx, arg_LDX_IM *a)
{
    tcg_gen_movi_i32(cpu_A, a->imm);
    return true;
}

static bool trans_LDX_ZEROPAGE(DisasContext *ctx, arg_LDX_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_LDX_ZEROPAGE_Y(DisasContext *ctx, arg_LDX_ZEROPAGE_Y *a)
{
    return true;
}

static bool trans_LDX_ABSOLUTE(DisasContext *ctx, arg_LDX_ABSOLUTE *a)
{
    return true;
}

static bool trans_LDX_ABSOLUTE_Y(DisasContext *ctx, arg_LDX_ABSOLUTE_Y *a)
{
    return true;
}

static bool trans_LDY_IM(DisasContext *ctx, arg_LDY_IM *a)
{
    tcg_gen_movi_i32(cpu_A, a->imm);
    return true;
}

static bool trans_LDY_ZEROPAGE(DisasContext *ctx, arg_LDY_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_LDY_ZEROPAGE_X(DisasContext *ctx, arg_LDY_ZEROPAGE_X *a)
{
    return true;
}

static bool trans_LDY_ABSOLUTE(DisasContext *ctx, arg_LDY_ABSOLUTE *a)
{
    return true;
}

static bool trans_LDY_ABSOLUTE_X(DisasContext *ctx, arg_LDY_ABSOLUTE_X *a)
{
    return true;
}

static bool trans_STA_ZEROPAGE(DisasContext *ctx, arg_STA_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_STA_ZEROPAGE_X(DisasContext *ctx, arg_STA_ZEROPAGE_X *a)
{
    return true;
}

static bool trans_STA_ABSOLUTE(DisasContext *ctx, arg_STA_ABSOLUTE *a)
{
    return true;
}

static bool trans_STA_ABSOLUTE_X(DisasContext *ctx, arg_STA_ABSOLUTE_X *a)
{
    return true;
}

static bool trans_STA_ABSOLUTE_Y(DisasContext *ctx, arg_STA_ABSOLUTE_Y *a)
{
    return true;
}

static bool trans_STA_INDIRECT_X(DisasContext *ctx, arg_STA_INDIRECT_X *a)
{
    return true;
}

static bool trans_STA_INDIRECT_Y(DisasContext *ctx, arg_STA_INDIRECT_Y *a)
{
    return true;
}

static bool trans_STX_ZEROPAGE(DisasContext *ctx, arg_STX_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_STX_ZEROPAGE_Y(DisasContext *ctx, arg_STX_ZEROPAGE_Y *a)
{
    return true;
}

static bool trans_STX_ABSOLUTE(DisasContext *ctx, arg_STX_ABSOLUTE *a)
{
    return true;
}

static bool trans_STY_ZEROPAGE(DisasContext *ctx, arg_STY_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_STY_ZEROPAGE_X(DisasContext *ctx, arg_STY_ZEROPAGE_X *a)
{
    return true;
}

static bool trans_STY_ABSOLUTE(DisasContext *ctx, arg_STY_ABSOLUTE *a)
{
    return true;
}

static bool trans_TAX(DisasContext *ctx, arg_TAX *a)
{
    return true;
}

static bool trans_TAY(DisasContext *ctx, arg_TAY *a)
{
    return true;
}

static bool trans_TSX(DisasContext *ctx, arg_TSX *a)
{
    return true;
}

static bool trans_TXS(DisasContext *ctx, arg_TXS *a)
{
    return true;
}

static bool trans_TXA(DisasContext *ctx, arg_TXA *a)
{
    return true;
}

static bool trans_TYA(DisasContext *ctx, arg_TYA *a)
{
    return true;
}

/*
 * Bit and Bit-test Instructions
 */

static bool trans_AND_IM(DisasContext *ctx, arg_AND_IM *a)
{
    tcg_gen_movi_i32(cpu_A, a->imm);
    return true;
}

static bool trans_AND_ZEROPAGE(DisasContext *ctx, arg_AND_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_AND_ZEROPAGE_X(DisasContext *ctx, arg_AND_ZEROPAGE_X *a)
{
    return true;
}

static bool trans_AND_ABSOLUTE(DisasContext *ctx, arg_AND_ABSOLUTE *a)
{
    return true;
}

static bool trans_AND_ABSOLUTE_X(DisasContext *ctx, arg_AND_ABSOLUTE_X *a)
{
    return true;
}

static bool trans_AND_ABSOLUTE_Y(DisasContext *ctx, arg_AND_ABSOLUTE_Y *a)
{
    return true;
}

static bool trans_AND_INDIRECT_X(DisasContext *ctx, arg_AND_INDIRECT_X *a)
{
    return true;
}

static bool trans_AND_INDIRECT_Y(DisasContext *ctx, arg_AND_INDIRECT_Y *a)
{
    return true;
}


static bool trans_EOR_IM(DisasContext *ctx, arg_EOR_IM *a)
{
    tcg_gen_movi_i32(cpu_A, a->imm);
    return true;
}

static bool trans_EOR_ZEROPAGE(DisasContext *ctx, arg_EOR_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_EOR_ZEROPAGE_X(DisasContext *ctx, arg_EOR_ZEROPAGE_X *a)
{
    return true;
}

static bool trans_EOR_ABSOLUTE(DisasContext *ctx, arg_EOR_ABSOLUTE *a)
{
    return true;
}

static bool trans_EOR_ABSOLUTE_X(DisasContext *ctx, arg_EOR_ABSOLUTE_X *a)
{
    return true;
}

static bool trans_EOR_ABSOLUTE_Y(DisasContext *ctx, arg_EOR_ABSOLUTE_Y *a)
{
    return true;
}

static bool trans_EOR_INDIRECT_X(DisasContext *ctx, arg_EOR_INDIRECT_X *a)
{
    return true;
}

static bool trans_EOR_INDIRECT_Y(DisasContext *ctx, arg_EOR_INDIRECT_Y *a)
{
    return true;
}


static bool trans_ORA_IM(DisasContext *ctx, arg_ORA_IM *a)
{
    tcg_gen_movi_i32(cpu_A, a->imm);
    return true;
}

static bool trans_ORA_ZEROPAGE(DisasContext *ctx, arg_ORA_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_ORA_ZEROPAGE_X(DisasContext *ctx, arg_ORA_ZEROPAGE_X *a)
{
    return true;
}

static bool trans_ORA_ABSOLUTE(DisasContext *ctx, arg_ORA_ABSOLUTE *a)
{
    return true;
}

static bool trans_ORA_ABSOLUTE_X(DisasContext *ctx, arg_ORA_ABSOLUTE_X *a)
{
    return true;
}

static bool trans_ORA_ABSOLUTE_Y(DisasContext *ctx, arg_ORA_ABSOLUTE_Y *a)
{
    return true;
}

static bool trans_ORA_INDIRECT_X(DisasContext *ctx, arg_ORA_INDIRECT_X *a)
{
    return true;
}

static bool trans_ORA_INDIRECT_Y(DisasContext *ctx, arg_ORA_INDIRECT_Y *a)
{
    return true;
}

static bool trans_ASL_A(DisasContext *ctx, arg_ASL_A *a)
{
    return true;
}

static bool trans_ASL_ZEROPAGE(DisasContext *ctx, arg_ASL_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_ASL_ZEROPAGE_X(DisasContext *ctx, arg_ASL_ZEROPAGE_X *a)
{
    return true;
}

static bool trans_ASL_ABSOLUTE(DisasContext *ctx, arg_ASL_ABSOLUTE *a)
{
    return true;
}

static bool trans_ASL_ABSOLUTE_X(DisasContext *ctx, arg_ASL_ABSOLUTE_X *a)
{
    return true;
}


static bool trans_LSR_A(DisasContext *ctx, arg_LSR_A *a)
{
    return true;
}

static bool trans_LSR_ZEROPAGE(DisasContext *ctx, arg_LSR_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_LSR_ZEROPAGE_X(DisasContext *ctx, arg_LSR_ZEROPAGE_X *a)
{
    return true;
}

static bool trans_LSR_ABSOLUTE(DisasContext *ctx, arg_LSR_ABSOLUTE *a)
{
    return true;
}

static bool trans_LSR_ABSOLUTE_X(DisasContext *ctx, arg_LSR_ABSOLUTE_X *a)
{
    return true;
}


static bool trans_ROL_A(DisasContext *ctx, arg_ROL_A *a)
{
    return true;
}

static bool trans_ROL_ZEROPAGE(DisasContext *ctx, arg_ROL_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_ROL_ZEROPAGE_X(DisasContext *ctx, arg_ROL_ZEROPAGE_X *a)
{
    return true;
}

static bool trans_ROL_ABSOLUTE(DisasContext *ctx, arg_ROL_ABSOLUTE *a)
{
    return true;
}

static bool trans_ROL_ABSOLUTE_X(DisasContext *ctx, arg_ROL_ABSOLUTE_X *a)
{
    return true;
}


static bool trans_ROR_A(DisasContext *ctx, arg_ROR_A *a)
{
    return true;
}

static bool trans_ROR_ZEROPAGE(DisasContext *ctx, arg_ROR_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_ROR_ZEROPAGE_X(DisasContext *ctx, arg_ROR_ZEROPAGE_X *a)
{
    return true;
}

static bool trans_ROR_ABSOLUTE(DisasContext *ctx, arg_ROR_ABSOLUTE *a)
{
    return true;
}

static bool trans_ROR_ABSOLUTE_X(DisasContext *ctx, arg_ROR_ABSOLUTE_X *a)
{
    return true;
}




static bool trans_BIT_ZEROPAGE(DisasContext *ctx, arg_BIT_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}


static bool trans_BIT_ABSOLUTE(DisasContext *ctx, arg_BIT_ABSOLUTE *a)
{
    return true;
}


static bool trans_CLC(DisasContext *ctx, arg_CLC *a)
{
    return true;
}

static bool trans_CLD(DisasContext *ctx, arg_CLD *a)
{
    return true;
}

static bool trans_CLI(DisasContext *ctx, arg_CLI *a)
{
    return true;
}

static bool trans_CLV(DisasContext *ctx, arg_CLV *a)
{
    return true;
}

static bool trans_SEC(DisasContext *ctx, arg_SEC *a)
{
    return true;
}

static bool trans_SED(DisasContext *ctx, arg_SED *a)
{
    return true;
}

static bool trans_SEI(DisasContext *ctx, arg_SEI *a)
{
    return true;
}

/*
 * Comparison
 */



static bool trans_CMP_IM(DisasContext *ctx, arg_CMP_IM *a)
{
    tcg_gen_movi_i32(cpu_A, a->imm);
    return true;
}

static bool trans_CMP_ZEROPAGE(DisasContext *ctx, arg_CMP_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_CMP_ZEROPAGE_X(DisasContext *ctx, arg_CMP_ZEROPAGE_X *a)
{
    return true;
}

static bool trans_CMP_ABSOLUTE(DisasContext *ctx, arg_CMP_ABSOLUTE *a)
{
    return true;
}

static bool trans_CMP_ABSOLUTE_X(DisasContext *ctx, arg_CMP_ABSOLUTE_X *a)
{
    return true;
}

static bool trans_CMP_ABSOLUTE_Y(DisasContext *ctx, arg_CMP_ABSOLUTE_Y *a)
{
    return true;
}

static bool trans_CMP_INDIRECT_X(DisasContext *ctx, arg_CMP_INDIRECT_X *a)
{
    return true;
}

static bool trans_CMP_INDIRECT_Y(DisasContext *ctx, arg_CMP_INDIRECT_Y *a)
{
    return true;
}


static bool trans_CPX_IM(DisasContext *ctx, arg_CPX_IM *a)
{
    tcg_gen_movi_i32(cpu_A, a->imm);
    return true;
}

static bool trans_CPX_ZEROPAGE(DisasContext *ctx, arg_CPX_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_CPX_ABSOLUTE(DisasContext *ctx, arg_CPX_ABSOLUTE *a)
{
    return true;
}



static bool trans_CPY_IM(DisasContext *ctx, arg_CPY_IM *a)
{
    tcg_gen_movi_i32(cpu_A, a->imm);
    return true;
}

static bool trans_CPY_ZEROPAGE(DisasContext *ctx, arg_CPY_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_CPY_ABSOLUTE(DisasContext *ctx, arg_CPY_ABSOLUTE *a)
{
    return true;
}


/*
 * Increment
 */

static bool trans_INC_ZEROPAGE(DisasContext *ctx, arg_INC_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_INC_ZEROPAGE_X(DisasContext *ctx, arg_INC_ZEROPAGE_X *a)
{
    return true;
}

static bool trans_INC_ABSOLUTE(DisasContext *ctx, arg_INC_ABSOLUTE *a)
{
    return true;
}

static bool trans_INC_ABSOLUTE_X(DisasContext *ctx, arg_INC_ABSOLUTE_X *a)
{
    return true;
}


static bool trans_INX(DisasContext *ctx, arg_INX *a)
{
    return true;
}

static bool trans_INY(DisasContext *ctx, arg_INY *a)
{
    return true;
}



/*
 * Decrement
 */

static bool trans_DEC_ZEROPAGE(DisasContext *ctx, arg_DEC_ZEROPAGE *a)
{
    tcg_gen_movi_i32(cpu_X, a->imm);
    return true;
}

static bool trans_DEC_ZEROPAGE_X(DisasContext *ctx, arg_DEC_ZEROPAGE_X *a)
{
    return true;
}

static bool trans_DEC_ABSOLUTE(DisasContext *ctx, arg_DEC_ABSOLUTE *a)
{
    return true;
}

static bool trans_DEC_ABSOLUTE_X(DisasContext *ctx, arg_DEC_ABSOLUTE_X *a)
{
    return true;
}

static bool trans_DEX(DisasContext *ctx, arg_DEX *a)
{
    return true;
}

static bool trans_DEY(DisasContext *ctx, arg_DEY *a)
{
    return true;
}

/*
 * Stack
 */

static bool trans_PHP(DisasContext *ctx, arg_PHP *a)
{
    return true;
}

static bool trans_PHA(DisasContext *ctx, arg_PHA *a)
{
    return true;
}

static bool trans_PLA(DisasContext *ctx, arg_PLA *a)
{
    return true;
}

static bool trans_PLP(DisasContext *ctx, arg_PLP *a)
{
    return true;
}

/*
 * MCU Control Instructions
 */

static bool trans_NOP(DisasContext *ctx, arg_NOP *a)
{
    return true;
}

static bool trans_BRK(DisasContext *ctx, arg_BRK *a)
{
    return true;
}


/*
 *  Core translation mechanism functions:
 *
 *    - translate()
 *    - canonicalize_skip()
 *    - gen_intermediate_code()
 *    - restore_state_to_opc()
 *
 */
static void translate(DisasContext *ctx)
{
    // uint32_t opcode = next_word(ctx);
    uint32_t opcode;
    opcode = decode_insn_load(ctx);
    if (!decode_insn(ctx, opcode)) {
        gen_helper_unsupported(cpu_env);
        ctx->base.is_jmp = DISAS_NORETURN;
    }
}

/* Standardize the cpu_skip condition to NE.  */
static bool canonicalize_skip(DisasContext *ctx)
{
    switch (ctx->skip_cond) {
    case TCG_COND_NEVER:
        /* Normal case: cpu_skip is known to be false.  */
        return false;

    case TCG_COND_ALWAYS:
        /*
         * Breakpoint case: cpu_skip is known to be true, via TB_FLAGS_SKIP.
         * The breakpoint is on the instruction being skipped, at the start
         * of the TranslationBlock.  No need to update.
         */
        return false;

    case TCG_COND_NE:
        if (ctx->skip_var1 == NULL) {
            tcg_gen_mov_tl(cpu_skip, ctx->skip_var0);
        } else {
            tcg_gen_xor_tl(cpu_skip, ctx->skip_var0, ctx->skip_var1);
            ctx->skip_var1 = NULL;
        }
        break;

    default:
        /* Convert to a NE condition vs 0. */
        if (ctx->skip_var1 == NULL) {
            tcg_gen_setcondi_tl(ctx->skip_cond, cpu_skip, ctx->skip_var0, 0);
        } else {
            tcg_gen_setcond_tl(ctx->skip_cond, cpu_skip,
                               ctx->skip_var0, ctx->skip_var1);
            ctx->skip_var1 = NULL;
        }
        ctx->skip_cond = TCG_COND_NE;
        break;
    }
    ctx->skip_var0 = cpu_skip;
    return true;
}

static void nes6502_tr_init_disas_context(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);
    CPUNES6502State *env = cs->env_ptr;
    uint32_t tb_flags = ctx->base.tb->flags;

    ctx->cs = cs;
    ctx->env = env;
    ctx->npc = ctx->base.pc_first;

    ctx->skip_cond = TCG_COND_NEVER;
    if (tb_flags & TB_FLAGS_SKIP) {
        ctx->skip_cond = TCG_COND_ALWAYS;
        ctx->skip_var0 = cpu_skip;
    }

    if (tb_flags & TB_FLAGS_FULL_ACCESS) {
        /*
         * This flag is set by ST/LD instruction we will regenerate it ONLY
         * with mem/cpu memory access instead of mem access
         */
        ctx->base.max_insns = 1;
    }
}

static void nes6502_tr_tb_start(DisasContextBase *db, CPUState *cs)
{
}

static void nes6502_tr_insn_start(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    tcg_gen_insn_start(ctx->npc);
}

static void nes6502_tr_translate_insn(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    translate(ctx);

    ctx->base.pc_next = ctx->npc;

}

static void nes6502_tr_tb_stop(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);
    /*
     * Because we disable interrupts while env->skip is set,
     * we must return to the main loop to re-evaluate afterward.
     */
    bool force_exit = ctx->base.tb->flags & TB_FLAGS_SKIP;

    switch (ctx->base.is_jmp) {
    case DISAS_NORETURN:
        break;
    case DISAS_NEXT:
    case DISAS_TOO_MANY:
        gen_goto_tb(ctx, 0, dcbase->pc_next);
        break;
    case DISAS_JUMP:
        tcg_gen_lookup_and_goto_ptr();
        break;
    case DISAS_EXIT:
        tcg_gen_exit_tb(NULL, 0);
        break;
    default:
        g_assert_not_reached();
    }
}

static void nes6502_tr_disas_log(const DisasContextBase *dcbase,
                             CPUState *cs, FILE *logfile)
{
    fprintf(logfile, "IN: %s\n", lookup_symbol(dcbase->pc_first));
    target_disas(logfile, cs, dcbase->pc_first, dcbase->tb->size);
}

static const TranslatorOps avr_tr_ops = {
    .init_disas_context = nes6502_tr_init_disas_context,
    .tb_start           = nes6502_tr_tb_start,
    .insn_start         = nes6502_tr_insn_start,
    .translate_insn     = nes6502_tr_translate_insn,
    .tb_stop            = nes6502_tr_tb_stop,
    .disas_log          = nes6502_tr_disas_log,
};

void gen_intermediate_code(CPUState *cs, TranslationBlock *tb, int *max_insns,
                           target_ulong pc, void *host_pc)
{
    DisasContext dc = { };
    translator_loop(cs, tb, max_insns, pc, host_pc, &avr_tr_ops, &dc.base);
}
