#include <stdio.h>
#define __USE_GNU
#include <stdlib.h>
#include <math.h>
#include <glad/gl.h>
#include "draw.h"
#include "sim.h"
#include "misc.h"

SDL_Window *wnd;

#define N_STACKS  8
#define N_SECTORS 16
#define N_INDICES ((N_STACKS + 1) * N_SECTORS * 6)

static GLuint vao;
static GLuint vbo;
static GLuint tex;
static GLuint prog;
static GLint proj_ul;
static vec3 colors[N_BALLS];

static vec3 vertices[N_STACKS + 1][N_SECTORS];
static GLushort indices[N_STACKS][N_SECTORS][6];

struct vertex {
    vec3 xyz;
    vec2 uv;
    vec3 rgb;
};

static struct vertex rect[6] = {
    {{0.5f, 0.5f}, {1.0f, 1.0f}}, 
    {{-0.5f, 0.5f}, {0.0f, 1.0f}},
    {{-0.5f, -0.5f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f}, {1.0f, 1.0f}}, 
    {{-0.5f, -0.5f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f}, {1.0f, 0.0f}}
};

static GLuint gen_shader(GLenum type, const char *path) {
    GLuint shader = glCreateShader(type);
    FILE *fp = fopen(path, "r");
    if (!fp) 
        die("could not open %s\n", path);
    char data[1024];
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

static void init_sdl(int width, int height) {
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
        sdl2_die("SDL_Init");
    if (atexit(SDL_Quit))
        std_die("atexit");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 
                    SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
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

static GLuint create_prog(const char *vert_path, const char *frag_path) {
    GLuint prog = glCreateProgram();
    GLuint vs = gen_shader(GL_VERTEX_SHADER, vert_path);
    glAttachShader(prog, vs);
    GLuint fs = gen_shader(GL_FRAGMENT_SHADER, frag_path);
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
    return prog;
}


static vec3 vertices[N_STACKS + 1][N_SECTORS];
static GLushort indices[N_STACKS][N_SECTORS][6];

static void create_sphere_texture(void) {
    /* generate sphere geometry*/
    float phi = GLM_PIf;
    for (int i = 0; i <= N_STACKS; i++) {
        float theta = 0.0f;
        float r = sinf(phi);
        float z = cosf(phi);
        for (int j = 0; j < N_SECTORS; j++) {
            vertices[i][j][0] = r * cosf(theta); 
            vertices[i][j][1] = r * sinf(theta);
            vertices[i][j][2] = z;
            theta += 2.0f * GLM_PIf / (N_SECTORS - 1);
        }
        phi += GLM_PIf / (N_STACKS * 2);
    }
    int r0 = 0;
    int r1 = 0;
    for (int i = 0; i < N_STACKS; i++) {
        r1 += N_SECTORS;
        for (int j = 0; j < N_SECTORS; j++) {
            int j2 = (j + 1) % N_SECTORS;
            indices[i][j][0] = r0 + j;
            indices[i][j][1] = r0 + j2;
            indices[i][j][2] = r1 + j;
            indices[i][j][3] = r1 + j2;
            indices[i][j][4] = r1 + j;
            indices[i][j][5] = r0 + j2;
        }
        r0 = r1;
    }

    GLuint vao, bos[2];
    glGenVertexArrays(1, &vao);
    glGenBuffers(2, bos);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, bos[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), 
                 vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), NULL);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bos[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), 
                 indices, GL_STATIC_DRAW);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, 64, 64, 0,
                 GL_RG, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                           GL_TEXTURE_2D, tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        die("glCheckFramebufferStatus: %u\n", glGetError());

    GLuint prog = create_prog("res/sphere.vert", "res/sphere.frag");
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glViewport(0, 0, 64, 64);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    mat4 world;
    glm_perspective_default(1, world);
    GLint world_ul = glGetUniformLocation(prog, "world");
    glUniformMatrix4fv(world_ul, 1, GL_FALSE, (float *) world);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, N_INDICES, GL_UNSIGNED_SHORT, NULL);
    glBindVertexArray(0);
    glDeleteProgram(prog);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteBuffers(2, bos);
    glDeleteVertexArrays(1, &vao);

    glBindTexture(GL_TEXTURE_2D, tex);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void init_bufs(void) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    for (int i = 0; i < N_BALLS; i++) {
        colors[i][0] = drand48();
        colors[i][1] = drand48();
        colors[i][2] = drand48();
    }
}

void init_draw(int w, int h) {
    init_sdl(w, h);
    prog = create_prog("res/vert.glsl", "res/frag.glsl");
    proj_ul = glGetUniformLocation(prog, "proj");
    init_bufs();
    create_sphere_texture();
}

static int pos_cmp(const void *ip, const void *jp, void *arg) {
    int i = *(int *) ip;
    int j = *(int *) jp;
    vec4 *v = arg;
    float a = v[i][2];
    float b = v[j][2];
    return (a > b) - (a < b);
}

void draw(int w, int h) {
    mat4 view;
    vec3 center;
    glm_vec3_add(eye, front, center);
    glm_lookat(eye, center, GLM_YUP, view);
    int n = 6 * N_BALLS;
    vec4 *poss = xmalloc(n * sizeof(*poss));
    for (int i = 0, k = 0; i < N_BALLS; i++) {
        mat4 mv;
        glm_translate_to(view, balls_pos[i], mv);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++)
                mv[i][j] = (i == j);
        }
        glm_scale_uni(mv, DIAMETER);
        for (int j = 0; j < 6; j++, k++) {
            glm_vec4(rect[j].xyz, 1.0f, poss[k]);
            glm_mat4_mulv(mv, poss[k], poss[k]);
        }
    }
    struct vertex *vertices = xmalloc(n * sizeof(*vertices));
    int *indices = xmalloc(N_BALLS * sizeof(*indices));
    for (int i = 0; i < N_BALLS; i++)
        indices[i] = i * 6;
    qsort_r(indices, N_BALLS, sizeof(int), pos_cmp, poss);
    for (int i = 0, k = 0; i < N_BALLS; i++) {
        for (int j = 0; j < 6; j++, k++) {
            int iv = indices[i];
            glm_vec3_copy(poss[iv + j], vertices[k].xyz);
            glm_vec2_copy(rect[j].uv, vertices[k].uv);
            glm_vec3_copy(colors[iv / 6], vertices[k].rgb);
        }
    }
    free(indices);
    indices = NULL;
    free(poss);
    poss = NULL;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    mat4 proj;
    glm_perspective_default(w / (float) h, proj);
    glUniformMatrix4fv(proj_ul, 1, GL_FALSE, (float *) proj);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, n * sizeof(*vertices), 
                 vertices, GL_DYNAMIC_DRAW);
    free(vertices);
    vertices = NULL;
#define VERTEX_ATTRIB(index, size, type, member) \
    glVertexAttribPointer(index, size, GL_FLOAT, GL_FALSE, \
                          sizeof(type), &((type *) 0)->member)
    VERTEX_ATTRIB(0, 3, struct vertex, xyz);
    VERTEX_ATTRIB(1, 2, struct vertex, uv);
    VERTEX_ATTRIB(2, 3, struct vertex, rgb);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glDrawArrays(GL_TRIANGLES, 0, n);
}
