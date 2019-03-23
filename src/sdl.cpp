#include <array>
#include <iostream>
#include <cstdio>

#include <SDL2/SDL.h>

#include "misc.hpp"
#include "sdl.hpp"

const auto in_scr_width = 256u;
const auto in_scr_height = 240u;

const auto overscan_top = 8u;
const auto overscan_bot = 8u;

const auto out_scr_width = in_scr_width * 1;
const auto out_scr_height = (in_scr_height - (overscan_top + overscan_bot)) * 1;

const int sdl::key_kp_1 = SDL_SCANCODE_KP_1;
const int sdl::key_kp_2 = SDL_SCANCODE_KP_2;
const int sdl::key_kp_3 = SDL_SCANCODE_KP_3;
const int sdl::key_kp_4 = SDL_SCANCODE_KP_4;
const int sdl::key_kp_5 = SDL_SCANCODE_KP_5;
const int sdl::key_kp_6 = SDL_SCANCODE_KP_6;
const int sdl::key_kp_7 = SDL_SCANCODE_KP_7;
const int sdl::key_kp_8 = SDL_SCANCODE_KP_8;
const int sdl::key_kp_9 = SDL_SCANCODE_KP_9;

std::array<bool, 1024> keyboard_state;

const char palette[][3] = {
    {0x75, 0x75, 0x75},
    {0x27, 0x1b, 0x8f},
    {0x00, 0x00, 0xab},
    {0x47, 0x00, 0x9f},
    {0x8f, 0x00, 0x77},
    {0xab, 0x00, 0x13},
    {0xa7, 0x00, 0x00},
    {0x7f, 0x0b, 0x00},
    {0x43, 0x2f, 0x00},
    {0x00, 0x47, 0x00},
    {0x00, 0x51, 0x00},
    {0x00, 0x3f, 0x17},
    {0x1b, 0x3f, 0x5f},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0xbc, 0xbc, 0xbc},
    {0x00, 0x73, 0xef},
    {0x23, 0x3b, 0xef},
    {0x83, 0x00, 0xf3},
    {0xbf, 0x00, 0xbf},
    {0xe7, 0x00, 0x5b},
    {0xdb, 0x2b, 0x00},
    {0xcb, 0x4f, 0x0f},
    {0x8b, 0x73, 0x00},
    {0x00, 0x97, 0x00},
    {0x00, 0xab, 0x00},
    {0xbe, 0x93, 0x3b},
    {0x00, 0x83, 0x8b},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0xff, 0xff, 0xff},
    {0x3b, 0xbf, 0xff},
    {0x5f, 0x97, 0xff},
    {0xa7, 0x8b, 0xfd},
    {0xf7, 0x7b, 0xff},
    {0xff, 0x77, 0xb7},
    {0xff, 0x77, 0x63},
    {0xff, 0x9b, 0x3b},
    {0xf3, 0xbf, 0x3f},
    {0x83, 0xd3, 0x13},
    {0x4f, 0xdf, 0x4b},
    {0x58, 0xf8, 0x98},
    {0x00, 0xeb, 0xdb},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0xff, 0xff, 0xff},
    {0xab, 0xe7, 0xff},
    {0xc7, 0xd7, 0xff},
    {0xd7, 0xcb, 0xff},
    {0xff, 0xc7, 0xff},
    {0xff, 0xc7, 0xdb},
    {0xff, 0xbf, 0xb3},
    {0xff, 0xdb, 0xab},
    {0xff, 0xe7, 0xa3},
    {0xe3, 0xff, 0xa3},
    {0xab, 0xf3, 0xbf},
    {0xb3, 0xff, 0xcf},
    {0x9f, 0xff, 0xf3},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00}
};

namespace {
    SDL_Window* window;
    SDL_Renderer* renderer;

    std::array<char, in_scr_width * in_scr_height> screen;
    unsigned scr_idx;
    long frame_idx;
    t_millisecond_timer timer;
    bool has_started;
    bool running;
    bool frame_done;
    unsigned frames_per_second;
    bool has_polled_after_rendering;
}

void sdl::render() {
    if (not has_started) {
        return;
    }

    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderClear(renderer);
    for (auto i = overscan_top; i < in_scr_height - overscan_bot; i++) {
        for (auto j = 0u; j < in_scr_width; j++) {
            auto idx = i * in_scr_width + j;
            auto rgb = &palette[screen[idx]][0];
            SDL_SetRenderDrawColor(renderer, rgb[0], rgb[1], rgb[2], 0xff);
            auto ih = in_scr_height - overscan_top - overscan_bot;
            auto rx = int(out_scr_width * j / in_scr_width);
            auto ry = int(out_scr_height * (i - overscan_top) / ih);
            auto rw = int(out_scr_width / in_scr_width);
            auto rh = int(out_scr_height / ih);
            SDL_Rect rect = { rx, ry, rw, rh };
            SDL_RenderFillRect(renderer, &rect);
        }
    }
    SDL_RenderPresent(renderer);

    char buf[0x10];
    std::snprintf(buf, 0x10, "%05ld", frame_idx);
    SDL_SetWindowTitle(window, buf);

    scr_idx = 0;
    frame_done = true;
    frame_idx++;
    has_polled_after_rendering = false;
}

void sdl::send_pixel(char color) {
    if (not has_started) {
        return;
    }
    if (scr_idx < screen.size()) {
        screen[scr_idx] = color;
        scr_idx++;
    }
}

int sdl::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "sdl init fail : " << SDL_GetError() << "\n";
        return failure;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    auto wu = SDL_WINDOWPOS_UNDEFINED;
    auto sw = out_scr_width;
    auto sh = out_scr_height;

    window = SDL_CreateWindow("nes", wu, wu, sw, sh, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "sdl create window fail : " << SDL_GetError() << "\n";
        return failure;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        std::cerr << "sdl create renderer fail : " << SDL_GetError() << "\n";
        return failure;
    }
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    std::fill(screen.begin(), screen.end(), 0x00);
    scr_idx = 0;
    frame_idx = 0;
    frame_done = false;
    running = true;
    has_started = false;
    frames_per_second = 60;
    has_polled_after_rendering = false;

    std::fill(keyboard_state.begin(), keyboard_state.end(), false);

    return success;
}

void sdl::poll() {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            running = false;
        }
        if (event.type == SDL_KEYDOWN) {
            auto sc = event.key.keysym.scancode;
            if (sc == SDL_SCANCODE_ESCAPE) {
                running = false;
            }
            keyboard_state[sc] = true;
        }
    }
}

bool sdl::is_running() {
    return running;
}

bool sdl::should_poll() {
    if (frame_done) {
        if (not has_polled_after_rendering) {
            has_polled_after_rendering = true;
            return true;
        }
        if (frames_per_second * timer.get_ticks() < 1000 * frame_idx) {
            return true;
        } else {
            frame_done = false;
            return false;
        }
    }
    return false;
}

void sdl::start() {
    if (not has_started) {
        has_started = true;
        timer.reset();
    }
}

void sdl::close() {
    running = false;
    SDL_DestroyRenderer(renderer);
    renderer = nullptr;
    SDL_DestroyWindow(window);
    window = nullptr;

    SDL_Quit();
}

bool sdl::get_key(int sc) {
    auto res = keyboard_state[sc];

    auto ks = SDL_GetKeyboardState(nullptr);
    keyboard_state[sc] = ks[sc];

    return res;
}

void sdl::set_frames_per_second(unsigned val) {
    frames_per_second = val;
}
