#include <array>
#include <iostream>
#include <algorithm>
#include <string>
#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "misc.hpp"
#include "gfx.hpp"
#include "machine.hpp"
#include "sdl.hpp"

#define fprintf

const auto transparent_pixel = char(0xff);
const auto scanline_length = 341u;
const auto scanline_count = 262u;
const auto prerender_line = scanline_count - 1;

const auto spr_y_ofs = 0;
const auto spr_idx_ofs = 1;
const auto spr_atr_ofs = 2;
const auto spr_x_ofs = 3;
const auto spr_atr_flip_hor_bit = 6;
const auto spr_atr_flip_ver_bit = 7;

namespace {
    char read_mem(unsigned);
    char get_palette_entry(unsigned);
    char get_palette_attribute(unsigned);
    char spr_pat_get(unsigned, unsigned, unsigned);
}

namespace {
    FILE* log;

    void print_tile(unsigned idx) {
        for (auto i = 0u; i < 8; i++) {
            for (auto j = 0u; j < 8; j++) {
                auto m0 = spr_pat_get(idx, i, 0);
                auto m1 = spr_pat_get(idx, i, 1);
                auto mm = get_bit(m1, 7 - j) * 2u + get_bit(m0, 7 - j);
                if (mm == 0) {
                    fprintf(log, " ");
                } else {
                    fprintf(log, "%u", mm);
                }
            }
            fprintf(log, "\n");
        }
    }

    bool sprite_0_hit_delayed;
    bool sprite_0_hit;
    bool sprite_0_y_in_range;
    bool sprite_0_y_in_range_next;

    unsigned copy_cnt;
    std::array<char, 64 * 4> oam;
    std::array<char, 8 * 4> sec_oam;

    char read_sec_oam(unsigned i, unsigned j) {
        return sec_oam[4 * i + j];
    }

    std::array<char, 8> spr_bitmap_lo;
    std::array<char, 8> spr_bitmap_hi;
    std::array<char, 8> spr_atr;
    std::array<char, 8> spr_x;
    std::array<bool, 8> spr_active;
    unsigned oam_idx;
    unsigned sec_oam_idx;
    char oam_data;
    char tmp_spr_y;
    char tmp_spr_idx;

    bool started;

    bool in_vblank;
    unsigned long frame_idx;

    unsigned hor_cnt;
    unsigned ver_cnt;

    unsigned set_adr;
    char set_val;
    unsigned long set_delay;
    bool set_delay_active;

    char oam_adr;
    char control_reg;
    char data_read_buffer;

    std::array<char, 0x0800> memory;
    std::array<char, 0x2000> pattern_table;
    std::array<char, 0x20> palette;

    unsigned cur_adr;

    void set_v(unsigned x) {
        // fprintf(log, "(%03u, %03u) v :  %03x %02x %05x %05x\n",
        //         hor_cnt, ver_cnt,
        //         get_bits(x, 12, 3), get_bits(x, 10, 2),
        //         get_bits(x, 5, 5), get_bits(x, 0, 5));
        fprintf(log, "(%03u, %03u) v = $%04x\n", hor_cnt, ver_cnt, x);
        cur_adr = x;
    }

    unsigned get_v() {
        return cur_adr;
    }

    unsigned tmp_adr;
    unsigned fine_x_scroll;
    bool write_toggle;

    bool mirroring;

    char nt_byte;
    char at_byte;
    char tile_bitmap_low;
    char tile_bitmap_high;

    unsigned canonize_adr(unsigned adr) {
        return get_bits(adr, 0, 14);
    }

    std::array<unsigned, 4> bg_bits;

    bool show_background;
    bool show_sprites;

    unsigned get_sprite_priority(unsigned i) {
         return get_bit(spr_atr[i], 5);
    }

    void load_tile_data() {
        set_octet(bg_bits[0], 0, tile_bitmap_low);
        set_octet(bg_bits[1], 0, tile_bitmap_high);
        set_octet(bg_bits[2], 0, get_palette_attribute(0));
        set_octet(bg_bits[3], 0, get_palette_attribute(1));
    }

    void shift_tile_data() {
        for (auto& x : bg_bits) {
            x <<= 1;
        }
    }

    char fetch_bg_pal_idx(unsigned fxs) {
        auto res = 0u;
        for (auto i = 0u; i < 4; i++) {
            set_bit(res, i, get_bit(bg_bits[i], 15 - fxs));
        }
        return res;
    }

    bool get_nmi_output_flag() {
        return get_bit(control_reg, 7);
    }

    void gen_vblank_nmi() {
        if (in_vblank and get_nmi_output_flag()) {
            machine::set_nmi_flag();
        }
    }

    void write_mem(unsigned adr, char val) {
        adr = canonize_adr(adr);
        if (adr == 0x3f10 || adr == 0x3f14 || adr == 0x3f18 || adr == 0x3f1c) {
            adr -= 0x10;
        }
        if (adr < 0x2000u) {
            pattern_table[adr] = val;
        } else if (adr < 0x3effu) {
            if (adr >= 0x3000u) {
                adr -= 0x1000u;
            }
            adr -= 0x2000u;
            if (mirroring == 0) {
                if (in_range(adr, 0x400, 0x800)) {
                    adr -= 0x400;
                } else if (in_range(adr, 0x800, 0xc00)) {
                    adr -= 0x400;
                } else if (in_range(adr, 0xc00, 0x1000)) {
                    adr -= 0x800;
                }
            } else {
                if (in_range(adr, 0x800, 0xc00)) {
                    adr -= 0x800;
                } else if (in_range(adr, 0xc00, 0x1000)) {
                    adr -= 0x800;
                }
            }
            memory[adr] = val;
        } else {
            char idx = get_last_bits(adr, 5);
            palette[idx] = val;
            if (get_last_bits(idx, 2) == 0) {
                flip_bit(idx, 4);
                palette[idx] = val;
            }
        }
    }

    char read_mem(unsigned adr) {
        canonize_adr(adr);
        if (adr == 0x3f10 || adr == 0x3f14 || adr == 0x3f18 || adr == 0x3f1c) {
            adr -= 0x10;
        }
        char res = 0;
        bool bad = false;
        if (adr < 0x2000u) {
            res = pattern_table[adr];
        } else if (adr < 0x3effu) {
            if (adr >= 0x3000u) {
                adr -= 0x1000u;
            }
            adr -= 0x2000u;
            if (mirroring == 0) {
                if (in_range(adr, 0x400, 0x800)) {
                    adr -= 0x400;
                } else if (in_range(adr, 0x800, 0xc00)) {
                    adr -= 0x400;
                } else if (in_range(adr, 0xc00, 0x1000)) {
                    adr -= 0x800;
                }
            } else {
                if (in_range(adr, 0x800, 0xc00)) {
                    adr -= 0x800;
                } else if (in_range(adr, 0xc00, 0x1000)) {
                    adr -= 0x800;
                }
            }
            res = memory[adr];
        } else {
            res = palette[get_last_bits(adr, 5)];
        }
        return res;
    }

    char get_palette_entry(unsigned idx) {
        return read_mem(0x3f00u + idx);
    }

    char get_bg_pattern_table_entry(unsigned idx) {
        if (get_bit(control_reg, 4)) {
            return read_mem(0x1000u + idx);
        } else {
            return read_mem(idx);
        }
    }

    char get_bg_pattern_table_entry(unsigned a, unsigned y, unsigned i) {
        a = 16 * a + y;
        if (i == 1) {
            a += 8;
        }
        auto res = get_bg_pattern_table_entry(a);
        return res;
    }

    char spr_pat_get(unsigned idx) {
        if (get_bit(control_reg, 3)) {
            return read_mem(0x1000u + idx);
        } else {
            return read_mem(idx);
        }
    }

    char spr_pat_get(unsigned a, unsigned y, unsigned i) {
        return spr_pat_get(16 * a + y + 8 * i);
    }

    unsigned get_tile_address(unsigned adr) {
        unsigned res = 0;
        copy_bits(res, 0, 12, adr);
        set_bit(res, 13);
        return res;
    }

    unsigned get_fine_y_scroll() {
        return get_bits(get_v(), 12, 3);
    }

    void set_fine_y_scroll(unsigned x) {
        auto y = get_v();
        copy_bits(y, 12, 3, x);
        set_v(y);
    }

    unsigned get_coarse_y_scroll() {
        return get_bits(get_v(), 5, 5);
    }

    unsigned get_coarse_x_scroll() {
        return get_bits(get_v(), 0, 5);
    }

    void set_coarse_y_scroll(unsigned x) {
        auto y = get_v();
        copy_bits(y, 5, 5, x);
        set_v(y);
    }

    unsigned get_attribute_address(unsigned adr) {
        auto res = 0u;
        copy_bits(res, 0, 3, adr, 2);
        copy_bits(res, 3, 3, adr, 7);
        set_bits(res, 6, 4);
        copy_bits(res, 10, 2, adr, 10);
        set_bit(res, 13);
        fprintf(log, "gaa res = %04x\n", res);
        return res;
    }

    void fetch_nametable_byte() {
        nt_byte = read_mem(get_tile_address(get_v()));
        fprintf(log, "fetch nt byte %02hhx\n", nt_byte);
    }

    void fetch_attribute_table_byte() {
        at_byte = read_mem(get_attribute_address(get_v()));
        fprintf(log, "fetchl at byte %02hhx\n", at_byte);
    }

    void fetch_tile_bitmap_low() {
        auto fy = get_fine_y_scroll();
        tile_bitmap_low = get_bg_pattern_table_entry(nt_byte, fy, 0);
        fprintf(log, "fetch tile bitmap low %02hhx\n", tile_bitmap_low);
    }

    void fetch_tile_bitmap_high() {
        auto fy = get_fine_y_scroll();
        tile_bitmap_high = get_bg_pattern_table_entry(nt_byte, fy, 1);
        fprintf(log, "fetch tile bitmap high %02hhx\n", tile_bitmap_high);
    }

    void inc_hor_scroll() {
        auto v = get_v();
        if (get_bits(v, 0, 5) == 0x1fu) {
            set_bits(v, 0, 5, 0);
            flip_bit(v, 10);
        } else {
            v++;
        }
        set_v(v);
    }

    void inc_ver_scroll() {
        auto fine_y = get_fine_y_scroll();
        if (fine_y < 7) {
            set_fine_y_scroll(fine_y + 1);
        } else {
            set_fine_y_scroll(0);
            auto y = get_coarse_y_scroll();
            if (y == 29) {
                y = 0;
                auto v = get_v();
                flip_bit(v, 11);
                set_v(v);
            } else if (y == 31) {
                y = 0;
            } else {
                y++;
            }
            set_coarse_y_scroll(y);
        }
    }

    void reset_hor_scroll() {
        auto v = get_v();
        copy_bits(v, 0, 5, tmp_adr, 0);
        set_bit(v, 10, get_bit(tmp_adr, 10));
        set_v(v);
    }

    void reset_ver_scroll() {
        auto v = get_v();
        copy_bits(v, 5, 5, tmp_adr, 5);
        copy_bits(v, 11, 4, tmp_adr, 11);
        set_v(v);
    }

    char get_palette_attribute(unsigned j) {
        auto xs = (get_coarse_x_scroll() * 8) % 32;
        xs /= 16;
        auto ys = (get_coarse_y_scroll() * 8 + get_fine_y_scroll()) % 32;
        ys /= 16;
        auto i = xs + ys * 2;
        fprintf(log, "get_bit at_byte %u\n", 2 * i + j);
        if (get_bit(at_byte, 2 * i + j)) {
            return 0xff;
        } else {
            return 0x00;
        }
    }

    char background_fetch_pixel() {
        auto res = fetch_bg_pal_idx(fine_x_scroll);
        if (res % 4 == 0) {
            return transparent_pixel;
        } else {
            return get_palette_entry(res);
        }
    }

    void render_pixel() {
        auto background_pixel = background_fetch_pixel();

        auto spr_pixel = transparent_pixel;
        auto spr_priority = 0;
        for (auto i = 0u; i < 8u; i++) {
            if (spr_active[i]) {
                fprintf(log, "spr active %u\n", i);
                fprintf(log, "spr bitmap lo %02hhx\n", spr_bitmap_lo[i]);
                fprintf(log, "spr bitmap hi %02hhx\n", spr_bitmap_hi[i]);
                fprintf(log, "spr atr       %02hhx\n", spr_atr[i]);
                auto b0 = get_bit(spr_bitmap_lo[i], 7);
                auto b1 = get_bit(spr_bitmap_hi[i], 7);
                auto b2 = get_bit(spr_atr[i], 0);
                auto b3 = get_bit(spr_atr[i], 1);
                if (not (b0 == 0 and b1 == 0)) {
                    if (background_pixel != transparent_pixel) {
                        if (i == 0 and sprite_0_y_in_range) {
                            if (show_background and show_sprites) {
                                sprite_0_hit = true;
                            }
                        }
                    }
                    spr_pixel = get_palette_entry(bin_num_le({b0, b1, b2, b3, 1}));
                    fprintf(log, "pix $%02hhx\n", spr_pixel);
                    spr_priority = get_sprite_priority(i);
                    break;
                }
            }
        }

        auto pixel = background_pixel;
        if (pixel == transparent_pixel) {
            pixel = spr_pixel;
        }
        if (spr_priority == 0 and spr_pixel != transparent_pixel) {
            pixel = spr_pixel;
        }
        if (pixel == transparent_pixel) {
            pixel = get_palette_entry(0);
        }
        sdl::send_pixel(pixel);
        if (hor_cnt == 256 and ver_cnt == 239) {
            sdl::render();
        }
    }

    void sprite_evaluation_step() {
        if (hor_cnt == 65) {
            oam_idx = 0;
            sec_oam_idx = 0;
            copy_cnt = 0;
        }
        if (oam_idx < oam.size()) {
            if (hor_cnt % 2 == 0) {
                if (sec_oam_idx < sec_oam.size()) {
                    sec_oam[sec_oam_idx] = oam_data;
                    auto inr = in_range(ver_cnt, oam_data, oam_data + 8);
                    // fprintf(log, "in_range %u %u %u ?\n", y, oam_data, oam_data + 8);
                    if (oam_idx == 0) {
                        sprite_0_y_in_range_next = inr;
                    }
                    if (copy_cnt > 0 or inr) {
                        // fprintf(log, "%03u : %03u match\n", ver_cnt, oam_idx / 4);
                        sec_oam_idx++;
                        // fprintf(log, "sec_idx = %02u\n", sec_oam_idx);
                        oam_idx++;
                        copy_cnt++;
                        if (copy_cnt == 4) {
                            copy_cnt = 0;
                        }
                    } else {
                        oam_idx += 4;
                    }
                } else {
                    oam_idx += 4;
                }
            } else {
                oam_data = oam[oam_idx];
            }
        }
        if (hor_cnt == 256) {
            fprintf(log, "sec\n");
            auto i = 0u;
            while (i < 32) {
                for (auto j = 0u; j < 4; j++) {
                    for (auto k = 0u; k < 4; k++) {
                        fprintf(log, " %02hhx ", sec_oam[i]);
                        i++;
                    }
                    fprintf(log, " | ");
                }
                fprintf(log, "\n");
            }
        }
    }

    void sprite_fetches_step() {
        auto q = (hor_cnt - 257) / 8;
        auto r = (hor_cnt - 257) % 8;
        char t;
        unsigned yy;
        switch (r) {
        case 0:
            spr_active[q] = false;
            tmp_spr_y = read_sec_oam(q, spr_y_ofs);
            break;
        case 1:
            tmp_spr_idx = read_sec_oam(q, spr_idx_ofs);
            // fprintf(log, "tile # %u\n", tmp_spr_idx);
            // print_tile(tmp_spr_idx);
            break;
        case 2:
            spr_atr[q] = read_sec_oam(q, spr_atr_ofs);
            break;
        case 3:
            spr_x[q] = read_sec_oam(q, spr_x_ofs);
            if (spr_x[q] == 0) {
                spr_active[q] = true;
            }
            break;
        case 5:
            yy = ver_cnt - tmp_spr_y;
            if (get_bit(spr_atr[q], spr_atr_flip_ver_bit)) {
                yy = 7 - yy;
            }
            t = spr_pat_get(tmp_spr_idx, yy, 0);
            if (get_bit(spr_atr[q], spr_atr_flip_hor_bit)) {
                t = reverse(t);
            }
            spr_bitmap_lo[q] = t;
            break;
        case 7:
            yy = ver_cnt - tmp_spr_y;
            if (get_bit(spr_atr[q], spr_atr_flip_ver_bit)) {
                yy = 7 - yy;
            }
            t = spr_pat_get(tmp_spr_idx, yy, 1);
            if (get_bit(spr_atr[q], spr_atr_flip_hor_bit)) {
                t = reverse(t);
            }
            spr_bitmap_hi[q] = t;
            break;
        }
        if (hor_cnt == 320) {
            fprintf(log, "spr_x : ");
            for (auto i = 0u; i < 8; i++) {
                fprintf(log, " %02hhx", spr_x[i]);
            }
            fprintf(log, "\n");
        }
    }

    void inc_v() {
        if (get_bit(control_reg, 2)) {
            set_v(get_v() + 32);
        } else {
            set_v(get_v() + 1);
        }
    }

    void delayed_set() {
        if (set_delay_active) {
            if (set_delay == 0) {
                gfx::set(set_adr, set_val);
                set_delay_active = false;
            } else {
                set_delay--;
            }
        }
    }
}

void gfx::load_pattern_table(std::ifstream& ifs) {
    ifs.read(&pattern_table[0], pattern_table.size());
}

void gfx::set_with_delay(unsigned adr, char val) {
    set_adr = adr;
    set_val = val;
    set_delay = 3 * machine::get_cycle_counter() - 2;
    set_delay_active = true;
}

void gfx::set(unsigned adr, char val) {
    fprintf(log, "set $%04x $%02hhx\n", adr, val);

    switch (adr) {

    case 0x2000:
        if (get_bit(val, 7) and not get_bit(control_reg, 7)) {
            gen_vblank_nmi();
        }
        control_reg = val;
        copy_bits(tmp_adr, 10, 2, val, 0);
        break;

    case 0x2001:
        show_background = get_bit(val, 3);
        show_sprites = get_bit(val, 4);
        break;

    case 0x2003:
        oam_adr = val;
        break;

    case 0x2004:
        oam_write(val);
        break;

    case 0x2005:
        if (write_toggle == 0) {
            copy_bits(tmp_adr, 0, 5, val, 3);
            fine_x_scroll = get_bits(val, 0, 3);
            write_toggle = 1;
        } else {
            copy_bits(tmp_adr, 12, 3, val, 0);
            copy_bits(tmp_adr, 5, 5, val, 3);
            write_toggle = 0;
        }
        break;

    case 0x2006:
        if (write_toggle == 0) {
            copy_bits(tmp_adr, 8, 6, val, 0);
            set_bit(tmp_adr, 14, 0);
            write_toggle = 1;
        } else {
            copy_bits(tmp_adr, 0, 8, val, 0);
            set_v(tmp_adr);
            write_toggle = 0;
        }
        break;

    case 0x2007:
        write_mem(get_v(), val);
        inc_v();
        break;

    }
}

char gfx::get(unsigned adr) {
    char res = 0;

    switch (adr) {

    case 0x2002:
        set_bit(res, 6, sprite_0_hit_delayed);
        set_bit(res, 7, in_vblank);
        in_vblank = false;
        write_toggle = 0;
        break;

    case 0x2007:
        if (canonize_adr(adr) < 0x3f00u) {
            res = data_read_buffer;
            data_read_buffer = read_mem(get_v());
        } else {
            res = read_mem(get_v());
        }
        inc_v();
        break;

    }

    return res;
}

int gfx::init() {
    sprite_0_hit_delayed = false;
    sprite_0_hit = false;
    sprite_0_y_in_range = false;
    sprite_0_y_in_range_next = false;

    started = false;

    hor_cnt = 0;
    ver_cnt = prerender_line;

    in_vblank = false;
    frame_idx = 0;

    set_delay_active = false;

    oam_adr = 0;
    control_reg = 0;

    mirroring = 0;

    auto ret = sdl::init();

    show_background = 0;
    show_sprites = 0;

    log = std::fopen("ppu_log.txt", "w");
    if (log == nullptr) {
        std::perror("ppu log opening failed ");
        return failure;
    }

    return ret;
}

void gfx::close() {
    std::fclose(log);
    sdl::close();
}

void gfx::poll() {
    sdl::poll();
}

bool gfx::is_running() {
    return sdl::is_running();
}

bool gfx::should_poll() {
    return sdl::should_poll();
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
    oam[oam_adr] = val;
    oam_adr++;
}

void gfx::set_mirroring(bool val) {
    mirroring = val;
}

void gfx::cycle() {
    if (not started and frame_idx == 2) {
        started = true;
        sdl::start();
        frame_idx = 0;
    }

    if (sprite_0_hit) {
        sprite_0_hit_delayed = true;
    }

    auto visible_line = ver_cnt < 240;

    if (hor_cnt == 0 and visible_line) {
        // fprintf(log, "palette\n");
        // for (auto j = 0u; j < 2; j++) {
        //     for (auto i = 0u; i < 16; i++) {
        //         fprintf(log, " $%02hhx", get_palette_entry(16 * j + i));
        //     }
        //     fprintf(log, "\n");
        // }
        sprite_0_y_in_range = sprite_0_y_in_range_next;
    }

    if (hor_cnt == 0 and ver_cnt == 0 and started) {
        fprintf(log, "tmp_adr == $%04x\n", tmp_adr);
        fprintf(log, "nt info\n");
        for (auto i = 0u; i < 30; i++) {
            for (auto j = 0u; j < 32; j++) {
                fprintf(log, " %02hhx", read_mem(0x2000u + i * 32 + j));
            }
            fprintf(log, "\n");
        }

        // fprintf(log, "at table\n");
        // for (auto i = 0u; i < 8; i++) {
        //     for (auto j = 0u; j < 8; j++) {
        //         fprintf(log, " %02hhx", read_mem(0x23c0u + i * 8 + j));
        //     }
        //     fprintf(log, "\n");
        // }

        fprintf(log, "oam\n");
        auto i = 0u;
        while (i < 256) {
            for (auto j = 0u; j < 4; j++) {
                for (auto k = 0u; k < 4; k++) {
                    fprintf(log, " %02hhx ", oam[i]);
                    i++;
                }
                fprintf(log, " | ");
            }
            fprintf(log, "\n");
        }
    }

    if (ver_cnt < 240 and in_range(hor_cnt, 1, 257)) {
        render_pixel();
    }

    if (show_background or show_sprites) {
        if (visible_line) {
            if (hor_cnt > 0) {
                if (hor_cnt < 65) {
                    if (hor_cnt % 2 == 0) {
                        auto si = (hor_cnt - 1) / 2;
                        sec_oam[si] = 0xff;
                    }
                } else if (hor_cnt < 257) {
                    sprite_evaluation_step();
                } else if (hor_cnt < 321) {
                    sprite_fetches_step();
                }
            }
        }
    }

    if (show_sprites) {
        if (ver_cnt < 240 and in_range(hor_cnt, 1, 257)) {
            for (auto i = 0u; i < spr_active.size(); i++) {
                if (spr_active[i]) {
                    spr_bitmap_lo[i] <<= 1;
                    spr_bitmap_hi[i] <<= 1;
                }
            }
            for (auto i = 0u; i < 8u; i++) {
                if (spr_x[i] > 0) {
                    spr_x[i]--;
                    // fprintf(log, "spr_x[%u] = %02hhx\n", i, spr_x[i]);
                    if (spr_x[i] == 0) {
                        spr_active[i] = true;
                    }
                }
            }
        }
    }

    if (show_background) {
        if ((ver_cnt < 240 or ver_cnt == prerender_line) and hor_cnt > 0) {
            if (hor_cnt < 257 or in_range(hor_cnt, 321, 337)) {
                shift_tile_data();
                auto m = (hor_cnt - 1) % 8;
                if (m == 0) {
                    fetch_nametable_byte();
                } else if (m == 2) {
                    fetch_attribute_table_byte();
                } else if (m == 4) {
                    fetch_tile_bitmap_low();
                } else if (m == 6) {
                    fetch_tile_bitmap_high();
                } else if (m == 7) {
                    load_tile_data();
                    inc_hor_scroll();
                }
                if (hor_cnt == 256) {
                    inc_ver_scroll();
                }
            } else if (hor_cnt == 257) {
                reset_hor_scroll();
            }
        }
    }

    if (ver_cnt == 241 and hor_cnt == 1) {
        in_vblank = true;
        gen_vblank_nmi();
    }

    if (ver_cnt == prerender_line) {
        if (hor_cnt == 1) {
            sprite_0_hit_delayed = false;
            sprite_0_hit = false;
            in_vblank = false;
        }
        if (in_range(hor_cnt, 280, 305)) {
            if (show_background) {
                reset_ver_scroll();
            }
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
        if (ver_cnt < 240 or ver_cnt == prerender_line) {
            fprintf(log, "y = %u\n", ver_cnt);
        }
        if (ver_cnt == scanline_count) {
            ver_cnt = 0;
            frame_idx++;
            fprintf(log, "kadr nomer %lu\n", frame_idx);
            in_vblank = false;
        }
    }

    delayed_set();
}
