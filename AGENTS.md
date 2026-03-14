# video-dither

Command-line tool that converts video files into 1-bit dithered (black and white) video using Sierra error diffusion dithering. Built with C++23 and OpenCV.

## Build

```bash
cd build && cmake .. && make
```

Requires OpenCV 4.10+ installed on the system.

## Run

```bash
./build/video-dither -h 480 -w 640 input.avi output.avi
```

Options: `-h <height>`, `-w <width>` (optional, defaults to input dimensions). Output is Motion JPEG, audio is discarded.

## Architecture

Three-stage concurrent pipeline:

1. **Producer** — reads frames from input video sequentially
2. **Worker pool** — `hardware_concurrency()` threads resize, grayscale convert, and apply Sierra dithering in parallel
3. **Consumer** — writes processed frames to output in correct order via priority queue

Synchronization: FIFO queue (producer→workers), priority queue (workers→consumer), binary semaphores for backpressure, condition variables for wake-up, atomic flags for completion signaling.

## Source layout

```
src/
  main.cpp          — entry point
  argparse.cpp/hpp  — CLI argument parsing
  video.cpp/hpp     — Sierra dithering algorithm and frame processing
  concurrency.cpp/hpp — producer-consumer threading pipeline
CMakeLists.txt      — build config (C++23, OpenCV)
```

## Key details

- C++23 standard required
- Sierra dithering distributes quantization error to 10 neighboring pixels
- Frames are processed out of order but reassembled in sequence via priority queue
- License: GPL-3.0
