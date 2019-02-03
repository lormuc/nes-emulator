#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <functional>

#include "machine.hpp"
#include "misc.hpp"
#include "gfx.hpp"

namespace {
    bool reset_flag;
    bool nmi_flag;
    bool irq_flag;

    bool ready;

    std::array<char, 0x0800> memory;
    std::vector<char> prg_rom;

    t_adr arg;
    unsigned r_cyc;
    unsigned w_cyc;
    unsigned long step_count;
    unsigned long cycle_count;

    // registers

    t_adr pc; // program counter
    char sp; // stack pointer
    char ra; // accumulator
    char rx; // x
    char ry; // y
    char rp; // processor status

    // addresses

    const t_adr adr_ra = 0x10000;
    const t_adr adr_rx = 0x10001;
    const t_adr adr_ry = 0x10002;
    const t_adr adr_rp = 0x10003;
    const t_adr adr_sp = 0x10004;

    // declarations

    // addressing modes

    void m_imp();
    void m_acc();
    void m_imm();
    void m_rel();
    void m_zpg();
    void m_zpx();
    void m_zpy();
    void m_abs();
    void m_abx();
    void m_aby();
    void m_ind();
    void m_inx();
    void m_iny();

    // instructions

    void i_lda();
    void i_ldx();
    void i_ldy();

    void i_sta();
    void i_stx();
    void i_sty();

    void i_tax();
    void i_tay();
    void i_txa();
    void i_tya();
    void i_tsx();
    void i_txs();

    void i_pha();
    void i_pla();

    void i_php();
    void i_plp();

    void i_and();
    void i_eor();
    void i_ora();
    void i_bit();

    void i_inc();
    void i_dec();

    void i_inx();
    void i_dex();
    void i_iny();
    void i_dey();

    void i_jmp();
    void i_jsr();
    void i_rts();

    void i_clc();
    void i_sec();
    void i_clv();
    void i_cld();
    void i_sed();
    void i_cli();
    void i_sei();

    void i_bcc();
    void i_bcs();
    void i_bpl();
    void i_bmi();
    void i_bne();
    void i_beq();
    void i_bvc();
    void i_bvs();

    void i_brk();
    void i_rti();
    void i_nop();

    void i_asl();
    void i_lsr();
    void i_rol();
    void i_ror();

    void i_adc();
    void i_sbc();
    void i_cmp();
    void i_cpx();
    void i_cpy();

    void i_isc();

    // helper functions

    void set_carry_flag(bool);
    bool get_carry_flag();
    void set_zero_flag(bool);
    bool get_zero_flag();
    void set_overflow_flag(bool);
    bool get_overflow_flag();
    void set_negative_flag(bool);
    bool get_negative_flag();
    void set_interrupt_disable_flag(bool);
    bool get_interrupt_disable_flag();
    void set_break_flag(bool);
    bool get_break_flag();
    char read_mem(t_adr);
    t_adr read_mem_2(t_adr);
    void write_mem(t_adr, char);
    void set_with_flags(t_adr, char);
    void set_arg(t_adr, int);
    void push(char);
    char pull();
    void push_adr(t_adr);
    t_adr pull_adr();
    void short_jump_if(bool);

    // definitions

    t_adr make_adr(char hi, char lo) {
        return (t_adr(hi) << 8) | lo;
    }

    void add_with_carry(char& x, char y, bool& c) {
        unsigned z = x;
        z += y;
        c = (z >= 0x100u);
        x = z;
    }

    void process_interrupt() {
        if (nmi_flag) {
            std::cout << "nmi\n";
            nmi_flag = 0;
            push_adr(pc);
            auto val = rp;
            set_bit(val, 5, 1);
            set_bit(val, 4, 0);
            push(val);
            set_interrupt_disable_flag(1);
            pc = read_mem_2(0xfffa);
        }
        if (reset_flag) {
            reset_flag = 0;
            set_interrupt_disable_flag(1);
            pc = read_mem_2(0xfffc);
        } else if (irq_flag) {
            irq_flag = 0;
            push_adr(pc);
            auto val = rp;
            set_bit(val, 5, 1);
            set_bit(val, 4, 0);
            push(val);
            set_interrupt_disable_flag(1);
            pc = read_mem_2(0xfffe);
        }
    }

    int step() {
        // machine::print_info();

        auto idf = get_interrupt_disable_flag();
        if (nmi_flag or (not idf and (reset_flag or irq_flag))) {
            process_interrupt();
            cycle_count = 6;
            return 0;
        }

        // fetch an instruction
        auto opcode = read_mem(pc);
        pc++;

        // execute the given instruction
        switch (opcode) {
        case 0x29: m_imm(); i_and(); break;
        case 0x25: m_zpg(); i_and(); break;
        case 0x35: m_zpx(); i_and(); break;
        case 0x2d: m_abs(); i_and(); break;
        case 0x3d: m_abx(); i_and(); break;
        case 0x39: m_aby(); i_and(); break;
        case 0x21: m_inx(); i_and(); break;
        case 0x31: m_iny(); i_and(); break;

        case 0x49: m_imm(); i_eor(); break;
        case 0x45: m_zpg(); i_eor(); break;
        case 0x55: m_zpx(); i_eor(); break;
        case 0x4d: m_abs(); i_eor(); break;
        case 0x5d: m_abx(); i_eor(); break;
        case 0x59: m_aby(); i_eor(); break;
        case 0x41: m_inx(); i_eor(); break;
        case 0x51: m_iny(); i_eor(); break;

        case 0x09: m_imm(); i_ora(); break;
        case 0x05: m_zpg(); i_ora(); break;
        case 0x15: m_zpx(); i_ora(); break;
        case 0x0d: m_abs(); i_ora(); break;
        case 0x1d: m_abx(); i_ora(); break;
        case 0x19: m_aby(); i_ora(); break;
        case 0x01: m_inx(); i_ora(); break;
        case 0x11: m_iny(); i_ora(); break;

        case 0x24: m_zpg(); i_bit(); break;
        case 0x2c: m_abs(); i_bit(); break;

        case 0xa9: m_imm(); i_lda(); break;
        case 0xa5: m_zpg(); i_lda(); break;
        case 0xb5: m_zpx(); i_lda(); break;
        case 0xad: m_abs(); i_lda(); break;
        case 0xbd: m_abx(); i_lda(); break;
        case 0xb9: m_aby(); i_lda(); break;
        case 0xa1: m_inx(); i_lda(); break;
        case 0xb1: m_iny(); i_lda(); break;

        case 0xa2: m_imm(); i_ldx(); break;
        case 0xa6: m_zpg(); i_ldx(); break;
        case 0xb6: m_zpy(); i_ldx(); break;
        case 0xae: m_abs(); i_ldx(); break;
        case 0xbe: m_aby(); i_ldx(); break;

        case 0xa0: m_imm(); i_ldy(); break;
        case 0xa4: m_zpg(); i_ldy(); break;
        case 0xb4: m_zpx(); i_ldy(); break;
        case 0xac: m_abs(); i_ldy(); break;
        case 0xbc: m_abx(); i_ldy(); break;

        case 0x85: m_zpg(); i_sta(); break;
        case 0x95: m_zpx(); i_sta(); break;
        case 0x8d: m_abs(); i_sta(); break;
        case 0x9d: m_abx(); i_sta(); break;
        case 0x99: m_aby(); i_sta(); break;
        case 0x81: m_inx(); i_sta(); break;
        case 0x91: m_iny(); i_sta(); break;

        case 0x86: m_zpg(); i_stx(); break;
        case 0x96: m_zpy(); i_stx(); break;
        case 0x8e: m_abs(); i_stx(); break;

        case 0x84: m_zpg(); i_sty(); break;
        case 0x94: m_zpx(); i_sty(); break;
        case 0x8c: m_abs(); i_sty(); break;

        case 0xaa: m_imp(); i_tax(); break;
        case 0xa8: m_imp(); i_tay(); break;
        case 0x8a: m_imp(); i_txa(); break;
        case 0x98: m_imp(); i_tya(); break;

        case 0xe6: m_zpg(); i_inc(); break;
        case 0xf6: m_zpx(); i_inc(); break;
        case 0xee: m_abs(); i_inc(); break;
        case 0xfe: m_abx(); i_inc(); break;
        case 0xe8: m_imp(); i_inx(); break;
        case 0xc8: m_imp(); i_iny(); break;

        case 0xc6: m_zpg(); i_dec(); break;
        case 0xd6: m_zpx(); i_dec(); break;
        case 0xce: m_abs(); i_dec(); break;
        case 0xde: m_abx(); i_dec(); break;
        case 0xca: m_imp(); i_dex(); break;
        case 0x88: m_imp(); i_dey(); break;

        case 0x0a: m_acc(); i_asl(); break;
        case 0x06: m_zpg(); i_asl(); break;
        case 0x16: m_zpx(); i_asl(); break;
        case 0x0e: m_abs(); i_asl(); break;
        case 0x1e: m_abx(); i_asl(); break;

        case 0x4a: m_acc(); i_lsr(); break;
        case 0x46: m_zpg(); i_lsr(); break;
        case 0x56: m_zpx(); i_lsr(); break;
        case 0x4e: m_abs(); i_lsr(); break;
        case 0x5e: m_abx(); i_lsr(); break;

        case 0x2a: m_acc(); i_rol(); break;
        case 0x26: m_zpg(); i_rol(); break;
        case 0x36: m_zpx(); i_rol(); break;
        case 0x2e: m_abs(); i_rol(); break;
        case 0x3e: m_abx(); i_rol(); break;

        case 0x6a: m_acc(); i_ror(); break;
        case 0x66: m_zpg(); i_ror(); break;
        case 0x76: m_zpx(); i_ror(); break;
        case 0x6e: m_abs(); i_ror(); break;
        case 0x7e: m_abx(); i_ror(); break;

        case 0xba: m_imp(); i_tsx(); break;
        case 0x9a: m_imp(); i_txs(); break;
        case 0x48: m_imp(); i_pha(); break;
        case 0x08: m_imp(); i_php(); break;
        case 0x68: m_imp(); i_pla(); break;
        case 0x28: m_imp(); i_plp(); break;

        case 0x4c: m_abs(); i_jmp(); break;
        case 0x6c: m_ind(); i_jmp(); break;
        case 0x20: m_abs(); i_jsr(); break;
        case 0x60: m_imp(); i_rts(); break;

        case 0x90: m_rel(); i_bcc(); break;
        case 0xb0: m_rel(); i_bcs(); break;
        case 0xf0: m_rel(); i_beq(); break;
        case 0x30: m_rel(); i_bmi(); break;
        case 0xd0: m_rel(); i_bne(); break;
        case 0x10: m_rel(); i_bpl(); break;
        case 0x50: m_rel(); i_bvc(); break;
        case 0x70: m_rel(); i_bvs(); break;

        case 0x18: m_imp(); i_clc(); break;
        case 0xd8: m_imp(); i_cld(); break;
        case 0x58: m_imp(); i_cli(); break;
        case 0xb8: m_imp(); i_clv(); break;
        case 0x38: m_imp(); i_sec(); break;
        case 0xf8: m_imp(); i_sed(); break;
        case 0x78: m_imp(); i_sei(); break;

        case 0x69: m_imm(); i_adc(); break;
        case 0x65: m_zpg(); i_adc(); break;
        case 0x75: m_zpx(); i_adc(); break;
        case 0x6d: m_abs(); i_adc(); break;
        case 0x7d: m_abx(); i_adc(); break;
        case 0x79: m_aby(); i_adc(); break;
        case 0x61: m_inx(); i_adc(); break;
        case 0x71: m_iny(); i_adc(); break;

        case 0xe9: m_imm(); i_sbc(); break;
        case 0xe5: m_zpg(); i_sbc(); break;
        case 0xf5: m_zpx(); i_sbc(); break;
        case 0xed: m_abs(); i_sbc(); break;
        case 0xfd: m_abx(); i_sbc(); break;
        case 0xf9: m_aby(); i_sbc(); break;
        case 0xe1: m_inx(); i_sbc(); break;
        case 0xf1: m_iny(); i_sbc(); break;

        case 0xc9: m_imm(); i_cmp(); break;
        case 0xc5: m_zpg(); i_cmp(); break;
        case 0xd5: m_zpx(); i_cmp(); break;
        case 0xcd: m_abs(); i_cmp(); break;
        case 0xdd: m_abx(); i_cmp(); break;
        case 0xd9: m_aby(); i_cmp(); break;
        case 0xc1: m_inx(); i_cmp(); break;
        case 0xd1: m_iny(); i_cmp(); break;

        case 0xe0: m_imm(); i_cpx(); break;
        case 0xe4: m_zpg(); i_cpx(); break;
        case 0xec: m_abs(); i_cpx(); break;

        case 0xc0: m_imm(); i_cpy(); break;
        case 0xc4: m_zpg(); i_cpy(); break;
        case 0xcc: m_abs(); i_cpy(); break;

        case 0xea: m_imp(); i_nop(); break;
        case 0x00: m_imp(); i_brk(); break;
        case 0x40: m_imp(); i_rti(); break;

        case 0x04: m_zpg(); i_nop(); break;
        case 0xe7: m_zpg(); i_isc(); break;

        default:
            return -1;
        }

        step_count++;

        return 0;
    }

    void m_imp() {
        r_cyc = 0;
        w_cyc = 0;
    }

    void m_acc() {
        r_cyc = 0;
        w_cyc = 0;
        set_arg(adr_ra, 0);
    }

    void m_imm() {
        r_cyc = 0;
        w_cyc = 0;
        set_arg(pc, 1);
    }

    void m_rel() {
        set_arg(pc, 1);
        r_cyc = 0;
        w_cyc = 0;
    }

    void m_zpg() {
        r_cyc = 1;
        w_cyc = 1;
        set_arg(read_mem(pc), 1);
    }

    void m_zpx() {
        r_cyc = 2;
        w_cyc = 2;
        set_arg(char(read_mem(pc) + rx), 1);
    }

    void m_zpy() {
        r_cyc = 2;
        w_cyc = 2;
        set_arg(char(read_mem(pc) + ry), 1);
    }

    void m_abs() {
        r_cyc = 2;
        w_cyc = 2;
        set_arg(read_mem_2(pc), 2);
    }

    void m_abx() {
        auto lo = read_mem(pc);
        auto hi = read_mem(pc + 1);
        bool carry;
        add_with_carry(lo, rx, carry);
        hi += carry;
        set_arg(make_adr(hi, lo), 2);
        r_cyc = 2 + carry;
        w_cyc = 3;
    }

    void m_aby() {
        auto lo = read_mem(pc);
        auto hi = read_mem(pc + 1);
        bool carry;
        add_with_carry(lo, ry, carry);
        hi += carry;
        set_arg(make_adr(hi, lo), 2);
        r_cyc = 2 + carry;
        w_cyc = 3;
    }

    void m_ind() {
        set_arg(read_mem_2(read_mem_2(pc)), 2);
        r_cyc = 4;
    }

    void m_inx() {
        r_cyc = 4;
        w_cyc = 4;
        set_arg(read_mem_2(char(read_mem(pc) + rx)), 1);
    }

    void m_iny() {
        auto adr = read_mem(pc);
        auto lo = read_mem(adr);
        adr++;
        auto hi = read_mem(adr);
        bool carry;
        add_with_carry(lo, ry, carry);
        hi += carry;
        set_arg(make_adr(hi, lo), 1);
        r_cyc = 3 + carry;
        w_cyc = 4;
    }

    void i_lda() {
        set_with_flags(adr_ra, read_mem(arg));
        cycle_count += 2 + r_cyc;
    }

    void i_ldx() {
        set_with_flags(adr_rx, read_mem(arg));
        cycle_count += 2 + r_cyc;
    }

    void i_ldy() {
        set_with_flags(adr_ry, read_mem(arg));
        cycle_count += 2 + r_cyc;
    }

    void i_sta() {
        cycle_count += 2 + w_cyc;
        write_mem(arg, ra);
    }

    void i_stx() {
        cycle_count += 2 + r_cyc;
        write_mem(arg, rx);
    }

    void i_sty() {
        cycle_count += 2 + r_cyc;
        write_mem(arg, ry);
    }

    void i_tax() {
        set_with_flags(adr_rx, ra);
        cycle_count += 2;
    }

    void i_tay() {
        set_with_flags(adr_ry, ra);
        cycle_count += 2;
    }

    void i_txa() {
        set_with_flags(adr_ra, rx);
        cycle_count += 2;
    }

    void i_tya() {
        set_with_flags(adr_ra, ry);
        cycle_count += 2;
    }

    void i_tsx() {
        set_with_flags(adr_rx, sp);
        cycle_count += 2;
    }

    void i_txs() {
        sp = rx;
        cycle_count += 2;
    }

    void i_pha() {
        push(ra);
        cycle_count += 3;
    }

    void i_pla() {
        set_with_flags(adr_ra, pull());
        cycle_count += 4;
    }

    void i_php() {
        auto val = rp;
        set_bit(val, 5, 1);
        set_bit(val, 4, 1);
        push(val);
        cycle_count += 3;
    }

    void i_plp() {
        rp = pull();
        cycle_count += 4;
    }

    void i_and() {
        set_with_flags(adr_ra, ra & read_mem(arg));
        cycle_count += 2 + r_cyc;
    }

    void i_eor() {
        set_with_flags(adr_ra, ra ^ read_mem(arg));
        cycle_count += 2 + r_cyc;
    }

    void i_ora() {
        set_with_flags(adr_ra, ra | read_mem(arg));
        cycle_count += 2 + r_cyc;
    }

    void i_bit() {
        auto val = read_mem(arg);
        set_zero_flag((ra & val) == 0);
        set_overflow_flag(get_bit(val, 6));
        set_negative_flag(get_bit(val, 7));
        cycle_count += 2 + r_cyc;
    };

    void i_inc() {
        set_with_flags(arg, read_mem(arg) + 1);
        cycle_count += 4 + w_cyc;
    }

    void i_dec() {
        set_with_flags(arg, read_mem(arg) - 1);
        cycle_count += 4 + w_cyc;
    }

    void i_inx() {
        set_with_flags(adr_rx, rx + 1);
        cycle_count += 2;
    }

    void i_dex() {
        set_with_flags(adr_rx, rx - 1);
        cycle_count += 2;
    }

    void i_iny() {
        set_with_flags(adr_ry, ry + 1);
        cycle_count += 2;
    }

    void i_dey() {
        set_with_flags(adr_ry, ry - 1);
        cycle_count += 2;
    }

    void i_jmp() {
        pc = arg;
        cycle_count += 1 + r_cyc;
    }

    void i_jsr() {
        push_adr(pc - 1);
        pc = arg;
        cycle_count += 4 + r_cyc;
    }

    void i_rts() {
        pc = pull_adr() + 1;
        cycle_count += 6;
    }

    void i_clc() {
        set_carry_flag(0);
        cycle_count += 2;
    }

    void i_sec() {
        set_carry_flag(1);
        cycle_count += 2;
    }

    void i_clv() {
        set_overflow_flag(0);
        cycle_count += 2;
    }

    void i_cld() {
        set_bit(rp, 3, 0);
        cycle_count += 2;
    }

    void i_sed() {
        set_bit(rp, 3, 1);
        cycle_count += 2;
    }

    void i_cli() {
        set_bit(rp, 2, 0);
        cycle_count += 2;
    }

    void i_sei() {
        set_bit(rp, 2, 1);
        cycle_count += 2;
    }

    void i_bcc() {
        short_jump_if(get_carry_flag() == 0);
    }

    void i_bcs() {
        short_jump_if(get_carry_flag() == 1);
    }

    void i_bpl() {
        short_jump_if(get_negative_flag() == 0);
    }

    void i_bmi() {
        short_jump_if(get_negative_flag() == 1);
    }

    void i_bne() {
        short_jump_if(get_zero_flag() == 0);
    }

    void i_beq() {
        short_jump_if(get_zero_flag() == 1);
    }

    void i_bvc() {
        short_jump_if(get_overflow_flag() == 0);
    }

    void i_bvs() {
        short_jump_if(get_overflow_flag() == 1);
    }

    void i_brk() {
        push_adr(pc + 1);
        auto val = rp;
        set_bit(val, 5, 1);
        set_bit(val, 4, 1);
        push(val);
        pc = read_mem_2(0xfffe);
        set_break_flag(1);
        set_interrupt_disable_flag(1);
        cycle_count += 7;
    }

    void i_rti() {
        rp = pull();
        pc = pull_adr();
        cycle_count += 6;
    }

    void i_nop() {
        cycle_count += 2 + r_cyc;
    }

    void i_asl() {
        auto val = read_mem(arg);
        set_carry_flag(get_bit(val, 7));
        set_with_flags(arg, val << 1);
        cycle_count += (arg == adr_ra) ? 2 : (4 + w_cyc);
    }

    void i_lsr() {
        auto val = read_mem(arg);
        set_carry_flag(get_bit(val, 0));
        set_with_flags(arg, val >> 1);
        cycle_count += (arg == adr_ra) ? 2 : (4 + w_cyc);
    }

    void i_rol() {
        auto val = read_mem(arg);
        auto ca = get_carry_flag();
        set_carry_flag(get_bit(val, 7));
        val <<= 1;
        set_bit(val, 0, ca);
        set_with_flags(arg, val);
        cycle_count += (arg == adr_ra) ? 2 : (4 + w_cyc);
    }

    void i_ror() {
        auto val = read_mem(arg);
        auto ca = get_carry_flag();
        set_carry_flag(get_bit(val, 0));
        val >>= 1;
        set_bit(val, 7, ca);
        set_with_flags(arg, val);
        cycle_count += (arg == adr_ra) ? 2 : (4 + w_cyc);
    }

    void i_adc() {
        unsigned res = ra;
        unsigned v = read_mem(arg);
        auto ca = get_carry_flag();
        v += ca;
        auto a7 = get_bit(ra, 7);
        auto b7 = get_bit(v, 7);
        res += v;
        set_with_flags(adr_ra, res);
        auto c7 = get_bit(ra, 7);
        if (ca == 1 and v == 0x80u) {
            set_overflow_flag(a7 == 0);
        } else {
            set_overflow_flag(a7 == b7 and a7 != c7);
        }
        set_carry_flag(res >= 0x100u);
        cycle_count += 2 + r_cyc;
    }

    void i_sbc() {
        unsigned res = ra;
        unsigned xx = read_mem(arg);
        auto nc = !get_carry_flag();
        xx += nc;
        auto a7 = get_bit(ra, 7);
        auto b7 = get_bit(xx, 7);
        res -= xx;
        set_with_flags(adr_ra, res);
        auto c7 = get_bit(ra, 7);
        if (nc == 1 and xx == 0x80u) {
            set_overflow_flag(a7 == 1);
        } else {
            set_overflow_flag(a7 != b7 and b7 == c7);
        }
        set_carry_flag(res < 0x100);
        cycle_count += 2 + r_cyc;
    }

    void i_cmp() {
        auto val = read_mem(arg);
        set_carry_flag(ra >= val);
        set_zero_flag(ra == val);
        set_negative_flag(get_bit(ra - val, 7));
        cycle_count += 2 + r_cyc;
    }

    void i_cpx() {
        auto val = read_mem(arg);
        set_carry_flag(rx >= val);
        set_zero_flag(rx == val);
        set_negative_flag(get_bit(rx - val, 7));
        cycle_count += 2 + r_cyc;
    }

    void i_cpy() {
        auto val = read_mem(arg);
        set_carry_flag(ry >= val);
        set_zero_flag(ry == val);
        set_negative_flag(get_bit(ry - val, 7));
        cycle_count += 2 + r_cyc;
    }

    void i_isc() {
        i_inc();
        i_sbc();
        cycle_count = 4 + w_cyc;
    }

    char read_mem(t_adr adr) {
        char res;
        bool bad = false;
        if (adr < 0x2000u) {
            adr %= 0x0800u;
            res = memory[adr];
        } else if (adr < 0x4000u) {
            adr &= 0x2007u;
            res = gfx::get(adr);
        } else if (adr < 0x8000u) {
            bad = true;
        } else if (adr < 0x10000ul) {
            adr -= 0x8000u;
            if (prg_rom.size() == 0x4000u) {
                // std::cout << "here\n";
                adr %= 0x4000u;
            }
            res = prg_rom[adr];
        } else {
            switch (adr) {
            case adr_ra: res = ra; break;
            case adr_rx: res = rx; break;
            case adr_ry: res = ry; break;
            case adr_rp: res = rp; break;
            case adr_sp: res = sp; break;
            default: bad = true; break;
            }
        }
        if (bad) {
            std::cout << "machine bad read_mem "; print_hex(adr); std::cout << "\n";
            res = 0x00;
        }
        return res;
    }

    void write_mem(t_adr adr, char val) {
        bool bad = false;
        if (adr < 0x2000u) {
            adr %= 0x0800u;
            memory[adr] = val;
        } else if (adr < 0x4000u) {
            adr &= 0x2007u;
            gfx::set(adr, val);
        } else if (adr < 0x8000u) {
            bad = true;
        } else if (adr < 0x10000ul) {
            bad = true;
        } else {
            switch (adr) {
            case adr_ra: ra = val; break;
            case adr_rx: rx = val; break;
            case adr_ry: ry = val; break;
            case adr_rp: rp = val; break;
            case adr_sp: sp = val; break;
            default: bad = true; break;
            }
        }
        if (bad) {
            std::cout << "machine bad write_mem "; print_hex(adr); std::cout << "\n";
        }
    }

    t_adr read_mem_2(t_adr adr) {
        auto v = read_mem(adr);
        auto u = read_mem(adr + 1);
        return make_adr(u, v);
    }

    void set_with_flags(t_adr adr, char v) {
        write_mem(adr, v);
        set_zero_flag(v == 0);
        set_negative_flag(get_bit(v, 7));
    }

    void push(char val) {
        write_mem(0x100u + sp, val);
        sp--;
    }

    char pull() {
        sp++;
        return read_mem(0x100u + sp);
    }

    void push_adr(t_adr adr) {
        push(char(adr >> 8));
        push(char(adr));
    }

    t_adr pull_adr() {
        auto v = pull();
        auto u = pull();
        return make_adr(u, v);
    }

    void short_jump_if(bool cond) {
        cycle_count += 2;
        if (cond) {
            cycle_count++;
            auto offset = read_mem(arg);
            char old_page = pc >> 8;
            if (offset < 0x80u) {
                pc += offset;
            } else {
                offset = ~offset;
                pc -= offset + 1u;
            }
            char new_page = pc >> 8;
            if (new_page != old_page) {
                cycle_count++;
            }
        }
    }

    void set_arg(t_adr adr, int n) {
        arg = adr;
        pc += n;
    }

    void set_carry_flag(bool x) {
        set_bit(rp, 0, x);
    }

    bool get_carry_flag() {
        return get_bit(rp, 0);
    }

    void set_zero_flag(bool x) {
        set_bit(rp, 1, x);
    }

    bool get_zero_flag() {
        return get_bit(rp, 1);
    }

    void set_interrupt_disable_flag(bool x) {
        set_bit(rp, 2, x);
    }

    bool get_interrupt_disable_flag() {
        return get_bit(rp, 2);
    }

    void set_overflow_flag(bool x) {
        set_bit(rp, 6, x);
    }

    bool get_overflow_flag() {
        return get_bit(rp, 6);
    }

    void set_negative_flag(bool x) {
        set_bit(rp, 7, x);
    }

    bool get_negative_flag() {
        return get_bit(rp, 7);
    }

    void set_break_flag(bool x) {
        set_bit(rp, 4, x);
    }

    bool get_break_flag() {
        return get_bit(rp, 4);
    }
}

t_adr machine::get_program_counter() {
    return pc;
}

void machine::set_program_counter(t_adr adr) {
    pc = adr;
}

bool machine::load_program(const std::string& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input.good()) {
        return false;
    }
    input.ignore(4);
    char prg_sz;
    input.read(&prg_sz, 1);
    char chr_sz;
    input.read(&chr_sz, 1);
    input.ignore(10);
    if ((prg_sz != 1 and prg_sz != 2) or chr_sz != 1) {
        return false;
    }
    prg_rom.resize(prg_sz * 0x4000u);
    input.read(&prg_rom[0], prg_rom.size());
    gfx::load_pattern_table(input);
    pc = 0x8000;
    return true;
}

char machine::read_memory(t_adr adr) {
    return read_mem(adr);
}

void machine::print_info() {
    std::cout << "| a : "; print_hex(ra);
    std::cout << " | x : "; print_hex(rx);
    std::cout << " | y : "; print_hex(ry);
    std::cout << " | sp : "; print_hex(sp);
    std::cout << " | pc : "; print_hex(pc);
    std::cout << " | p : "; print_hex(rp);
    std::cout << " | sc : "; print_hex(step_count);
    std::cout << " |\n";
}

unsigned long machine::get_step_counter() {
    return step_count;
}

unsigned long machine::get_cycle_counter() {
    return cycle_count;
}

void machine::cycle() {
    if (cycle_count == 0) {
        if (not ready) {
            return;
        }
        step();
    }
    cycle_count--;
}

void machine::halt() {
    ready = false;
}

bool machine::is_halted() {
    return not ready;
}

void machine::resume() {
    ready = true;
}

void machine::set_nmi_flag(bool val) {
    nmi_flag = val;
}

void machine::init() {
    sp = 0xff;
    ra = 0x00;
    rx = 0x00;
    ry = 0x00;
    rp = 0x24;
    std::fill(memory.begin(), memory.end(), 0x00);
    nmi_flag = 0;
    irq_flag = 0;
    reset_flag = 0;
    step_count = 0;
    cycle_count = 0;
    ready = true;
}
