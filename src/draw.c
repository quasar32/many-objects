#include <stdio.h>
#define __USE_GNU
#include <stdlib.h>
#include <math.h>
#include <glad/gl.h>
#include "draw.h"
#include "sim.h"
#include "misc.h"

SDL_Window *wnd;
vec3s eye = {-64.0f, 0.0f, 0.0f};
vec3s front = {1.0f, 0.0f, 0.0f};
int width = 640;
int height = 480;

static GLuint vao;
static GLuint ssbo[2];
static GLuint tex;
static GLuint prog;
static uint32_t colors[N_BALLS];

static vec4s gl_positions[N_BALLS];
static uint32_t gl_colors[N_BALLS];

static GLuint gen_shader(GLenum type, const char *path) {
    GLuint shader = glCreateShader(type);
    FILE *fp = fopen(path, "r");
    if (!fp) 
        die("could not open %s\n", path);
    char data[4096];
    size_t sz = fread(data, 1, sizeof(data), fp);
    if (sz == sizeof(data))
        die("%s too big\n", path);
    if (ferror(fp))
        die("error reading %s\n");
    fclose(fp);
    data[sz] = '\0';
    const char *src = data;
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, sizeof(data), NULL, data);
        die("%s: %s\n", path, data);
    }
    return shader;
}

static void sdl2_die(const char *func) {
    printf("%s(): %s\n", func, SDL_GetError());
    exit(EXIT_FAILURE);
}

static void std_die(const char *func) {
    perror(func);
    exit(EXIT_FAILURE);
}

static void destroy_wnd(void) {
    SDL_DestroyWindow(wnd);
}

static void atexit_fatal(void(*func)(void)) {
    if (atexit(func)) {
        func();
        std_die("atexit");
    }
}

static void init_sdl(void) {
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
        sdl2_die("SDL_Init");
    atexit_fatal(SDL_Quit);
    atexit_fatal(destroy_wnd);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 
                    SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    wnd = SDL_CreateWindow("Many Objects",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            width, height, SDL_WINDOW_OPENGL);
    if (!wnd)
        sdl2_die("SDL_CreateWindow");
    if (!SDL_GL_CreateContext(wnd))
        sdl2_die("SDL_GL_CreateContext");
    if (gladLoadGL((GLADloadfunc) SDL_GL_GetProcAddress) == 0)
        sdl2_die("SDL_GL_GetProcAddress");
}

static void init_prog(void) {
    prog = glCreateProgram();
    GLuint vs = gen_shader(GL_VERTEX_SHADER, "res/billboard.vert");
    glAttachShader(prog, vs);
    GLuint fs = gen_shader(GL_FRAGMENT_SHADER, "res/billboard.frag");
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    int success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        die("program: %s\n", log);
    }
    glDetachShader(prog, vs);
    glDeleteShader(vs);
    glDetachShader(prog, fs);
    glDeleteShader(fs);
}

static void init_tex(void) {
    double c = cos(GLM_PI / 8.0);
    double s = sin(GLM_PI / 8.0);
    int tex_size = 1024;
    uint16_t *pixels = xmalloc(tex_size * tex_size * 2);
    for (int px = 0; px < tex_size; px++) {
        double x0 = px / (tex_size / 2 - 0.5) - 1.0;
        for (int py = 0; py < tex_size; py++) {
            double y0 = py / (tex_size / 2 - 0.5) - 1.0;
            double z0 = sqrt(1 - x0 * x0 - y0 * y0);
            if (isnan(z0)) {
                pixels[py * tex_size + px] = 0;
            } else {
                double z1 = y0 * s + z0 * c;
                double z2 = x0 * s + z1 * c; 
                int intensity = fmax(z2, 0.0) * 255;
                pixels[py * tex_size + px] = intensity + 255 * 256;
            }
        }
    }
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, tex_size, tex_size, 0,
                 GL_RG, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    free(pixels);
}

static void init_bufs(void) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(2, ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(gl_positions), 
                 NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbo[0]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(gl_colors), 
                 NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo[1]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

static void init_colors(void) {
    for (int i = 0; i < N_BALLS; i++) {
        double h = drand48() * 6.0;
        double s = drand48() * 0.5 + 0.5;
        double v = drand48() * 0.5 + 0.5;
        double c = v * s;
        double x = c * (1 - fabs(fmod(h, 2.0) - 1.0));
        double rgb0[6][3] = {
            {c, x, 0},
            {x, c, 0},
            {0, c, x},
            {0, x, c},
            {x, 0, c},
            {c, 0, x}
        };
        double m = v - c;
        int j = h;
        uint32_t r = (rgb0[j][0] + m) * 255.0;
        uint32_t g = (rgb0[j][1] + m) * 255.0;
        uint32_t b = (rgb0[j][2] + m) * 255.0;
        colors[i] = (r << 16) | (g << 8) | b;
    }
}

void init_draw(void) {
    init_sdl();
    init_prog();
    init_bufs();
    init_tex();
    init_colors();
}

static int pos_cmp(const void *ip, const void *jp, void *arg) {
    int i = *(int *) ip;
    int j = *(int *) jp;
    float *v = arg;
    float a = v[i];
    float b = v[j];
    return (a > b) - (a < b);
}

void draw(void) {
    /* generate view matrices */
    vec3s center = vec3_add(eye, front);
    mat4s view = glms_lookat(eye, center, GLMS_YUP);
    float aspect = width / (float) height; 
    mat4s proj = glms_perspective_default(aspect);

    /* transform positions into view space*/
    float *zs = malloc(N_BALLS * sizeof(float));
    for (int i = 0; i < N_BALLS; i++) {
        vec4s v = glms_vec4(glms_vec3(sim.x[i]), 1.0f);
        v = mat4_mulv(view, v);
        zs[i] = v.z;
    }

    /* indirect sort view space positions back to front*/
    int *balls_idx = malloc(N_BALLS * sizeof(int));
    for (int i = 0; i < N_BALLS; i++) {
        balls_idx[i] = i;
    }
    qsort_r(balls_idx, N_BALLS, sizeof(int), pos_cmp, zs);
    free(zs);
    zs = NULL;

    /* create ball ssbo data*/
    for (int i = 0; i < N_BALLS; i++) {
        int j = balls_idx[i];
        gl_positions[i] = glms_vec4(glms_vec3(sim.x[j]), 1.0f);
        gl_colors[i] = colors[j];
    }
    free(balls_idx);
    balls_idx = NULL;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[0]);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                    sizeof(gl_positions), &gl_positions);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[1]);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                    sizeof(gl_colors), &gl_colors);
    
    /* render data */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    glUniformMatrix4fv(0, 1, GL_FALSE, (float *) &proj);
    glUniformMatrix4fv(1, 1, GL_FALSE, (float *) &view);
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glDrawArrays(GL_TRIANGLES, 0, N_BALLS * 6);
}
