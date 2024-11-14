#pragma once

#include <SDL2/SDL.h>

extern SDL_Window *wnd;

void init_draw(int w, int h);
void draw(int w, int h);
void die(const char *fmt, ...);
