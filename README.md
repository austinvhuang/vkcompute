# vkcompute

This is a minimal starter repo for experimenting with compute shaders for general purpose computation. It simply sets up an input array on the CPU, binds the data to GPU memory, dispatches a softmax computation, copies the result back to memory and prints it.

## Why? Who is this for?

For a while I've been hypothesizing that LLMs are coming to personal devices and that it would be an interesting transition when that happens. With projects like ggml (llama.cpp), smelte-rs this is starting to become a reality.

I'm interested in exploring if GPGPU on personal devices has something to add with vulkan and WebGPU. This is an initial small experiment towards that.

Even as a tiny toy computation, figuring out how to finagle vulkan for computation use cases was pretty painful as most of the resources out there are oriented towards graphics. It's difficult to learn vulkan for the first time and at the same time determine what aspect of tutorials are relevant to compute use cases. Hopefully this repo makes it a little easier for others to onboard and start experimenting. 

## What does it do?

1. `main.cpp` sets up input and output arrays of numbers on the host with the help of vulkan utility functions in `vkcompute.hpp`. The computation setup in `main.cpp` is annotated to help beginners follow the big picture of setting up a computation.
2. Calls out to execute a softmax computation implementated as a GPU compute shader in `softmax.glsl` (which is compiled to an SPIR-V artifact `build/softmax.spv`. 
3. After the computation is finished, `main.cpp` copies the output back to the host and prints the result.

Build dependencies are managed by conan and the build itself is defined using cmake (`CMakeLists.txt`. The `Makefile` has a few convenient aliases for building and running.

## Dependencies

- [conan](https://conan.io/) for installing library dependencies described in `conanfile.txt`.
- [cmake](https://cmake.org/) for building.
- [vulkan SDK](https://www.lunarg.com/vulkan-sdk/) - vulkan SDK includes vulkan headers and library files.
- [glslc](https://github.com/google/shaderc#downloads) - glsl compiler which compiles `src/softmax.glsl` to `build/softmax.spv`.

Optional:

`rg` and `entr` command line tools are used in the `Makefile` aliases for continuous builds. `clang-format` is also used by the makefile to automate formatting cleanup.

## Project Structure

- `src/main.cpp` the main entrypoint for the program - sets up, runs the shader computation, and prints the result.
- `src/vkcompute.hpp` header file of helper functions supporting setting up vulkan.
- `src/softmax.glsl` compute shader implementing a softmax computation as a (single workgroup) parallel sum reduction on the GPU.

## Building

Building and running requires two things:

1. Building the computer shader `src/softmax.glsl` to create a SPIR-V shader artifact `build/softmax.spv`
2. Building the main program with cmake

On mac, the program can be built with `make build/softmax.spv` to build the shader followed by `make run-osx` to build and run the program (see the `Makefile` for details if you want to do the steps manually. 

On linux, the program can be built with `make build/softmax.spv` to build the shader followed by `make run-linux` to build and run the program. 

## Contact and Contributions

You can find me via DM on twitter [@austinvhuang](https://twitter.com/austinvhuang).
