#pragma once

#include <array>
#include <vector>

using t_adr = unsigned long;

namespace machine {
    void init();
    void set_program_counter(t_adr);
    t_adr get_program_counter();
    unsigned long get_step_counter();
    unsigned long get_cycle_counter();
    void print_info();
    char read_memory(t_adr);
    int load_program(const std::string&);
    void reset();
    void cycle();
    void halt();
    bool is_halted();
    void resume();
    void set_nmi_flag(bool = true);
}
