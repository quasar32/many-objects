BENCH_ST_OBJ=obj/bench.o obj/misc.o obj/sim-st.o obj/sim.o
BENCH_MT_OBJ=obj/bench.o obj/misc.o obj/sim-mt.o obj/sim.o obj/worker.o
BENCH_CL_OBJ=obj/bench.o obj/misc.o obj/sim-cl.o obj/sim.o
BENCH_NH_OBJ=obj/bench.o obj/misc.o obj/sim-nh.o obj/sim.o
VIDEO_OBJ=obj/draw.o obj/gl.o obj/misc.o obj/sim-mt.o obj/sim.o obj/vid.o obj/worker.o
WINDOW_OBJ=obj/draw.o obj/gl.o obj/misc.o obj/sim-st.o obj/sim.o obj/wnd.o obj/worker.o
CFLAGS=-Idep/cglm/include -Idep/glad/include -DCGLM_OMIT_NS_FROM_STRUCT_API

bin/bench-mt: bin obj $(BENCH_MT_OBJ) 
	gcc $(BENCH_MT_OBJ) -o $@ -lm

bin/bench-st: bin obj $(BENCH_ST_OBJ) 
	gcc $(BENCH_ST_OBJ) -o $@ -lm

bin/bench-cl: bin obj $(BENCH_CL_OBJ) 
	gcc $(BENCH_CL_OBJ) -o $@ -lm -lOpenCL

bin/bench-nh: bin obj $(BENCH_NH_OBJ) 
	gcc $(BENCH_NH_OBJ) -o $@ -lm

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

obj/sim-mt.o: src/sim-mt.c src/sim.h src/worker.h
	gcc $< -o $@ $(CFLAGS) -c -O3

obj/sim-cl.o: src/sim-cl.c src/sim.h
	gcc $< -o $@ $(CFLAGS) -c -O3

obj/sim-st.o: src/sim-st.c src/sim.h
	gcc $< -o $@ $(CFLAGS) -c -O3

obj/sim-nh.o: src/sim-nh.c src/sim.h
	gcc $< -o $@ $(CFLAGS) -c -O3

obj/sim.o: src/sim.c src/sim.h
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
