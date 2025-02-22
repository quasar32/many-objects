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

struct sim sim;

static cl_context context;
static cl_device_id device;
static cl_program program;
static cl_kernel kernel;
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
    fprintf(stderr, "cl: %s\n");
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

static void init_cl(void) {
    cl_int err;
    err = clGetDeviceIDs(NULL, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
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
        fprintf(stderr, "cl: %s\n", log);
        free(log);
        log = NULL;
    }
    if (err) {
        die("clBuildProgram(%d)\n", err);
    }
    kernel = clCreateKernel(program, "resolve_pair_collisions", &err);
    if (err) {
        die("clCreateKernel(%d)\n", err);
    }
    sim_mem = clCreateBuffer(context, 0, sizeof(sim), NULL, &err);
    if (err) {
        die("clCreateBuffer(%d)\n", err);
    }
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &sim_mem);
    if (err) {
        die("clSetKernelArg(%d)\n", err);
    }
    cmdq = clCreateCommandQueueWithProperties(
        context, 
        device, 
        (cl_queue_properties[]) {
            CL_QUEUE_PROPERTIES, 
            CL_QUEUE_PROFILING_ENABLE, 
            0
        }, 
        &err
    );
    if (err) {
        die("clCreateCommandQueue(%d)\n", err);
    }
}

void init_sim(void) {
    init_cl();
    create_workers();
    init_positions();
    init_velocities();
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
        sim.x[i] = vec3_muladds(sim.v[i], DT, sim.x[i]);
        sim.x[i].x = fclampf(sim.x[i].x, GRID_MIN, GRID_MAX);
        sim.x[i].y = fclampf(sim.x[i].y, GRID_MIN, GRID_MAX);
        sim.x[i].z = fclampf(sim.x[i].z, GRID_MIN, GRID_MAX);
        sim.v[i] = vec3_sub(sim.x[i], sim.x0[i]);
        sim.v[i] = vec3_scale(sim.v[i], SPS);
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
    vec3s normal = vec3_sub(sim.x[i], sim.x[j]);
    float d2 = vec3_norm2(normal);
    if (d2 > 0.0f && d2 < DIAMETER * DIAMETER) {
        float d = sqrtf(d2);
        normal = vec3_divs(normal, d);
        float corr = (DIAMETER - d) / 2.0f;
        vec3s dx = vec3_scale(normal, corr);
        sim.x[i] = vec3_add(sim.x[i], dx);
        sim.x[j] = vec3_sub(sim.x[j], dx);
        float vi = vec3_dot(sim.v[i], normal);
        float vj = vec3_dot(sim.v[j], normal);
        vec3s dv = vec3_scale(normal, vi - vj);
        sim.v[i] = vec3_sub(sim.v[i], dv);
        sim.v[j] = vec3_add(sim.v[j], dv);
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

static void gpu_runtime(void) {
    cl_event ev;
    cl_int err;
    err = clEnqueueWriteBuffer(
        cmdq, /* command queue */
        sim_mem, /* destination */
        CL_TRUE, /* blocking */
        0, /* offset of destination */
        sizeof(sim), /* size of copy */
        &sim, /* source */
        0, /* empty wait list */
        NULL, 
        &ev 
    );
    if (err) {
        die("clEnqueueWriteBuffer(%d)\n", err);
    }
    err = clEnqueueNDRangeKernel(
        cmdq, 
        kernel, 
        1, 
        NULL, 
        (size_t[]) {64}, 
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
    clReleaseEvent(ev);
#if 0
    err = clEnqueueReadBuffer(
        cmdq, 
        sim_mem, 
        CL_TRUE, 
        0, 
        sizeof(sim), 
        &sim, 
        0, 
        NULL, 
        NULL
    );
    if (err) {
        die("clEnqueueReadBuffer", err);
    }
#endif
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
        //parallel_work(resolve_pair_collisions);
        gpu_runtime();
    }
}

void step_sim(void) {
    activate_workers();
    parallel_work(symplectic_euler);
    init_grid();
    resolve_collisions();
    deactivate_workers();
    static int n;
    if (++n == N_STEPS / 10)
        printf("%f\n", elapsed / 1e9);
}
