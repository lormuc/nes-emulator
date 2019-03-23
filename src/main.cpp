#include <iostream>
#include <chrono>
#include <string>
#include <cstdio>

#include "gfx.hpp"
#include "machine.hpp"
#include "misc.hpp"

int main(int argc, char** argv) {
    if (argc < 2 or argc >= 4) {
        std::cout << "invalid arguments\n";
        return 1;
    }
    unsigned long fps = 60;
    if (argc == 3) {
        fps = std::stoul(argv[2]);
    }

    machine::init();
    gfx::init();
    auto ret = machine::load_program(argv[1]);
    if (ret != success) {
        std::cout << "could not load file\n";
        return 1;
    }

    gfx::set_frames_per_second(fps);

    while (gfx::is_running()) {
        if (gfx::should_poll()) {
            gfx::poll();
        } else {
            gfx::cycle();
            gfx::cycle();
            gfx::cycle();
            machine::cycle();
        }
    }

    gfx::close();
}
