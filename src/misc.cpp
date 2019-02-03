#include <cstdio>
#include <iostream>

#include "misc.hpp"

bool get_bit(char x, unsigned n) {
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

unsigned get_last_bits(unsigned x, unsigned n) {
    return x & ((1u << n) - 1);
}

void flip_bit(char& x, unsigned n) {
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

unsigned bin_num(const std::vector<bool>& v) {
    unsigned res = 0;
    auto e = 1u;
    for (auto x : v) {
        res += x * e;
        e <<= 1;
    }
    return res;
}
