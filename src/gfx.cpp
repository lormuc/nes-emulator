#include <array>
#include <iostream>
#include <algorithm>
#include <string>
#include <chrono>
#include <cstdio>

#include "misc.hpp"
#include "gfx.hpp"
#include "machine.hpp"
#include "sdl.hpp"

const auto not_a_color = char(0xff);
const auto scanline_length = 341u;
const auto scanline_count = 262u;

namespace {
    bool started;

    bool in_vblank;
    bool nmi_output;
    unsigned long frame_idx;

    unsigned hor_cnt;
    unsigned ver_cnt;

    unsigned set_adr;
    char set_val;
    unsigned long set_delay;
    bool set_delay_active;

    unsigned address;

    class {
        bool latch;
    public:
        bool get_value() { return latch; }
        void flip() { latch ^= 1; }
        void reset() { latch = 1; }
    } address_latch;

    std::array<char, 0x0800> memory;
    std::vector<char> pattern_table;
    std::array<char, 0x20> palette;

    void gen_nmi() {
        if (in_vblank and nmi_output) {
            machine::set_nmi_flag();
        }
    }

    void write_mem(unsigned adr, char val) {
        //std::cout << "gfx write_mem "; print_hex(adr); std::cout << "\n";
        adr &= 0x3fffu;
        bool bad = false;
        if (adr < 0x2000u) {
            bad = true;
        } else if (adr < 0x3000u) {
            adr -= 0x2000u;
            set_bit(adr, 10, 0);
            if (adr >= 0x0800u) {
                adr -= 0x0400u;
            }
            memory[adr] = val;
        } else if (adr < 0x3effu) {
            bad = true;
        } else {
            char idx = get_last_bits(adr, 5);
            palette[idx] = val;
            if (get_last_bits(idx, 2) == 0) {
                flip_bit(idx, 4);
                palette[idx] = val;
            }
        }
        if (bad) {
            std::cout << "gfx bad write_mem "; print_hex(adr); std::cout << "\n";
        }
    }

    char read_mem(unsigned adr) {
        adr &= 0x3fffu;
        char res = 0;
        bool bad = false;
        if (adr < 0x2000u) {
            res = pattern_table[adr];
        } else if (adr < 0x3000u) {
            adr -= 0x2000u;
            set_bit(adr, 10, 0);
            if (adr >= 0x0800u) {
                adr -= 0x0400u;
            }
            res = memory[adr];
        } else if (adr < 0x3effu) {
            bad = true;
        } else {
            res = palette[get_last_bits(adr, 5)];
        }
        if (bad) {
            std::cout << "gfx bad read_mem "; print_hex(adr); std::cout << "\n";
        }
        return res;
    }
}

void gfx::load_pattern_table(std::ifstream& ifs) {
    pattern_table.resize(0x2000);
    ifs.read(&pattern_table[0], pattern_table.size());
}

void gfx::set_with_delay(unsigned adr, char val) {
    set_adr = adr;
    set_val = val;
    set_delay = 3 * machine::get_cycle_counter() - 2;
    set_delay_active = true;
}

void gfx::set(unsigned adr, char val) {
    switch (adr) {

    case 0x2006:
        set_octet(address, address_latch.get_value(), val);
        address_latch.flip();
        break;

    case 0x2007:
        write_mem(address, val);
        address++;
        break;

    }
}

char gfx::get(unsigned adr) {
    char res = 0;

    switch (adr) {

    case 0x2002:
        set_bit(res, 7, in_vblank);
        in_vblank = false;
        address_latch.reset();
        break;

    }

    return res;
}

bool gfx::init() {
    started = false;

    hor_cnt = 0;
    ver_cnt = 0;

    in_vblank = false;
    nmi_output = true;
    frame_idx = 0;

    set_delay_active = false;

    auto success = sdl::init();

    return success;
}

void gfx::close() {
    sdl::close();
}

void gfx::poll() {
    sdl::poll();
}

bool gfx::is_running() {
    return sdl::is_running();
}

bool gfx::is_waiting() {
    return sdl::is_waiting();
}

void gfx::set_frames_per_second(unsigned val) {
    sdl::set_frames_per_second(val);
}

void gfx::print_info() {
    std::cout.flush();
    printf("h %3u  v %3u\n", hor_cnt, ver_cnt);
    std::fflush(stdout);
}

void gfx::cycle() {
    if (not started and frame_idx == 2) {
        started = true;
        frame_idx = 0;
        sdl::begin_drawing();
    }
    if (ver_cnt < 240) {
        if (in_range(hor_cnt, 1, 257)) {
            auto x = hor_cnt - 1;
            auto y = ver_cnt;
            auto th = x / 8;
            auto bh = x % 8;
            auto tv = y / 8;
            auto bv = y % 8;
            auto nm_idx = tv * 32 + th;
            unsigned pt_idx = read_mem(0x2000u + nm_idx);
            pt_idx *= 16;
            pt_idx += bv;
            bh = 7 - bh;
            auto b0 = get_bit(read_mem(pt_idx), bh);
            auto b1 = get_bit(read_mem(pt_idx + 8), bh);

            auto ax = x / 32;
            auto abx = x % 32;
            auto ay = y / 32;
            auto aby = y % 32;
            auto at = read_mem(0x23c0u + ay * 8 + ax);
            abx /= 16;
            aby /= 16;
            auto bb = (abx + aby * 2) * 2;
            auto b2 = get_bit(at, bb);
            auto b3 = get_bit(at, bb + 1);

            auto color_idx = bin_num({b0, b1, b2, b3});
            if (color_idx == 4 || color_idx == 8 || color_idx == 12) {
                color_idx = 0;
            }
            auto color = read_mem(0x3f00u + color_idx);

            if ((x % 32) == 0 || (y % 32) == 0) {
                color = 0x14;
            }

            sdl::send_pixel(color);
            if (hor_cnt == 256 and ver_cnt == 239) {
                sdl::render();
            }
        }
    }

    if (ver_cnt == 241) {
        if (hor_cnt == 1) {
            in_vblank = true;
            gen_nmi();
        }
    }

    if ((frame_idx % 2) == 1 and ver_cnt == 261 and hor_cnt == 339) {
        hor_cnt += 2;
    } else {
        hor_cnt++;
    }
    if (hor_cnt == scanline_length) {
        hor_cnt = 0;
        ver_cnt++;
        if (ver_cnt == scanline_count) {
            ver_cnt = 0;
            frame_idx++;
            in_vblank = false;
        }
    }
    if (set_delay_active) {
        if (set_delay == 0) {
            set(set_adr, set_val);
            set_delay_active = false;
        } else {
            set_delay--;
        }
    }
}
