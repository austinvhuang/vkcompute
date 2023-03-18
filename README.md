# vkCompute

A minimal example and template repo for GPU computation using vulkan.

What does it do?

1. `main.cpp` sets up input and output arrays of numbers on the host with the help of vulkan utility functions in `vkcompute.hpp`
2. Calls out to execute a softmax computation implementated as a GPU compute shader in `softmax.glsl` (which is compiled to an SPIR-V artifact `build/softmax.spv`. 
3. After the computation is finished, `main.cpp` copies the output back to the host and prints the result.

Build dependencies are managed by conan and the build itself is defined using cmake (`CMakeLists.txt`. The `Makefile` has a few convenient aliases for building and running.

## Dependencies

- [conan](https://conan.io/) for installing library dependencies described in `conanfile.txt`.
- [cmake](https://cmake.org/) for building.
- [vulkan SDK](https://www.lunarg.com/vulkan-sdk/) - vulkan SDK includes vulkan headers and library files.
- [glslc](https://github.com/google/shaderc#downloads) - glsl compiler which compiles `src/softmax.glsl` to `build/softmax.spv`.

Optional:

`rg` and `entr` command line tools are used in the `Makefile` for continuous build watching. `clang-format` is also used by the makefile to automate formatting cleanup.

## Project Structure

- `src/main.cpp` the main entrypoint for the program - sets up, runs the shader computation, and prints the result.
- `src/vkcompute.hpp` header file of helper functions supporting setting up vulkan.
- `src/softmax.glsl` compute shader implementing a softmax computation as a (single workgroup) parallel sum reduction on the GPU.

## Building

Building and running requires two things:

1. Building the computer shader `src/softmax.glsl` to create a SPIR-V shader artifact `build/softmax.spv`
2. Building the main program with cmake

On mac, the program can be built with `make build/softmax.spv` to build the shader followed by `make run-osx` to build and run the program (see the `Makefile` for details if you want to do the steps manually. 

On linux, the program can be built with `make build/softmax.spv` to build the shader followed by `make run-linux` to build and run the program. [NOTE / TODO : it may crash if you don't have the khronos compatibility extension - TODO make this optional]

## Contact and Contributions

You can find me via DM on twitter [@austinvhuang](https://twitter.com/austinvhuang).
