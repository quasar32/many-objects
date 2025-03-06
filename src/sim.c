#include "sim.h"
#include "worker.h"
#include "misc.h"
#include <math.h>
#include <string.h>
#include <CL/cl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define USE_CL

struct sim sim;

static cl_context context;
static cl_device_id device;
static cl_program program;
static cl_kernel symplectic_euler_kernel;
static cl_kernel resolve_pair_collisions_kernel;
static cl_mem sim_mem;
static cl_command_queue cmdq;

static void init_positions(void) {
    for (int i = 0; i < N_BALLS; i++) {
        sim.x[i].x = i % 16 - GRID_LEN / 4 + 0.5f;
        sim.x[i].y = i / 16 % 16 - GRID_LEN / 4 + 0.5f;
        sim.x[i].z = i / 256 - GRID_LEN / 4 + 0.5f;
    }
}

static void init_velocities(void) {
    for (int i = 0; i < N_BALLS; i++) {
        sim.v[i].x = 2.0 * drand48() - 1.0;
        sim.v[i].y = 2.0 * drand48() - 1.0;
        sim.v[i].z = 2.0 * drand48() - 1.0;
    }
}

static void cl_notify(const char *err, const void *priv, size_t cb, void *user) {
    fprintf(stderr, "cl: %s\n", err);
}

static void cdie(const char *func) {
    die("%s(%d): %s\n", func, errno, strerror(errno));
}

static char *read_text_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        cdie("open");
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        cdie("fstat");
    }
    char *buf = malloc(st.st_size + 1);
    if (!buf) {
        cdie("malloc");
    }
    if (read(fd, buf, st.st_size) < st.st_size) {
        cdie("read");
    }
    buf[st.st_size] = '\0';
    close(fd);
    return buf;
} 

static cl_kernel create_kernel(const char *name) {
    cl_int err;
    cl_kernel kernel = clCreateKernel(program, name, &err);
    if (err) {
        die("clCreateKernel(%d)\n", err);
    }
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &sim_mem);
    if (err) {
        die("clSetKernelArg(%d)\n", err);
    }
    return kernel;
}

static void copy_balls_to_gpu(void) {
    cl_int err = clEnqueueWriteBuffer(
        cmdq, /* command queue */
        sim_mem, /* destination */
        CL_TRUE, /* blocking */
        0, /* offset of destination */
        sizeof(sim.x) + sizeof(sim.v), /* size of copy */
        &sim, /* source */
        0, /* empty wait list */
        NULL, 
        NULL
    );
    if (err) {
        die("clEnqueueWriteBuffer(%d)\n", err);
    }
}

static void init_cl(void) {
    cl_int err;
    err = clGetDeviceIDs(NULL, CL_DEVICE_TYPE_ALL, 1, &device, NULL);
    if (err) {
        die("clGetDeviceIDs(%d)\n", err);
    }
    context = clCreateContext(NULL, 1, &device, cl_notify, NULL, &err);
    if (err) {
        die("clCreateContext(%d)\n", err);
    }
    const char *code = read_text_file("res/sim.cl");
    program = clCreateProgramWithSource(context, 1, &code, NULL, &err);
    if (err) {
        die("clCreateProgramWithSource(%d)\n", err);
    }
    free((void *) code);
    code = NULL;
    err = clBuildProgram(program, 1, &device, "", NULL, NULL);
    if (err == CL_BUILD_PROGRAM_FAILURE) {
        size_t size;
        clGetProgramBuildInfo(
            program, 
            device, 
            CL_PROGRAM_BUILD_LOG, 
            0, 
            NULL, 
            &size
        );
        char *log = xmalloc(size);
        err = clGetProgramBuildInfo(
            program, 
            device, 
            CL_PROGRAM_BUILD_LOG, 
            size, 
            log, 
            NULL
        );
        fprintf(stderr, "clGetProgramBuildInfo: %s\n", log);
        free(log);
        log = NULL;
    }
    if (err) {
        die("clBuildProgram(%d)\n", err);
    }
    sim_mem = clCreateBuffer(context, 0, sizeof(sim), NULL, &err);
    if (err) {
        die("clCreateBuffer(%d)\n", err);
    }
    symplectic_euler_kernel = create_kernel("symplectic_euler");
    resolve_pair_collisions_kernel = create_kernel("resolve_pair_collisions");
    cmdq = clCreateCommandQueueWithProperties(
        context, 
        device, 
        (cl_queue_properties[]) {
#ifdef USE_PROFILE
            CL_QUEUE_PROPERTIES, 
            CL_QUEUE_PROFILING_ENABLE, 
#endif 
            0
        }, 
        &err
    );
    if (err) {
        die("clCreateCommandQueue(%d)\n", err);
    }
}

void init_sim(void) {
    init_positions();
    init_velocities();
#ifdef USE_CL
    init_cl();
    copy_balls_to_gpu();
#else
    create_workers();
#endif
}

static float fclampf(float v, float l, float h) {
    return fminf(fmaxf(v, l), h);
}

static void symplectic_euler(int worker_idx) {
    int i = worker_idx * N_BALLS / N_WORKERS;
    int n = (worker_idx + 1) * N_BALLS / N_WORKERS;
    for (; i < n; i++) {
        sim.v[i].y -= 10.0f * DT;
        sim.x0[i] = sim.x[i];
        sim.x[i] = vec4_muladds(sim.v[i], DT, sim.x[i]);
        sim.x[i].x = fclampf(sim.x[i].x, GRID_MIN, GRID_MAX);
        sim.x[i].y = fclampf(sim.x[i].y, GRID_MIN, GRID_MAX);
        sim.x[i].z = fclampf(sim.x[i].z, GRID_MIN, GRID_MAX);
        sim.v[i] = vec4_sub(sim.x[i], sim.x0[i]);
        sim.v[i] = vec4_scale(sim.v[i], SPS);
    }
}

static void init_grid(void) {
    memset(sim.grid, 0xFF, sizeof(sim.grid));
    for (int i = 0; i < N_BALLS; i++) {
        int x = sim.x[i].x + GRID_LEN / 2;
        int y = sim.x[i].y + GRID_LEN / 2;
        int z = sim.x[i].z + GRID_LEN / 2;
        sim.nodes[i] = sim.grid[x][y][z];
        sim.grid[x][y][z] = i;
    }
}

static void resolve_ball_ball_collision(int i, int j) {
    vec4s normal = vec4_sub(sim.x[i], sim.x[j]);
    float d2 = vec4_norm2(normal);
    if (d2 > 0.0f && d2 < DIAMETER * DIAMETER) {
        float d = sqrtf(d2);
        normal = vec4_divs(normal, d);
        float corr = (DIAMETER - d) / 2.0f;
        vec4s dx = vec4_scale(normal, corr);
        sim.x[i] = vec4_add(sim.x[i], dx);
        sim.x[j] = vec4_sub(sim.x[j], dx);
        float vi = vec4_dot(sim.v[i], normal);
        float vj = vec4_dot(sim.v[j], normal);
        vec4s dv = vec4_scale(normal, vi - vj);
        sim.v[i] = vec4_sub(sim.v[i], dv);
        sim.v[j] = vec4_add(sim.v[j], dv);
    }
}

static void resolve_tile_tile_collisions(int i0, int j0) {
    for (int i = i0; i >= 0; i = sim.nodes[i]) {
        if (i0 == j0) {
            for (int j = sim.nodes[i]; j >= 0; j = sim.nodes[j]) {
                resolve_ball_ball_collision(i, j);
            }
        } else {
            for (int j = j0; j >= 0; j = sim.nodes[j]) {
                resolve_ball_ball_collision(i, j);
            }
        }
    }
}

static void resolve_pair_collisions(int worker_idx) {
    int lx = (GRID_LEN - sim.px - sim.nx + sim.ix - 1) / sim.ix;
    int xi = worker_idx * lx / N_WORKERS * sim.ix + sim.px;
    int xf = (worker_idx + 1) * lx / N_WORKERS * sim.ix + sim.px;
    int yi = sim.py;
    int yf = GRID_LEN - sim.ny; 
    int zi = sim.pz;
    int zf = GRID_LEN - sim.nz;
    int ix = sim.ix;
    int iy = sim.iy;
    int iz = sim.iz;
    for (int x = xi; x < xf; x += ix) {
        for (int y = yi; y < yf; y += iy) {
            for (int z = zi; z < zf; z += iz) {
                int i = sim.grid[x][y][z];
                int j = sim.grid[x + sim.tx][y + sim.ty][z + sim.tz];
                resolve_tile_tile_collisions(i, j);
            }
        }
    }
}

static cl_ulong elapsed;

static void copy_grid_to_gpu(void) {
    cl_int err = clEnqueueWriteBuffer(
        cmdq, /* command queue */
        sim_mem, /* destination */
        CL_TRUE, /* blocking */
        offsetof(struct sim, nodes), /* offset of source */
        sizeof(sim.nodes) + sizeof(sim.grid), /* size of copy */
        &sim.nodes, /* source */
        0, /* empty wait list */
        NULL, 
        NULL
    );
    if (err) {
        die("clEnqueueWriteBuffer(%d)\n", err);
    }
}

static void symplectic_euler_gpu(void) {
    cl_event ev;
    cl_int err;
    err = clEnqueueNDRangeKernel(
        cmdq, 
        symplectic_euler_kernel, 
        1, 
        NULL, 
        (size_t[]) {N_BALLS}, 
        NULL, 
        0, 
        NULL, 
        &ev
    );
    if (err) {
        die("clEnqueueTask(%d)\n", err);
    }
    err = clWaitForEvents(1, &ev);
    if (err) {
        die("clWaitForEvents(%d)\n", err);
    }
    clReleaseEvent(ev);
}

static void resolve_pair_collisions_gpu(void) {
    cl_event ev;
    cl_int err;
    err = clEnqueueWriteBuffer(
        cmdq, /* command queue */
        sim_mem, /* destination */
        CL_TRUE, /* blocking */
        offsetof(struct sim, tx), /* offset of destination */
        24, /* size of copy */
        &sim.tx, /* source */
        0, /* empty wait list */
        NULL, 
        NULL
    );
    if (err) {
        die("clEnqueueWriteBuffer(%d)\n", err);
    }
    err = clEnqueueNDRangeKernel(
        cmdq, 
        resolve_pair_collisions_kernel, 
        3, 
        NULL, 
        (size_t[]) {GRID_LEN, GRID_LEN, GRID_LEN}, 
	(size_t[]) {8, 8, 8},
        0, 
        NULL, 
        &ev
    );
    if (err) {
        die("clEnqueueTask(%d)\n", err);
    }
    err = clWaitForEvents(1, &ev);
    if (err) {
        die("clWaitForEvents(%d)\n", err);
    }
#ifdef USE_PROFILE 
    cl_ulong start, end;
    err = clGetEventProfilingInfo(ev, CL_PROFILING_COMMAND_START, 8, &start, NULL);
    if (err) {
        die("clGetEventProfilingInfo(%d)\n", err);
    }
    err = clGetEventProfilingInfo(ev, CL_PROFILING_COMMAND_END, 8, &end, NULL);
    if (err) {
        die("clGetEventProfilingInfo(%d)\n", err);
    }
    elapsed += end - start;
#endif
    clReleaseEvent(ev);
}

static void resolve_collisions(void) {
    for (int i = 0; i < 27; i++) {
        int dx = i % 3 - 1;
        int dy = i / 3 % 3 - 1;
        int dz = i / 9 - 1;
        sim.ix = dx && !dy && !dz ? 2 : 1;
        sim.iy = dy && !dz ? 2 : 1;
        sim.iz = dz ? 2 : 1;
        int nd = abs(dx) + abs(dy) + abs(dz);
        sim.tx = abs(dx);
        sim.ty = abs(dy);
        sim.tz = abs(dz);
        switch (nd) {
        case 0:
        case 1:
            sim.px = dx < 0;
            sim.py = dy < 0;
            sim.nx = dx < 0;
            sim.ny = dy < 0;
            break;
        case 2:
            if (!dx) {
                sim.ty = dy / dz;
                sim.px = 0;
                sim.py = sim.ty < 0;
                sim.nx = 0;
                sim.ny = sim.ty > 0;
            } else if (!dy) {
                sim.tx = dx / dz;
                sim.px = sim.tx < 0;
                sim.py = 0;
                sim.nx = sim.tx > 0;
                sim.ny = 0;
            } else {
                sim.tx = dx / dy;
                sim.px = sim.tx < 0;
                sim.py = dy < 0;
                sim.nx = sim.tx > 0;
                sim.ny = dy < 0;
            }
            break;
        default:
            sim.tx = dx / dz;
            sim.ty = dy / dz;
            sim.px = sim.tx < 0;
            sim.py = sim.ty < 0;
            sim.nx = sim.tx > 0;
            sim.ny = sim.ty > 0;
        }
        sim.pz = dz < 0;
        sim.nz = dz < 0;
#ifdef USE_CL
        resolve_pair_collisions_gpu();
#else
        parallel_work(resolve_pair_collisions);
#endif
    }
}

static void copy_balls_to_cpu(void) {
    cl_int err = clEnqueueReadBuffer(
        cmdq, 
        sim_mem, 
        CL_TRUE, 
        0, 
        sizeof(sim.x),
        &sim, 
        0, 
        NULL, 
        NULL
    );
    if (err) {
        die("clEnqueueReadBuffer(%d)\n", err);
    }
}

void step_sim(void) {
#ifdef USE_CL 
    symplectic_euler_gpu();
    copy_balls_to_cpu();
    init_grid();
    copy_grid_to_gpu();
    resolve_collisions();
    copy_balls_to_cpu();
#else
    activate_workers();
    parallel_work(symplectic_euler);
    init_grid();
    resolve_collisions();
    deactivate_workers();
#endif
}

void print_profile(void) {
#ifdef USE_PROFILE
    printf("kernel: %ld ms\n", elapsed / 1000000);
#endif
}
