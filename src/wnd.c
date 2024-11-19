#include <glad/gl.h>
#include "draw.h"
#include "sim.h"

int main(void) {
    init_draw(640, 480);
    init_sim();
    int w = -1;
    int h = -1;
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 frame = freq / FPS;
    Uint64 t0 = SDL_GetPerformanceCounter();
    Uint64 acc = 0;
    SDL_ShowWindow(wnd);
    while (!SDL_QuitRequested()) {
        int w0, h0;
        SDL_GetWindowSizeInPixels(wnd, &w0, &h0);
        if (w != w0 && h != h0) {
            w = w0;
            h = h0;
            glViewport(0, 0, w, h);
        }
        draw(w, h);
        SDL_GL_SwapWindow(wnd);
        Uint64 t1 = SDL_GetPerformanceCounter();
        acc += t1 - t0;
        t0 = t1;
        while (acc >= frame) {
            acc -= frame;
            update_sim();
        }
    }
    return EXIT_SUCCESS;
}
