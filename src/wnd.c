#include <glad/gl.h>
#include "draw.h"
#include "sim.h"

#define MAX_PITCH (GLM_PI_2f - 0.01f)
#define MIN_PITCH (-MAX_PITCH)

static vec3s right;
static float yaw; 
static float pitch; 
static const Uint8 *keys;

static void step_rots(void) {
    int x, y;
    SDL_GetRelativeMouseState(&x, &y);
    yaw += x * 0.001f;
    pitch -= y * 0.001f;
    yaw = fmodf(yaw, GLM_PIf * 2.0f);
    pitch = fminf(fmaxf(pitch, MIN_PITCH), MAX_PITCH);
}

static void step_dirs(void) {
    float xp = cosf(pitch);
    front.x = cosf(yaw) * xp;
    front.y = sinf(pitch);
    front.z = sinf(yaw) * xp;
    right = vec3_cross(front, GLMS_YUP);
    right = vec3_normalize(right);
}

static void step_eye(void) {
    if (keys[SDL_SCANCODE_W]) {
        eye = vec3_muladds(front, 20.0f * DT, eye);
    }
    if (keys[SDL_SCANCODE_S]) {
        eye = vec3_mulsubs(front, 20.0f * DT, eye);
    }
    if (keys[SDL_SCANCODE_A]) {
        eye = vec3_mulsubs(right, 20.0f * DT, eye);
    }
    if (keys[SDL_SCANCODE_D]) {
        eye = vec3_muladds(right, 20.0f * DT, eye);
    }
}

int main(void) {
    init_draw();
    init_sim();
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 frame = freq / SPS;
    Uint64 t0 = SDL_GetPerformanceCounter();
    Uint64 acc = 0;
    int n_keys;
    keys = SDL_GetKeyboardState(&n_keys);
    SDL_ShowWindow(wnd);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    while (!SDL_QuitRequested()) {
        Uint64 t1 = SDL_GetPerformanceCounter();
        acc += t1 - t0;
        t0 = t1;
        int w0 = width;
        int h0 = height;
        SDL_GetWindowSizeInPixels(wnd, &width, &height);
        if (width != w0 || height != h0) {
            glViewport(0, 0, width, height);
            SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
        draw();
        SDL_GL_SwapWindow(wnd);
        SDL_PumpEvents();
        step_rots();
        step_dirs();
        if (acc > freq / 10) {
            acc = freq / 10;
        }
        while (acc >= frame) {
            acc -= frame;
            step_eye();
            step_sim();
        }
    }
    return EXIT_SUCCESS;
}
