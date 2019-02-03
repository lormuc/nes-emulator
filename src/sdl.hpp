#pragma once

namespace sdl {
    bool init();
    bool is_running();
    bool is_waiting();
    void render();
    void poll();
    void begin_drawing();
    void send_pixel(char);
    void set_frames_per_second(unsigned);
    void close();

    bool get_key(int);
    extern const int key_right;
    extern const int key_left;
    extern const int key_down;
    extern const int key_up;
    extern const int key_left_trigger;
    extern const int key_right_trigger;
}
