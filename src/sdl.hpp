#pragma once

namespace sdl {
    int init();
    bool is_running();
    bool is_waiting();
    void render();
    void poll();
    void begin_drawing();
    void send_pixel(char);
    void set_frames_per_second(unsigned);
    void close();

    void debug_render();
    void debug_send_pixel(char);

    bool get_key(int);
    extern const int key_kp_1;
    extern const int key_kp_2;
    extern const int key_kp_3;
    extern const int key_kp_4;
    extern const int key_kp_5;
    extern const int key_kp_6;
    extern const int key_kp_7;
    extern const int key_kp_8;
    extern const int key_kp_9;
}
