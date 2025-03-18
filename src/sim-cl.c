#include "sim.h"
#include "misc.h"
#include <CL/cl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

void init_positions(void);
void init_velocities(void);
void init_grid(void);
void resolve_collisions(void);

static cl_context context;
static cl_device_id device;
static cl_program program;
static cl_kernel symplectic_euler_kernel;
static cl_kernel resolve_pair_collisions_kernel;
static cl_mem sim_mem;
static cl_command_queue cmdq;
static cl_ulong elapsed;

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
    init_cl();
    copy_balls_to_gpu();
}

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

static void symplectic_euler(void) {
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

void resolve_pair_collisions(void) {
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

void step_sim(void) {
    symplectic_euler();
    copy_balls_to_cpu();
    init_grid();
    copy_grid_to_gpu();
    resolve_collisions();
    copy_balls_to_cpu();
}

void print_profile(void) {
#ifdef USE_PROFILE
    printf("kernel: %ld ms\n", elapsed / 1000000);
#endif
}
