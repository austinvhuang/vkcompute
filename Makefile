.PHONY:

ifeq ($(shell uname -s), Darwin)
    SUFFIX=osx
else ifeq ($(shell uname -s), Linux)
		SUFFIX=linux
else
    $(error Unsupported OS: $(shell uname -s))
endif

conan-deps-linux: .PHONY
	mkdir -p conan && cd conan && conan install .. --build=missing -pr:b=default -s compiler.libcxx=libstdc++11 -c tools.system.package_manager:mode=install

conan-deps-osx: .PHONY
	mkdir -p ./conan
	cd conan && conan install .. --build=missing -s compiler.libcxx=libc++ 

build-osx: .PHONY
	mkdir -p build 
	cd build && cmake .. -DCMAKE_BUILD_TYPE=Release
	cd build && cmake --build .

build-linux: .PHONY
	mkdir -p build 
	cd build && cmake .. -DCMAKE_TOOLCHAIN_FILE=../conan/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release 
	cd build && cmake --build .

build/softmax.spv: .PHONY
	@if ! which glslc >/dev/null; then \
			echo "Error: glslc not found in PATH. It can be obtained as part of the glslang install."; \
			exit 1; \
	fi
	glslc -fshader-stage=compute src/softmax.glsl -o build/softmax.spv

watch-shaders:
	rg --files | entr -s "mkdir -p ./build && make build/softmax.spv && echo 'Compiled shader'"

run-osx: build-osx build/softmax.spv
	./build/vkcompute

run-linux: build-linux build/softmax.spv
	./build/vkcompute

watch-osx: .PHONY build/softmax.spv
	rg -t cpp -t txt --files | entr -s "clang-format -i src/*.cpp src/*.hpp && make build-osx && ./build/vkcompute"

watch-linux: .PHONY build/softmax.spv
	rg -t cpp -t txt --files | entr -s "clang-format -i src/*.cpp src/*.hpp && make build-linux && ./build/vkcompute"

clean: .PHONY
	rm -rf conan
	rm -rf build

clformat:
	clang-format -i src/*.cpp src/*.hpp

###############################################################################
# Experimental Features
###############################################################################


build/dot.spv: .PHONY
	@if ! which glslc >/dev/null; then \
			echo "Error: glslc not found in PATH. It can be obtained as part of the glslang install."; \
			exit 1; \
	fi
	glslc -fshader-stage=compute src/dot.glsl -o build/dot.spv


build/hadamard.spv: .PHONY
	@if ! which glslc >/dev/null; then \
			echo "Error: glslc not found in PATH. It can be obtained as part of the glslang install."; \
			exit 1; \
	fi
	glslc -fshader-stage=compute src/hadamard.glsl -o build/hadamard.spv

xp-watch-shaders:
	rg --files | entr -s "mkdir -p ./build && make build/dot.spv && echo 'Compiled shader'"

xp-watch-osx: .PHONY build/dot.spv
	rg -t cpp -t txt --files | entr -s "clang-format -i src/*.cpp src/*.hpp && make build-osx && ./build/xp"

watch-format:
	rg -t cpp -t txt --files | entr -s "clang-format -i src/*.cpp src/*.hpp"
