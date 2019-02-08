#include "misc.hpp"
#include "sdl.hpp"
#include "input.hpp"

namespace {
    unsigned cnt;
    bool prev_value;
}

char input::read() {
    char res = 0;
    const int key_map[] = {
        sdl::key_kp_7, sdl::key_kp_9, sdl::key_kp_2, sdl::key_kp_3,
        sdl::key_kp_8, sdl::key_kp_5, sdl::key_kp_4, sdl::key_kp_6
    };
    if (cnt < 8) {
        set_bit(res, 0, sdl::get_key(key_map[cnt]));
    }
    cnt++;
    if (cnt == 24) {
        cnt = 0;
    }
    return res;
}

void input::write(char val) {
    auto b = get_bit(val, 0);
    if (b == 0 and prev_value == 1) {
        cnt = 0;
    }
    prev_value = b;
}
