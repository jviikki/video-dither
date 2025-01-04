# video-dither

video-dither is a command line tool for creating video files with 1-bit
dithered look. The tool takes any video file, downscales it and converts each
frame into a 1-bit dithered image.

## Compilation

OpenCV library (with C++ headers) version 4.10 is required for compilation.

```
mkdir build
cd build
cmake ..
make
```

## Usage

```
video-dither -h 640 -w 480 input.avi output.avi
```
