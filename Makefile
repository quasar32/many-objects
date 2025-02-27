BENCH_OBJ=obj/bench.o obj/misc.o obj/sim.o obj/worker.o
VIDEO_OBJ=obj/draw.o obj/gl.o obj/misc.o obj/sim.o obj/vid.o obj/worker.o
WINDOW_OBJ=obj/draw.o obj/gl.o obj/misc.o obj/sim.o obj/wnd.o obj/worker.o
CFLAGS=-Idep/cglm/include -Idep/glad/include -DCGLM_OMIT_NS_FROM_STRUCT_API

bin/bench: bin obj $(BENCH_OBJ) 
	gcc $(BENCH_OBJ) -o $@ -lm

bin/video: bin obj $(VIDEO_OBJ) 
	gcc $(VIDEO_OBJ) -o $@ -lSDL2main -lSDL2 -lm -lswscale \
		-lavcodec -lavformat -lavutil -lx264

bin/window: bin obj $(WINDOW_OBJ) 
	gcc $(WINDOW_OBJ) -o $@ -lSDL2main -lSDL2 -lm

bin:
	mkdir bin

obj/bench.o: src/bench.c src/sim.h 
	gcc $< -o $@ $(CFLAGS) -c

obj/draw.o: src/draw.c src/draw.h src/sim.h
	gcc $< -o $@ $(CFLAGS) -c -O3

obj/gl.o: dep/glad/src/gl.c 
	gcc $< -o $@ $(CFLAGS) -c

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
