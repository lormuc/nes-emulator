#include <iostream>
#include <chrono>
#include <string>
#include <cstdio>

#include "gfx.hpp"
#include "machine.hpp"

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
    auto success = machine::load_program(argv[1]);
    if (not success) {
        std::cout << "could not load file\n";
        return 1;
    }

    gfx::init();
    gfx::set_frames_per_second(fps);

    while (gfx::is_running()) {
        if (gfx::is_waiting()) {
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
