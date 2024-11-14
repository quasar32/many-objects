#include <glad/gl.h>
#include "draw.h"
#include "sim.h"

int main(void) {
    init_draw(640, 480);
    init_sim();
    int w = -1;
    int h = -1;
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
        update_sim();
        SDL_GL_SwapWindow(wnd);
    }
    return EXIT_SUCCESS;
}
