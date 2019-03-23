#pragma once

#include <vector>
#include <chrono>

class t_millisecond_timer {
    std::chrono::time_point<std::chrono::steady_clock> t0;
public:
    void reset() {
        t0 = std::chrono::steady_clock::now();
    }
    long long int get_ticks() {
        auto t1 = std::chrono::steady_clock::now();
        auto dt = t1 - t0;
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(dt);
        return dur.count();
    }
};

bool get_bit(unsigned, unsigned);
void set_bit(char&, unsigned, bool = 1);
void set_bit(unsigned&, unsigned, bool = 1);
void flip_bit(char&, unsigned);
void flip_bit(unsigned&, unsigned);
unsigned get_last_bits(unsigned, unsigned);
void set_octet(unsigned&, unsigned, char);
void print_hex(char);
void print_hex(unsigned);
void print_hex(unsigned long);
bool in_range(unsigned, unsigned, unsigned);
unsigned bin_num_le(const std::vector<bool>&);
char get_bits(char, unsigned, unsigned);
unsigned get_bits(unsigned, unsigned, unsigned);
void set_bits(char&, unsigned, unsigned, char);
void set_bits(unsigned&, unsigned, unsigned, bool = 1);
void copy_bits(unsigned&, unsigned, unsigned, unsigned, unsigned = 0);
char reverse(char);

const int success = 0;
const int failure = 1;
