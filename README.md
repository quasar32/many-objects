# Many Objects 

Optimized simulation of many spheres on CPU.

## Build

Use `make` with the name of the program you wish to create.
If no name is provided will create `bin/bench`.

## `bin/bench-mt`

`bin/bench-mt` outputs the time it takes to simulate 30 
seconds of a multi-threaded simulation.

## `bin/bench-st`

`bin/bench-st` outputs the time it takes to simulate 30 
seconds of single-threaded simulation. 

## `bin/bench-nh`

`bin/bench-nh` outputs the time it takes to simulate 30 
seconds of a single-threaded simulation without
spatial gridding.

## `bin/bench-cl`

`bin/bench-cl` outputs the time it takes to simulate 30 
seconds of OpenCL simulation. 

## `bin/video`

Outputs 30 seconds of simulation as a video. If video path
not provided, default is `many-objects.mp4`.

`bin/video` 

## `bin/window`
`bin/window` opens window with a continuous simulation.
