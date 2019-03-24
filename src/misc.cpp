#include <cstdio>
#include <iostream>
#include <cstdarg>

#include "misc.hpp"

const auto debug_mode = true;

void debug_print(FILE* fp, const char* fmt, ...) {
    if (not debug_mode) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
}

bool get_bit(unsigned x, unsigned n) {
    return x & (1u << n);
}

void set_bit(char& x, unsigned n, bool v) {
    if (v) {
        x |= (1u << n);
    } else {
        x &= ~(1u << n);
    }
}

void set_bit(unsigned& x, unsigned n, bool v) {
    if (v) {
        x |= (1u << n);
    } else {
        x &= ~(1u << n);
    }
}

void flip_bit(char& x, unsigned n) {
    x ^= (1u << n);
}

void flip_bit(unsigned& x, unsigned n) {
    x ^= (1u << n);
}

void set_octet(unsigned& x, unsigned n, char v) {
    n *= 8;
    x &= ~(0xffu << n);
    x |= (v << n);
}

void print_hex(char x) {
    std::cout.flush(); printf("$%02x", x); fflush(stdout);
}

void print_hex(unsigned x) {
    std::cout.flush(); printf("$%04x", x); fflush(stdout);
}

void print_hex(unsigned long x) {
    std::cout.flush(); printf("$%04lx", x); fflush(stdout);
}

bool in_range(unsigned x, unsigned a, unsigned b) {
    return x >= a and x < b;
}

unsigned bin_num_le(const std::vector<bool>& v) {
    auto res = 0u;
    auto e = 1u;
    for (auto x : v) {
        res += x * e;
        e <<= 1;
    }
    return res;
}

char get_bits(char x, unsigned m, unsigned n) {
    return (x >> m) & ((1u << n) - 1u);
}

unsigned get_bits(unsigned x, unsigned m, unsigned n) {
    return (x >> m) & ((1u << n) - 1u);
}

void set_bits(char& x, unsigned m, unsigned n, char y) {
    x &= ~(((1u << n) - 1u) << m);
    y &= ((1u << n) - 1u);
    y <<= m;
    x |= y;
}

unsigned make_range_mask(unsigned m, unsigned n) {
    unsigned y;
    if (n >= sizeof(unsigned) * 8) {
        y = 0;
    } else {
        y = (1u << n);
    }
    return (y - 1) << m;
}

unsigned get_last_bits(unsigned x, unsigned n) {
    return x & make_range_mask(0, n);
}

void clear_bits(unsigned& x, unsigned m, unsigned n) {
    x &= ~make_range_mask(m, n);
}

void copy_bits(unsigned& x, unsigned m, unsigned n, unsigned y, unsigned k) {
    y >>= k;
    y = get_last_bits(y, n);
    y <<= m;
    clear_bits(x, m, n);
    x |= y;
}

void set_bits(unsigned& x, unsigned m, unsigned n, bool v) {
    if (v) {
        copy_bits(x, m, n, unsigned(-1));
    } else {
        copy_bits(x, m, n, unsigned(0));
    }
}

char reverse(char b) {
    b = (b & 0xf0) >> 4 | (b & 0x0f) << 4;
    b = (b & 0xcc) >> 2 | (b & 0x33) << 2;
    b = (b & 0xaa) >> 1 | (b & 0x55) << 1;
    return b;
}
