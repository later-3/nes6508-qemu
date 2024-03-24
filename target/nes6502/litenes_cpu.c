#include "litenes_cpu.h"
unsigned long long cpu_cycles;  // Total CPU Cycles Since Power Up (wraps)
//void (*cpu_op_address_mode[256])();       // Array of address modes
const char *cpu_op_name[256];                   // Instruction names
const char *cpu_op_address_mode[256];                   // Instruction names
int cpu_op_cycles[256];                   // CPU cycles used by instructions

// void cpu_address_implied()
// {
// }

// void cpu_address_immediate()
// {

// }

// void cpu_address_zero_page()
// {

// }

// void cpu_address_zero_page_x()
// {

// }

// void cpu_address_zero_page_y()
// {

// }

// void cpu_address_absolute()
// {

// }

// void cpu_address_absolute_x()
// {

// }

// void cpu_address_absolute_y()
// {

// }

// void cpu_address_relative()
// {

// }

// void cpu_address_indirect()
// {

// }

// void cpu_address_indirect_x()
// {
// }

// void cpu_address_indirect_y()
// {

// }


#define cpu_flag_set(flag) common_bit_set(cpu.P, flag)
#define cpu_modify_flag(flag, value) common_modify_bitb(&cpu.P, flag, value)
#define cpu_set_flag(flag) common_set_bitb(&cpu.P, flag)
#define cpu_unset_flag(flag) common_unset_bitb(&cpu.P, flag)

#define cpu_update_zn_flags(value) cpu.P = (cpu.P & ~(zero_flag | negative_flag)) | cpu_zn_flag_table[value]

#define cpu_branch(flag) if (flag) cpu.PC = op_address;
#define cpu_compare(reg) int result = reg - op_value; \
                         cpu_modify_flag(carry_bp, result >= 0); \
                         cpu_modify_flag(zero_bp, result == 0); \
                         cpu_modify_flag(negative_bp, (result >> 7) & 1);

// Base 6502 instruction set

#define CPU_OP_BIS(o, c, f, n, a) cpu_op_name[0x##o] = n; \
                                  cpu_op_address_mode[0x##o] = a; 

// Not implemented instructions

#define CPU_OP_NII(o, a) cpu_op_name[0x##o] = "NOP"; \
                         cpu_op_address_mode[0x##o] = a; 

// Extended instruction set found in other CPUs and implemented for compatibility

#define CPU_OP_EIS(o, c, f, n, a) cpu_op_name[0x##o] = n; \
                                  cpu_op_address_mode[0x##o] = a; 



// CPU Lifecycle

static void __attribute((constructor)) litenes_cpu_init(void)
{
    CPU_OP_BIS(00, 7, brk, "BRK", "implied")
    CPU_OP_BIS(01, 6, ora, "ORA", "indirect_x")
    CPU_OP_BIS(05, 3, ora, "ORA", "zero_page")
    CPU_OP_BIS(06, 5, asl, "ASL", "zero_page")
    CPU_OP_BIS(08, 3, php, "PHP", "implied")
    CPU_OP_BIS(09, 2, ora, "ORA", "immediate")
    CPU_OP_BIS(0A, 2, asla,"ASL", "implied")
    CPU_OP_BIS(0D, 4, ora, "ORA", "absolute")
    CPU_OP_BIS(0E, 6, asl, "ASL", "absolute")
    CPU_OP_BIS(10, 2, bpl, "BPL", "relative")
    CPU_OP_BIS(11, 5, ora, "ORA", "indirect_y")
    CPU_OP_BIS(15, 4, ora, "ORA", "zero_page_x")
    CPU_OP_BIS(16, 6, asl, "ASL", "zero_page_x")
    CPU_OP_BIS(18, 2, clc, "CLC", "implied")
    CPU_OP_BIS(19, 4, ora, "ORA", "absolute_y")
    CPU_OP_BIS(1D, 4, ora, "ORA", "absolute_x")
    CPU_OP_BIS(1E, 7, asl, "ASL", "absolute_x")
    CPU_OP_BIS(20, 6, jsr, "JSR", "absolute")
    CPU_OP_BIS(21, 6, and, "AND", "indirect_x")
    CPU_OP_BIS(24, 3, bit, "BIT", "zero_page")
    CPU_OP_BIS(25, 3, and, "AND", "zero_page")
    CPU_OP_BIS(26, 5, rol, "ROL", "zero_page")
    CPU_OP_BIS(28, 4, plp, "PLP", "implied")
    CPU_OP_BIS(29, 2, and, "AND", "immediate")
    CPU_OP_BIS(2A, 2, rola,"ROL", "implied")
    CPU_OP_BIS(2C, 4, bit, "BIT", "absolute")
    CPU_OP_BIS(2D, 2, and, "AND", "absolute")
    CPU_OP_BIS(2E, 6, rol, "ROL", "absolute")
    CPU_OP_BIS(30, 2, bmi, "BMI", "relative")
    CPU_OP_BIS(31, 5, and, "AND", "indirect_y")
    CPU_OP_BIS(35, 4, and, "AND", "zero_page_x")
    CPU_OP_BIS(36, 6, rol, "ROL", "zero_page_x")
    CPU_OP_BIS(38, 2, sec, "SEC", "implied")
    CPU_OP_BIS(39, 4, and, "AND", "absolute_y")
    CPU_OP_BIS(3D, 4, and, "AND", "absolute_x")
    CPU_OP_BIS(3E, 7, rol, "ROL", "absolute_x")
    CPU_OP_BIS(40, 6, rti, "RTI", "implied")
    CPU_OP_BIS(41, 6, eor, "EOR", "indirect_x")
    CPU_OP_BIS(45, 3, eor, "EOR", "zero_page")
    CPU_OP_BIS(46, 5, lsr, "LSR", "zero_page")
    CPU_OP_BIS(48, 3, pha, "PHA", "implied")
    CPU_OP_BIS(49, 2, eor, "EOR", "immediate")
    CPU_OP_BIS(4A, 2, lsra,"LSR", "implied")
    CPU_OP_BIS(4C, 3, jmp, "JMP", "absolute")
    CPU_OP_BIS(4D, 4, eor, "EOR", "absolute")
    CPU_OP_BIS(4E, 6, lsr, "LSR", "absolute")
    CPU_OP_BIS(50, 2, bvc, "BVC", "relative")
    CPU_OP_BIS(51, 5, eor, "EOR", "indirect_y")
    CPU_OP_BIS(55, 4, eor, "EOR", "zero_page_x")
    CPU_OP_BIS(56, 6, lsr, "LSR", "zero_page_x")
    CPU_OP_BIS(58, 2, cli, "CLI", "implied")
    CPU_OP_BIS(59, 4, eor, "EOR", "absolute_y")
    CPU_OP_BIS(5D, 4, eor, "EOR", "absolute_x")
    CPU_OP_BIS(5E, 7, lsr, "LSR", "absolute_x")
    CPU_OP_BIS(60, 6, rts, "RTS", "implied")
    CPU_OP_BIS(61, 6, adc, "ADC", "indirect_x")
    CPU_OP_BIS(65, 3, adc, "ADC", "zero_page")
    CPU_OP_BIS(66, 5, ror, "ROR", "zero_page")
    CPU_OP_BIS(68, 4, pla, "PLA", "implied")
    CPU_OP_BIS(69, 2, adc, "ADC", "immediate")
    CPU_OP_BIS(6A, 2, rora,"ROR", "implied")
    CPU_OP_BIS(6C, 5, jmp, "JMP", "indirect")
    CPU_OP_BIS(6D, 4, adc, "ADC", "absolute")
    CPU_OP_BIS(6E, 6, ror, "ROR", "absolute")
    CPU_OP_BIS(70, 2, bvs, "BVS", "relative")
    CPU_OP_BIS(71, 5, adc, "ADC", "indirect_y")
    CPU_OP_BIS(75, 4, adc, "ADC", "zero_page_x")
    CPU_OP_BIS(76, 6, ror, "ROR", "zero_page_x")
    CPU_OP_BIS(78, 2, sei, "SEI", "implied")
    CPU_OP_BIS(79, 4, adc, "ADC", "absolute_y")
    CPU_OP_BIS(7D, 4, adc, "ADC", "absolute_x")
    CPU_OP_BIS(7E, 7, ror, "ROR", "absolute_x")
    CPU_OP_BIS(81, 6, sta, "STA", "indirect_x")
    CPU_OP_BIS(84, 3, sty, "STY", "zero_page")
    CPU_OP_BIS(85, 3, sta, "STA", "zero_page")
    CPU_OP_BIS(86, 3, stx, "STX", "zero_page")
    CPU_OP_BIS(88, 2, dey, "DEY", "implied")
    CPU_OP_BIS(8A, 2, txa, "TXA", "implied")
    CPU_OP_BIS(8C, 4, sty, "STY", "absolute")
    CPU_OP_BIS(8D, 4, sta, "STA", "absolute")
    CPU_OP_BIS(8E, 4, stx, "STX", "absolute")
    CPU_OP_BIS(90, 2, bcc, "BCC", "relative")
    CPU_OP_BIS(91, 6, sta, "STA", "indirect_y")
    CPU_OP_BIS(94, 4, sty, "STY", "zero_page_x")
    CPU_OP_BIS(95, 4, sta, "STA", "zero_page_x")
    CPU_OP_BIS(96, 4, stx, "STX", "zero_page_y")
    CPU_OP_BIS(98, 2, tya, "TYA", "implied")
    CPU_OP_BIS(99, 5, sta, "STA", "absolute_y")
    CPU_OP_BIS(9A, 2, txs, "TXS", "implied")
    CPU_OP_BIS(9D, 5, sta, "STA", "absolute_x")
    CPU_OP_BIS(A0, 2, ldy, "LDY", "immediate")
    CPU_OP_BIS(A1, 6, lda, "LDA", "indirect_x")
    CPU_OP_BIS(A2, 2, ldx, "LDX", "immediate")
    CPU_OP_BIS(A4, 3, ldy, "LDY", "zero_page")
    CPU_OP_BIS(A5, 3, lda, "LDA", "zero_page")
    CPU_OP_BIS(A6, 3, ldx, "LDX", "zero_page")
    CPU_OP_BIS(A8, 2, tay, "TAY", "implied")
    CPU_OP_BIS(A9, 2, lda, "LDA", "immediate")
    CPU_OP_BIS(AA, 2, tax, "TAX", "implied")
    CPU_OP_BIS(AC, 4, ldy, "LDY", "absolute")
    CPU_OP_BIS(AD, 4, lda, "LDA", "absolute")
    CPU_OP_BIS(AE, 4, ldx, "LDX", "absolute")
    CPU_OP_BIS(B0, 2, bcs, "BCS", "relative")
    CPU_OP_BIS(B1, 5, lda, "LDA", "indirect_y")
    CPU_OP_BIS(B4, 4, ldy, "LDY", "zero_page_x")
    CPU_OP_BIS(B5, 4, lda, "LDA", "zero_page_x")
    CPU_OP_BIS(B6, 4, ldx, "LDX", "zero_page_y")
    CPU_OP_BIS(B8, 2, clv, "CLV", "implied")
    CPU_OP_BIS(B9, 4, lda, "LDA", "absolute_y")
    CPU_OP_BIS(BA, 2, tsx, "TSX", "implied")
    CPU_OP_BIS(BC, 4, ldy, "LDY", "absolute_x")
    CPU_OP_BIS(BD, 4, lda, "LDA", "absolute_x")
    CPU_OP_BIS(BE, 4, ldx, "LDX", "absolute_y")
    CPU_OP_BIS(C0, 2, cpy, "CPY", "immediate")
    CPU_OP_BIS(C1, 6, cmp, "CMP", "indirect_x")
    CPU_OP_BIS(C4, 3, cpy, "CPY", "zero_page")
    CPU_OP_BIS(C5, 3, cmp, "CMP", "zero_page")
    CPU_OP_BIS(C6, 5, dec, "DEC", "zero_page")
    CPU_OP_BIS(C8, 2, iny, "INY", "implied")
    CPU_OP_BIS(C9, 2, cmp, "CMP", "immediate")
    CPU_OP_BIS(CA, 2, dex, "DEX", "implied")
    CPU_OP_BIS(CC, 4, cpy, "CPY", "absolute")
    CPU_OP_BIS(CD, 4, cmp, "CMP", "absolute")
    CPU_OP_BIS(CE, 6, dec, "DEC", "absolute")
    CPU_OP_BIS(D0, 2, bne, "BNE", "relative")
    CPU_OP_BIS(D1, 5, cmp, "CMP", "indirect_y")
    CPU_OP_BIS(D5, 4, cmp, "CMP", "zero_page_x")
    CPU_OP_BIS(D6, 6, dec, "DEC", "zero_page_x")
    CPU_OP_BIS(D8, 2, cld, "CLD", "implied")
    CPU_OP_BIS(D9, 4, cmp, "CMP", "absolute_y")
    CPU_OP_BIS(DD, 4, cmp, "CMP", "absolute_x")
    CPU_OP_BIS(DE, 7, dec, "DEC", "absolute_x")
    CPU_OP_BIS(E0, 2, cpx, "CPX", "immediate")
    CPU_OP_BIS(E1, 6, sbc, "SBC", "indirect_x")
    CPU_OP_BIS(E4, 3, cpx, "CPX", "zero_page")
    CPU_OP_BIS(E5, 3, sbc, "SBC", "zero_page")
    CPU_OP_BIS(E6, 5, inc, "INC", "zero_page")
    CPU_OP_BIS(E8, 2, inx, "INX", "implied")
    CPU_OP_BIS(E9, 2, sbc, "SBC", "immediate")
    CPU_OP_BIS(EA, 2, nop, "NOP", "implied")
    CPU_OP_BIS(EC, 4, cpx, "CPX", "absolute")
    CPU_OP_BIS(ED, 4, sbc, "SBC", "absolute")
    CPU_OP_BIS(EE, 6, inc, "INC", "absolute")
    CPU_OP_BIS(F0, 2, beq, "BEQ", "relative")
    CPU_OP_BIS(F1, 5, sbc, "SBC", "indirect_y")
    CPU_OP_BIS(F5, 4, sbc, "SBC", "zero_page_x")
    CPU_OP_BIS(F6, 6, inc, "INC", "zero_page_x")
    CPU_OP_BIS(F8, 2, sed, "SED", "implied")
    CPU_OP_BIS(F9, 4, sbc, "SBC", "absolute_y")
    CPU_OP_BIS(FD, 4, sbc, "SBC", "absolute_x")
    CPU_OP_BIS(FE, 7, inc, "INC", "absolute_x")

    CPU_OP_EIS(03, 8, aso, "SLO", "indirect_x")
    CPU_OP_EIS(07, 5, aso, "SLO", "zero_page")
    CPU_OP_EIS(0F, 6, aso, "SLO", "absolute")
    CPU_OP_EIS(13, 8, aso, "SLO", "indirect_y")
    CPU_OP_EIS(17, 6, aso, "SLO", "zero_page_x")
    CPU_OP_EIS(1B, 7, aso, "SLO", "absolute_y")
    CPU_OP_EIS(1F, 7, aso, "SLO", "absolute_x")
    CPU_OP_EIS(23, 8, rla, "RLA", "indirect_x")
    CPU_OP_EIS(27, 5, rla, "RLA", "zero_page")
    CPU_OP_EIS(2F, 6, rla, "RLA", "absolute")
    CPU_OP_EIS(33, 8, rla, "RLA", "indirect_y")
    CPU_OP_EIS(37, 6, rla, "RLA", "zero_page_x")
    CPU_OP_EIS(3B, 7, rla, "RLA", "absolute_y")
    CPU_OP_EIS(3F, 7, rla, "RLA", "absolute_x")
    CPU_OP_EIS(43, 8, lse, "SRE", "indirect_x")
    CPU_OP_EIS(47, 5, lse, "SRE", "zero_page")
    CPU_OP_EIS(4F, 6, lse, "SRE", "absolute")
    CPU_OP_EIS(53, 8, lse, "SRE", "indirect_y")
    CPU_OP_EIS(57, 6, lse, "SRE", "zero_page_x")
    CPU_OP_EIS(5B, 7, lse, "SRE", "absolute_y")
    CPU_OP_EIS(5F, 7, lse, "SRE", "absolute_x")
    CPU_OP_EIS(63, 8, rra, "RRA", "indirect_x")
    CPU_OP_EIS(67, 5, rra, "RRA", "zero_page")
    CPU_OP_EIS(6F, 6, rra, "RRA", "absolute")
    CPU_OP_EIS(73, 8, rra, "RRA", "indirect_y")
    CPU_OP_EIS(77, 6, rra, "RRA", "zero_page_x")
    CPU_OP_EIS(7B, 7, rra, "RRA", "absolute_y")
    CPU_OP_EIS(7F, 7, rra, "RRA", "absolute_x")
    CPU_OP_EIS(83, 6, axs, "SAX", "indirect_x")
    CPU_OP_EIS(87, 3, axs, "SAX", "zero_page")
    CPU_OP_EIS(8F, 4, axs, "SAX", "absolute")
    CPU_OP_EIS(93, 6, axa, "SAX", "indirect_y")
    CPU_OP_EIS(97, 4, axs, "SAX", "zero_page_y")
    CPU_OP_EIS(9F, 5, axa, "SAX", "absolute_y")
    CPU_OP_EIS(A3, 6, lax, "LAX", "indirect_x")
    CPU_OP_EIS(A7, 3, lax, "LAX", "zero_page")
    CPU_OP_EIS(AF, 4, lax, "LAX", "absolute")
    CPU_OP_EIS(B3, 5, lax, "LAX", "indirect_y")
    CPU_OP_EIS(B7, 4, lax, "LAX", "zero_page_y")
    CPU_OP_EIS(BF, 4, lax, "LAX", "absolute_y")
    CPU_OP_EIS(C3, 8, dcm, "DCP", "indirect_x")
    CPU_OP_EIS(C7, 5, dcm, "DCP", "zero_page")
    CPU_OP_EIS(CF, 6, dcm, "DCP", "absolute")
    CPU_OP_EIS(D3, 8, dcm, "DCP", "indirect_y")
    CPU_OP_EIS(D7, 6, dcm, "DCP", "zero_page_x")
    CPU_OP_EIS(DB, 7, dcm, "DCP", "absolute_y")
    CPU_OP_EIS(DF, 7, dcm, "DCP", "absolute_x")
    CPU_OP_EIS(E3, 8, ins, "ISB", "indirect_x")
    CPU_OP_EIS(E7, 5, ins, "ISB", "zero_page")
    CPU_OP_EIS(EB, 2, sbc, "SBC", "immediate")
    CPU_OP_EIS(EF, 6, ins, "ISB", "absolute")
    CPU_OP_EIS(F3, 8, ins, "ISB", "indirect_y")
    CPU_OP_EIS(F7, 6, ins, "ISB", "zero_page_x")
    CPU_OP_EIS(FB, 7, ins, "ISB", "absolute_y")
    CPU_OP_EIS(FF, 7, ins, "ISB", "absolute_x")

    CPU_OP_NII(04, "zero_page")
    CPU_OP_NII(0C, "absolute")
    CPU_OP_NII(14, "zero_page_x")
    CPU_OP_NII(1A, "implied")
    CPU_OP_NII(1C, "absolute_x")
    CPU_OP_NII(34, "zero_page_x")
    CPU_OP_NII(3A, "implied")
    CPU_OP_NII(3C, "absolute_x")
    CPU_OP_NII(44, "zero_page")
    CPU_OP_NII(54, "zero_page_x")
    CPU_OP_NII(5A, "implied")
    CPU_OP_NII(5C, "absolute_x")
    CPU_OP_NII(64, "zero_page")
    CPU_OP_NII(74, "zero_page_x")
    CPU_OP_NII(7A, "implied")
    CPU_OP_NII(7C, "absolute_x")
    CPU_OP_NII(80, "immediate")
    CPU_OP_NII(D4, "zero_page_x")
    CPU_OP_NII(DA, "implied")
    CPU_OP_NII(DC, "absolute_x")
    CPU_OP_NII(F4, "zero_page_x")
    CPU_OP_NII(FA, "implied")
    CPU_OP_NII(FC, "absolute_x")
}

// void cpu_reset()
// {
//     cpu.PC = cpu_reset_interrupt_address();
//     cpu.SP -= 3;
//     cpu.P |= interrupt_flag;
// }

// void cpu_interrupt()
// {
//     // if (ppu_in_vblank()) {
//         if (ppu_generates_nmi()) {
//             cpu.P |= interrupt_flag;
//             cpu_unset_flag(unused_bp);
//             cpu_stack_pushw(cpu.PC);
//             cpu_stack_pushb(cpu.P);
//             cpu.PC = cpu_nmi_interrupt_address();
//         }
//     // }
// }

// inline unsigned long long cpu_clock()
// {
//     return cpu_cycles;
// }

// void cpu_run(long cycles)
// {
//     while (cycles > 0) {
//         op_code = memory_readb(cpu.PC++);
//         if (cpu_op_address_mode[op_code] == NULL) {
//         }
//         else {
//             cpu_op_address_mode[op_code]();
//             cpu_op_handler[op_code]();
//         }
//         cycles -= cpu_op_cycles[op_code] + op_cycles;
//         cpu_cycles -= cpu_op_cycles[op_code] + op_cycles;
//         op_cycles = 0;
//     }
// }

FILE *g_fp;
void init_log(void)
{
    if (g_fp) {
        return;
    }
   g_fp = fopen("./log.txt", "w+");
}