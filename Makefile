BENCH_CPU_OBJ=obj/bench.o obj/misc.o obj/sim-cpu.o obj/sim.o obj/worker.o
BENCH_GPU_OBJ=obj/bench.o obj/misc.o obj/sim-gpu.o obj/sim.o
VIDEO_OBJ=obj/draw.o obj/gl.o obj/misc.o obj/sim-cpu.o obj/sim.o obj/vid.o obj/worker.o
WINDOW_OBJ=obj/draw.o obj/gl.o obj/misc.o obj/sim-cpu.o obj/sim.o obj/wnd.o obj/worker.o
CFLAGS=-Idep/cglm/include -Idep/glad/include -DCGLM_OMIT_NS_FROM_STRUCT_API

bin/bench-cpu: bin obj $(BENCH_CPU_OBJ) 
	gcc $(BENCH_CPU_OBJ) -o $@ -lm -lOpenCL

bin/bench-gpu: bin obj $(BENCH_GPU_OBJ) 
	gcc $(BENCH_GPU_OBJ) -o $@ -lm -lOpenCL

bin/video: bin obj $(VIDEO_OBJ) 
	gcc $(VIDEO_OBJ) -o $@ -lSDL2main -lSDL2 -lm -lswscale \
		-lavcodec -lavformat -lavutil -lx264 -lOpenCL

bin/window: bin obj $(WINDOW_OBJ) 
	gcc $(WINDOW_OBJ) -o $@ -lSDL2main -lSDL2 -lm -lOpenCL

bin:
	mkdir bin

obj/bench.o: src/bench.c src/sim.h 
	gcc $< -o $@ $(CFLAGS) -c

obj/draw.o: src/draw.c src/draw.h src/sim.h
	gcc $< -o $@ $(CFLAGS) -c -O3

obj/gl.o: dep/glad/src/gl.c 
	gcc $< -o $@ $(CFLAGS) -c

obj/sim-cpu.o: src/sim-cpu.c src/sim.h src/worker.h
	gcc $< -o $@ $(CFLAGS) -c -O3

obj/sim-gpu.o: src/sim-gpu.c src/sim.h src/worker.h
	gcc $< -o $@ $(CFLAGS) -c -O3

obj/sim.o: src/sim.c src/sim.h src/worker.h
	gcc $< -o $@ $(CFLAGS) -c -O3

obj/vid.o: src/vid.c src/draw.h src/misc.h src/sim.h
	gcc $< -o $@ $(CFLAGS) -c

obj/wnd.o: src/wnd.c src/draw.h src/sim.h
	gcc $< -o $@ $(CFLAGS) -c

obj/misc.o: src/misc.c src/misc.h
	gcc $< -o $@ -c

obj/worker.o: src/worker.c src/worker.h src/misc.h
	gcc $< -o $@ -c

obj:
	mkdir obj

clean:
	rm -rf obj bin 
