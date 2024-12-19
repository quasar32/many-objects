#pragma once

#include <SDL2/SDL.h>
#include <cglm/cglm.h>

extern SDL_Window *wnd;

void init_draw(int w, int h);
void draw(int w, int h, vec3 eye, vec3 front);
