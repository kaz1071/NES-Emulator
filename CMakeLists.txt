cmake_minimum_required(VERSION 3.16)
project(NES_Emulator VERSION 0.49.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_executable(nes-emulator
    src/main.cpp
    src/apu/apu.cpp
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

include(GNUInstallDirs)

install(TARGETS nes-emulator
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

configure_file(
    data/pixelcore-nes.desktop.in
    pixelcore-nes.desktop
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pixelcore-nes.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES data/pixelcore-nes.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

include(CTest)
if(BUILD_TESTING)
    add_executable(mmc5-irq-tests
        tests/mmc5_irq_tests.cpp
        src/cartridge/cartridge.cpp
    )
    target_include_directories(mmc5-irq-tests PRIVATE src/cartridge)
    target_compile_options(mmc5-irq-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME mmc5-irq-tests COMMAND mmc5-irq-tests)

    add_executable(mmc5-chr-tests
        tests/mmc5_chr_tests.cpp
        src/cartridge/cartridge.cpp
    )
    target_include_directories(mmc5-chr-tests PRIVATE src/cartridge)
    target_compile_options(mmc5-chr-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME mmc5-chr-tests COMMAND mmc5-chr-tests)

    add_executable(mmc5-audio-tests tests/mmc5_audio_tests.cpp src/apu/apu.cpp)
    target_include_directories(mmc5-audio-tests PRIVATE src/apu)
    target_compile_options(mmc5-audio-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME mmc5-audio-tests COMMAND mmc5-audio-tests)

    add_executable(mmc5-prg-tests
        tests/mmc5_prg_tests.cpp
        src/cartridge/cartridge.cpp
    )
    target_include_directories(mmc5-prg-tests PRIVATE src/cartridge)
    target_compile_options(mmc5-prg-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME mmc5-prg-tests COMMAND mmc5-prg-tests)
endif()
