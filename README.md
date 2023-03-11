# vkCompute

A minimal example and template repo for GPU computation using vulkan.

What does it do?

`main.cpp` sets up input and output arrays of numbers on the host with the help of vulkan utility functions in `vkcompute.hpp`, executes a softmax computation implementated as a GPU compute shader in `softmax.glsl` (which is compiled to an SPIR-V artifact `build/softmax.spv`. After the computation is finished, `main.cpp` copies the output back to the host and prints the result.

Build dependencies are managed by conan and the build itself is defined using cmake (`CMakeLists.txt`. The `Makefile` has a few convenient aliases for building and running.

## Dependencies

- [vulkan SDK](https://www.lunarg.com/vulkan-sdk/) - vulkan SDK.
- [glslc](https://github.com/google/shaderc#downloads) - glsl compiler which compiles `src/softmax.glsl` to `build/softmax.spv`.
- cmake
- [conan](https://conan.io/)

## Building

*TODO*

## Running

*TODO*

## Contact and Contributions

*TODO*
