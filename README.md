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

## Building

On mac:

```
	mkdir -p build 
	cd build && cmake .. -DCMAKE_BUILD_TYPE=Release
	cd build && cmake --build .
```

Alternatively use `make build-osx`.

Then run `build/vkcompute` (note that the program loads the `softmax.spv` shader at runtime so being in the right relative path (the top level directory) is important. Otherwise the shader file won't be found and loaded.





## Running

*TODO*

## Contact and Contributions

*TODO*
