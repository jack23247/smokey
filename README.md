# Smoke Propagation Simulator

Project for the course "Complex Systems: Models and Simulation".

## Prerequisistes

The software is written in C++ and can be compiled with the GCC or Clang compilers. CMake is required for building the software, as is AddressSanitizer if you're using the Debug config.

The software is based on Dear ImGui and uses the SDL2 and OpenGL3 backends. Those libraries, unlike ImGui, are not shipped with this project and must be procured separately.

On Fedora Linux, for example, the following packages must be installed:

```
SDL2-devel mesa-libGL-devel clang cmake libasan
```

AddressSanitizer is only a requirement for Debug builds. 


## Building

The software can be built from the command line as follows:

```
cmake --build $(pwd)/build --config [Debug,Release] --target all -j $(nproc)
```

The Debug config is unoptimized, exports Debug symbols and is built with `-fsanitize-address`. A set of suppressions is available and can be used by exporting the following environment variable from the project's root directory:

```
export LSAN_OPTIONS=suppressions=lsan-suppressions.txt
```

## Running

To run the program, launch `build/smokey` from the terminal.