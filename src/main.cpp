#include <SDL.h>

#include "cartridge/cartridge.hpp"
#include "cpu/cpu6502.hpp"
#include "bus/bus.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
constexpr int WINDOW_WIDTH = 768;
constexpr int WINDOW_HEIGHT = 720;
constexpr int NES_WIDTH = 256;
constexpr int NES_HEIGHT = 240;

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
    NESBus bus;
    CPU6502 cpu;
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
        bus.connectCartridge(&cartridge);
        bus.reset();
        cpu.connectBus(&bus);
        cpu.reset();
        std::cout << "CPU reset complete.\n"
                  << "6502 reset vector: $" << std::hex << cpu.pc << std::dec << '\n';
    } else {
        bus.reset();
        std::cout << "No ROM supplied. Start with: ./build/nes-emulator path/to/game.nes\n";
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow(
        windowTitle.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN
    );
    if (window == nullptr) {
        std::cerr << "Window creation failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (renderer == nullptr) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == nullptr) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_Texture* screen = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        NES_WIDTH, NES_HEIGHT
    );
    if (screen == nullptr) {
        std::cerr << "Texture creation failed: " << SDL_GetError() << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "NES Emulator started. Press Escape to quit.\n";
    if (hasCartridge) {
        std::cout << "Experimental CPU execution active (partial 6502 instruction set; not cycle-accurate).\n";
    }
    std::array<std::uint32_t, NES_WIDTH * NES_HEIGHT> frame{};
    bool running = true;
    bool cpuStopReported = false;
    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
        }
        // Execute bounded CPU work per displayed frame. This is not cycle-accurate.
        if (hasCartridge && !cpu.halted()) {
            for (int instruction = 0; instruction < 30000; ++instruction) {
                if (!cpu.step()) {
                    if (!cpuStopReported) {
                        std::cerr << "CPU stopped: " << cpu.error() << '\\n';
                        cpuStopReported = true;
                    }
                    break;
                }
            }
        }
        bus.renderFrame(frame);
        SDL_UpdateTexture(screen, nullptr, frame.data(),
                          NES_WIDTH * static_cast<int>(sizeof(std::uint32_t)));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, screen, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
    SDL_DestroyTexture(screen);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
