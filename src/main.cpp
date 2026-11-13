#include <SDL.h>
#include "cartridge/cartridge.hpp"
#include "cpu/cpu6502.hpp"
#include "bus/bus.hpp"
#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
constexpr int WINDOW_WIDTH = 768, WINDOW_HEIGHT = 768, TOOLBAR_HEIGHT = 48;
constexpr int NES_WIDTH = 256, NES_HEIGHT = 240;

const char* mirroringName(Mirroring m) {
    switch (m) {
        case Mirroring::Horizontal: return "Horizontal";
        case Mirroring::Vertical: return "Vertical";
        case Mirroring::FourScreen: return "Four-screen";
    }
    return "Unknown";
}

std::string chooseRomFile() {
    int pipeFd[2];
    if (pipe(pipeFd) != 0) {
        std::cerr << "Could not create file-picker pipe.\n";
        return {};
    }
    const pid_t child = fork();
    if (child < 0) {
        close(pipeFd[0]); close(pipeFd[1]);
        std::cerr << "Could not start file picker.\n";
        return {};
    }
    if (child == 0) {
        close(pipeFd[0]);
        dup2(pipeFd[1], STDOUT_FILENO);
        close(pipeFd[1]);
        char* zenity[] = {
            const_cast<char*>("zenity"), const_cast<char*>("--file-selection"),
            const_cast<char*>("--title=Choose an NES ROM"),
            const_cast<char*>("--file-filter=NES ROMs | *.nes"), nullptr
        };
        execvp("zenity", zenity);
        char* kdialog[] = {
            const_cast<char*>("kdialog"), const_cast<char*>("--getopenfilename"),
            const_cast<char*>(""), const_cast<char*>("*.nes|NES ROMs"),
            const_cast<char*>("--title"), const_cast<char*>("Choose an NES ROM"), nullptr
        };
        execvp("kdialog", kdialog);
        _exit(127);
    }
    close(pipeFd[1]);
    std::string path;
    char buffer[512];
    ssize_t bytes;
    while ((bytes = read(pipeFd[0], buffer, sizeof(buffer))) > 0)
        path.append(buffer, static_cast<std::size_t>(bytes));
    close(pipeFd[0]);
    int status = 0;
    waitpid(child, &status, 0);
    while (!path.empty() && (path.back() == '\n' || path.back() == '\r'))
        path.pop_back();
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127)
        std::cerr << "No Linux file picker found. Install one with: sudo apt install zenity\n";
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return {};
    return path;
}

void drawLoadButton(SDL_Renderer* renderer) {
    SDL_Rect button{312, 8, 144, 32};
    SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
    SDL_RenderFillRect(renderer, &button);
    SDL_SetRenderDrawColor(renderer, 210, 210, 210, 255);
    SDL_RenderDrawRect(renderer, &button);

    // Tiny built-in 5x7 font for the LOAD ROM button.
    const std::array<std::array<unsigned char, 7>, 9> glyphs{{
        {{16,16,16,16,16,16,31}}, {{14,17,17,17,17,17,14}},
        {{14,17,17,31,17,17,17}}, {{30,17,17,17,17,17,30}},
        {{30,17,17,30,20,18,17}}, {{31,16,16,30,16,16,31}},
        {{17,27,21,21,17,17,17}}, {{0,0,0,0,0,0,0}},
        {{31,16,16,30,16,16,16}}
    }};
    const std::array<int, 8> letters{{0,1,2,3,7,4,1,6}}; // LOAD ROM
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    int x = 325;
    for (int glyph : letters) {
        for (int y = 0; y < 7; ++y)
            for (int col = 0; col < 5; ++col)
                if (glyphs[glyph][y] & (1U << (4-col))) {
                    SDL_Rect pixel{x + col*2, 13 + y*2, 2, 2};
                    SDL_RenderFillRect(renderer, &pixel);
                }
        x += 15;
    }
}

void printCartridgeInfo(const Cartridge& c, const std::filesystem::path& path) {
    std::cout << "Cartridge loaded: " << path.filename().string() << '\n'
              << "Mapper: " << static_cast<int>(c.mapper) << " (NROM)\n"
              << "PRG-ROM: " << c.prgRom.size()/1024 << " KiB\n"
              << "CHR: " << (c.usesChrRam ? "8 KiB CHR-RAM" : "8 KiB CHR-ROM") << '\n'
              << "Mirroring: " << mirroringName(c.mirroring) << '\n';
}
}

int main(int argc, char** argv) {
    Cartridge cartridge;
    NESBus bus;
    CPU6502 cpu;
    bool hasCartridge = false;
    bool cpuStopReported = false;
    std::string windowTitle = "NES Emulator";
    bus.reset();

    if (argc > 1) {
        std::string error;
        const std::filesystem::path path(argv[1]);
        if (!loadCartridge(path, cartridge, error)) {
            std::cerr << "ROM loading failed: " << error << '\n';
            return 1;
        }
        hasCartridge = true;
        windowTitle += " - " + path.filename().string();
        printCartridgeInfo(cartridge, path);
        bus.connectCartridge(&cartridge);
        bus.reset();
        cpu.connectBus(&bus);
        cpu.reset();
        std::cout << "CPU reset complete. 6502 reset vector: $" << std::hex
                  << cpu.pc << std::dec << '\n';
    } else {
        std::cout << "No ROM loaded. Click LOAD ROM to choose an iNES NROM file.\n";
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow(windowTitle.c_str(), SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window); SDL_Quit();
        return 1;
    }
    SDL_Texture* screen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, NES_WIDTH, NES_HEIGHT);
    if (!screen) {
        std::cerr << "Texture creation failed: " << SDL_GetError() << '\n';
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        return 1;
    }

    std::cout << "NES Emulator started. Press Escape to quit.\n";
    if (hasCartridge)
        std::cout << "Experimental CPU execution active (partial 6502 instruction set; not cycle-accurate).\n";

    std::array<std::uint32_t, NES_WIDTH * NES_HEIGHT> frame{};
    bool running = true;
    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
                event.button.x >= 312 && event.button.x < 456 &&
                event.button.y >= 8 && event.button.y < 40) {
                const std::string selected = chooseRomFile();
                if (!selected.empty()) {
                    Cartridge nextCartridge;
                    std::string error;
                    const std::filesystem::path path(selected);
                    if (!loadCartridge(path, nextCartridge, error)) {
                        std::cerr << "ROM loading failed: " << error << '\n';
                        SDL_SetWindowTitle(window, "NES Emulator - ROM load failed (see terminal)");
                    } else {
                        cartridge = std::move(nextCartridge);
                        hasCartridge = true;
                        cpuStopReported = false;
                        bus.connectCartridge(&cartridge);
                        bus.reset();
                        cpu.connectBus(&bus);
                        cpu.reset();
                        windowTitle = "NES Emulator - " + path.filename().string();
                        SDL_SetWindowTitle(window, windowTitle.c_str());
                        printCartridgeInfo(cartridge, path);
                        std::cout << "CPU reset complete. 6502 reset vector: $" << std::hex
                                  << cpu.pc << std::dec << '\n'
                                  << "Experimental CPU execution active (partial 6502 instruction set; not cycle-accurate).\n";
                    }
                }
            }
        }

        if (hasCartridge && !cpu.halted()) {
            for (int instruction = 0; instruction < 30000; ++instruction) {
                if (!cpu.step()) {
                    if (!cpuStopReported) {
                        std::cerr << "CPU stopped: " << cpu.error() << '\n';
                        cpuStopReported = true;
                    }
                    break;
                }
            }
        }

        bus.renderFrame(frame);
        SDL_UpdateTexture(screen, nullptr, frame.data(),
            NES_WIDTH * static_cast<int>(sizeof(std::uint32_t)));
        SDL_SetRenderDrawColor(renderer, 18, 22, 30, 255);
        SDL_RenderClear(renderer);
        const SDL_Rect gameArea{0, TOOLBAR_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT-TOOLBAR_HEIGHT};
        SDL_RenderCopy(renderer, screen, nullptr, &gameArea);
        drawLoadButton(renderer);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(screen);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
