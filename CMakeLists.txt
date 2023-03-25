cmake_minimum_required(VERSION 3.15)
project(vkcompute CXX)

set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}/conan")

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(${PROJECT_NAME} src/main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE src)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)

# Experimental

add_executable(xp src/experimental.cpp)

target_include_directories(xp PRIVATE src)

set_property(TARGET xp PROPERTY CXX_STANDARD 17)

target_link_libraries(xp PRIVATE fmt::fmt)
target_link_libraries(xp PRIVATE spdlog::spdlog)
target_link_libraries(xp PRIVATE Vulkan::Vulkan)
