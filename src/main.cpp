#include <SDL.h>

#include "cartridge/cartridge.hpp"
#include "cpu/cpu6502.hpp"
#include "bus/bus.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace {
constexpr int WINDOW_WIDTH = 960;
constexpr int WINDOW_HEIGHT = 720;

const char* mirroringName(Mirroring mirroring) {
    switch (mirroring) {
        case Mirroring::Horizontal: return "Horizontal";
        case Mirroring::Vertical: return "Vertical";
        case Mirroring::FourScreen: return "Four-screen";
    }
    return "Unknown";
}
}

int main(int argc, char** argv) {
    Cartridge cartridge;
    bool hasCartridge = false;
    std::string windowTitle = "NES Emulator";

    if (argc > 1) {
        std::string error;
        const std::filesystem::path romPath(argv[1]);

        if (!loadCartridge(romPath, cartridge, error)) {
            std::cerr << "ROM loading failed: " << error << '\n';
            return 1;
        }

        hasCartridge = true;
        windowTitle += " - " + romPath.filename().string();

        std::cout << "Cartridge loaded: " << romPath.filename().string() << '\n'
                  << "Mapper: " << static_cast<int>(cartridge.mapper) << " (NROM)\n"
                  << "PRG-ROM: " << cartridge.prgRom.size() / 1024 << " KiB\n"
                  << "CHR: " << (cartridge.usesChrRam ? "8 KiB CHR-RAM" : "8 KiB CHR-ROM") << '\n'
                  << "Mirroring: " << mirroringName(cartridge.mirroring) << '\n';

        NESBus bus;
        bus.connectCartridge(&cartridge);
        bus.reset();

        CPU6502 cpu;
        cpu.connectBus(&bus);
        cpu.reset();
        std::cout << "CPU reset complete.\n"
                  << "6502 reset vector: $" << std::hex << cpu.pc
                  << std::dec << '\n';
    } else {
        std::cout << "No ROM supplied. Start with: ./build/nes-emulator path/to/game.nes\n";
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        windowTitle.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (window == nullptr) {
        std::cerr << "Window creation failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }

    if (renderer == nullptr) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "NES Emulator started. Press Escape to quit.\n";

    bool running = true;
    while (running) {
        SDL_Event event{};

        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        // The PPU will draw actual NES frames here in a later milestone.
        if (hasCartridge) {
            SDL_SetRenderDrawColor(renderer, 24, 38, 30, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 18, 22, 34, 255);
        }
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
