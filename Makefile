CSV_OBJ=obj/balls.o obj/csv.o obj/sim.o
VID_OBJ=obj/balls.o obj/draw.o obj/gl.o obj/vid.o 
WND_OBJ=obj/balls.o obj/draw.o obj/gl.o obj/sim.o obj/wnd.o 

bin/csv: bin obj $(CSV_OBJ) 
	gcc $(CSV_OBJ) -o $@ -lm

bin/vid: bin obj $(VID_OBJ) 
	gcc $(VID_OBJ) -o $@ -lSDL2main -lSDL2 -lm -lswscale \
		-lavcodec -lavformat -lavutil -lx264 

bin/wnd: bin obj $(WND_OBJ) 
	gcc $(WND_OBJ) -o $@ -lSDL2main -lSDL2 -lm

bin:
	mkdir bin

obj/balls.o: src/balls.c src/balls.h src/sim.h 
	gcc $< -o $@ -Idep/cglm/include -c

obj/csv.o: src/csv.c src/balls.h src/sim.h 
	gcc $< -o $@ -Idep/cglm/include -c

obj/draw.o: src/draw.c src/draw.h src/balls.h 
	gcc $< -o $@ -Idep/cglm/include -Idep/glad/include -c

obj/gl.o: dep/glad/src/gl.c 
	gcc $< -o $@ -Idep/glad/include -c

obj/sim.o: src/sim.c src/sim.h 
	gcc $< -o $@ -Idep/cglm/include -c

obj/vid.o: src/vid.c src/balls.h src/draw.h 
	gcc $< -o $@ -Idep/cglm/include -Idep/glad/include -c

obj/wnd.o: src/wnd.c src/draw.h 
	gcc $< -o $@ -Idep/cglm/include -Idep/glad/include -c

obj:
	mkdir obj

clean:
	rm -rf obj bin 
