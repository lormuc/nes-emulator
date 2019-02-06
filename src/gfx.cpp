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

const auto transparent_pixel = char(0xff);
const auto scanline_length = 341u;
const auto scanline_count = 262u;

const auto sprite_width = 8u;
const auto sprite_height = 8u;

namespace {
    char read_mem(unsigned);
}

using t_sprite_data = std::array<char, sprite_width * sprite_height>;

class t_sprite {
    unsigned sx;
    unsigned sy;
    unsigned tile_index;
    unsigned attr;

public:
    unsigned get_y() {
        return sy;
    }

    void set_y(unsigned val) {
        sy = val;
    }

    unsigned get_x() {
        return sx;
    }

    void set_x(unsigned val) {
        sx = val;
    }

    unsigned get_tile_index() {
        return tile_index;
    }

    void set_tile_index(unsigned val) {
        tile_index = val;
    }

    void set_attr(unsigned val) {
        attr = val;
    }

    unsigned get_priority() {
        return get_bit(attr, 5);
    }

    bool contains(unsigned x, unsigned y) {
        bool cx = in_range(x, sx, sx + sprite_width);
        bool cy = in_range(y, sy, sy + sprite_height);
        return cx and cy;
    }

    char get_pixel(unsigned x, unsigned y) {
        if (get_bit(attr, 6)) {
            x = 7 - x;
        }
        if (get_bit(attr, 7)) {
            y = 7 - y;
        }
        auto pt_idx = tile_index;
        pt_idx *= 16;
        pt_idx += y;
        x = 7 - x;
        auto b0 = get_bit(read_mem(pt_idx), x);
        auto b1 = get_bit(read_mem(pt_idx + 8), x);
        auto b2 = get_bit(attr, 0);
        auto b3 = get_bit(attr, 1);
        auto color_idx = bin_num({b0, b1, b2, b3, 1});
        if (b0 == 0 and b1 == 0) {
            return transparent_pixel;
        }
        return read_mem(0x3f00u + color_idx);
    }
};

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
    char oam_address;

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

    std::array<t_sprite, 64> sprite_list;
    std::vector<t_sprite> line_sprite_list;
    std::vector<t_sprite_data> line_sprite_data_list;

    void gen_nmi() {
        if (in_vblank and nmi_output) {
            machine::set_nmi_flag();
        }
    }

    void write_mem(unsigned adr, char val) {
        //std::cout << "gfx write_mem "; print_hex(adr); std::cout << "\n";
        adr &= 0x3fffu;
        if (adr == 0x3f10 || adr == 0x3f14 || adr == 0x3f18 || adr == 0x3f1c) {
            adr -= 0x10;
        }
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
        if (adr == 0x3f10 || adr == 0x3f14 || adr == 0x3f18 || adr == 0x3f1c) {
            adr -= 0x10;
        }
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

    bool pixel_is_opaque(char pixel) {
        return pixel != transparent_pixel;
    }

    void evaluate_sprites(unsigned y) {
        line_sprite_list.clear();
        line_sprite_data_list.clear();
        for (auto& spr : sprite_list) {
            auto sy = spr.get_y();
            if (in_range(y, sy, sy + sprite_height)) {
                line_sprite_list.push_back(spr);
                t_sprite_data sd;
                for (auto i = 0u; i < sprite_height * sprite_width; i++) {
                    auto x = i % sprite_width;
                    auto y = i / sprite_width;
                    sd[i] = spr.get_pixel(x, y);
                }
                line_sprite_data_list.push_back(sd);
                if (line_sprite_list.size() >= 8) {
                    break;
                }
            }
        }
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

    case 0x2003:
        oam_address = val;
        break;

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

    oam_address = 0;

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

void gfx::oam_write(char val) {
    auto m = oam_address / 4;
    auto& spr = sprite_list[m];
    auto n = oam_address % 4;
    if (n == 0) {
        spr.set_y(val + 1);
    } else if (n == 1) {
        spr.set_tile_index(val);
    } else if (n == 2) {
        spr.set_attr(val);
    } else if (n == 3) {
        spr.set_x(val);
    }
    oam_address++;
}

void gfx::cycle() {
    if (not started and frame_idx == 2) {
        started = true;
        frame_idx = 0;
        sdl::begin_drawing();
    }
    if (ver_cnt < 240) {
        if (hor_cnt == 0) {
            evaluate_sprites(ver_cnt);
        } else if (hor_cnt < 257) {
            auto x = hor_cnt - 1;
            auto y = ver_cnt;

            auto spr_pixel = transparent_pixel;
            unsigned pixel_priority = 0;
            for (auto i = 0u; i < line_sprite_list.size(); i++) {
                auto& spr = line_sprite_list[i];
                auto& spr_dat = line_sprite_data_list[i];
                auto x0 = spr.get_x();
                auto y0 = spr.get_y();
                if (in_range(x, x0, x0 + sprite_width)) {
                    auto sx = x - x0;
                    auto sy = y - y0;
                    auto idx = sy * sprite_width + sx;
                    spr_pixel = spr_dat[idx];
                    pixel_priority = spr.get_priority();
                    if (pixel_is_opaque(spr_pixel)) {
                        break;
                    }
                }
            }

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
            auto background_pixel = read_mem(0x3f00u + color_idx);

            auto pixel = background_pixel;
            if (background_pixel == transparent_pixel) {
                pixel = spr_pixel;
            }
            if (pixel_is_opaque(spr_pixel) and pixel_priority == 0) {
                pixel = spr_pixel;
            }

            if ((x % 32) == 0 || (y % 32) == 0) {
                pixel = 0x14;
            }

            sdl::send_pixel(pixel);
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
