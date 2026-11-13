cmake_minimum_required(VERSION 3.16)
project(NES_Emulator VERSION 0.16.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_executable(nes-emulator
    src/main.cpp
    src/cartridge/cartridge.cpp
    src/bus/bus.cpp
    src/ppu/ppu2c02.cpp
    src/cpu/cpu6502.cpp
)

target_link_libraries(nes-emulator PRIVATE PkgConfig::SDL2)

target_compile_options(nes-emulator PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
