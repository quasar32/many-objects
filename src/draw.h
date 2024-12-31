#pragma once

#include <SDL2/SDL.h>
#include <cglm/struct.h>

extern SDL_Window *wnd;
extern vec3s eye;
extern vec3s front;
extern int width;
extern int height;

void init_draw(void);
void draw(void);
