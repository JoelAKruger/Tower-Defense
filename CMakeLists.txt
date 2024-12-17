cmake_minimum_required(VERSION 3.10)
project(TowerDefense)

find_package(GameNetworkingSockets REQUIRED)

add_compile_definitions(DEBUG=1)
add_executable(TowerDefense WIN32 src/PlatformWin32DX11.cpp)
target_link_libraries(TowerDefense GameNetworkingSockets::shared)

set_target_properties(TowerDefense PROPERTIES WIN32_EXECUTABLE TRUE)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(TowerDefense PRIVATE -Wno-writable-strings)
endif()