#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <glad/gl.h>
#include "draw.h"
#include "balls.h"

#define N_STACKS  8
#define N_SECTORS 16
#define N_INDICES ((N_STACKS + 1) * N_SECTORS * 6)

struct instance {
    mat4 model;
    mat3 invt;
};

SDL_Window *wnd;

static GLuint vao;
static GLuint bos[3];
static GLuint prog;
static GLint world_loc;
static vec3 vertices[N_STACKS + 1][N_SECTORS];
static GLushort indices[N_STACKS][N_SECTORS][6];
static struct instance instances[N_BALLS];

static const char vs_src[] = 
    "#version 330 core\n"
    "layout(location = 0) in vec3 pos;"
    "layout(location = 1) in mat4 model;"
    "layout(location = 5) in mat3 invt;"
    "out vec3 frag_pos;"
    "out vec3 vs_norm;"
    "uniform mat4 world;"
    "void main() {"
        "vec4 model_pos = model * vec4(pos, 1.0f);"
        "gl_Position = world * model_pos;"
	"frag_pos = vec3(model_pos);"
	"vs_norm = invt * pos;"
    "}";

static const char fs_src[] = 
    "#version 330 core\n"
    "out vec4 color;"
    "in vec3 norm;"
    "in vec3 frag_pos;"
    "in vec3 vs_norm;"
    "void main() {"
        "float ambient_strength = 0.1f;"
        "vec3 light_color = vec3(1.0f, 1.0f, 1.0f);"
        "vec3 ambient = ambient_strength * light_color;"

        "vec3 norm = normalize(vs_norm);"
        "vec3 light_pos = vec3(-8.0f, -8.0f, -48.0f);"
        "vec3 light_dir = normalize(light_pos - frag_pos);"
        "float diff = max(dot(norm, light_dir), 0.0F);"
        "vec3 diffuse = diff * light_color;"

        "float specular_strength = 0.5f;"
        "vec3 view_pos = vec3(-8.0f, -8.0f, -48.0f);"
        "vec3 view_dir = normalize(view_pos - frag_pos);"
        "vec3 reflect_dir = reflect(-light_dir, norm);"
        "float spec = pow(max(dot(view_dir, reflect_dir), 0.0F), 32.0F);"
        "vec3 specular = specular_strength * spec * light_color;"

        "vec3 obj_color = vec3(1.0f, 0.0f, 0.0f);"
        "vec3 rgb = (ambient + diffuse) * obj_color;"
        "color = vec4(rgb, 1.0F);"
    "}";

void die(const char *fmt, ...) {
    va_list ap;	
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

static GLuint gen_shader(GLenum type, const char *src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        die("shader: %s\n", log);
    }
    return shader;
}

static void init_prog(void) {
    prog = glCreateProgram();
    GLuint vs = gen_shader(GL_VERTEX_SHADER, vs_src);
    glAttachShader(prog, vs);
    GLuint fs = gen_shader(GL_FRAGMENT_SHADER, fs_src);
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
    world_loc = glGetUniformLocation(prog, "world");
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

static void init_sphere(void) {
    float phi = 0.0f;
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
        phi += GLM_PIf / N_STACKS;
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
}

static void buffer_instances(void) {
    glBindBuffer(GL_ARRAY_BUFFER, bos[2]);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(instances), instances);
    for (int i = 0; i < 4; i++) 
        glVertexAttribPointer(i + 1, 4, GL_FLOAT, GL_FALSE, 
                sizeof(*instances), &((struct instance *) 0)->model[i]);
    for (int i = 0; i < 3; i++) 
        glVertexAttribPointer(i + 5, 3, GL_FLOAT, GL_FALSE, 
                sizeof(*instances), &((struct instance *) 0)->invt[i]);
    for (int i = 1; i < 8; i++) 
        glVertexAttribDivisor(i, 1);
}

static void init_buffers(void) {
    glCreateVertexArrays(1, &vao);
    glCreateBuffers(3, bos);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, bos[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), 
            vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), NULL);
    for (int i = 0; i < 8; i++)
        glEnableVertexAttribArray(i);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bos[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), 
            indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, bos[2]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(instances), 
            NULL, GL_DYNAMIC_DRAW);
    glBindVertexArray(0);
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

void init_draw(int width, int height) {
    init_sdl(width, height);
    init_sphere();
    init_buffers();
    init_prog();
}

void draw(int w, int h) {
    glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(prog);
    mat4 proj, view, world;
    glm_perspective(GLM_PI_4f, w / (float) h, 0.1f, 100.0f, proj);
    glm_translate_make(view, (vec3) {-8.0f, -8.0f, -48.0f});
    glm_rotate_y(view, GLM_PIf / 16.0f, view);
    glm_mul(proj, view, world);
    glUniformMatrix4fv(world_loc, 1, GL_FALSE, (float *) world);
    for (int i = 0; i < N_BALLS; i++) {
        glm_mat4_identity(instances[i].model);
        glm_translate(instances[i].model, balls_pos[i]);
        glm_scale_uni(instances[i].model, RADIUS);
        mat4 tmp;
        glm_mat4_inv(instances[i].model, tmp);
        glm_mat4_pick3t(instances[i].model, instances[i].invt);
    }
    glBindVertexArray(vao);
    buffer_instances();
    glDrawElementsInstanced(GL_TRIANGLES, N_INDICES, 
            GL_UNSIGNED_SHORT, NULL, N_BALLS);
}
