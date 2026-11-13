#include <SDL.h>
#include "cartridge/cartridge.hpp"
#include "cpu/cpu6502.hpp"
#include "bus/bus.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
constexpr int WINDOW_WIDTH = 768, WINDOW_HEIGHT = 768;
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

constexpr int BUTTON_WIDTH = 320;
constexpr int BUTTON_HEIGHT = 72;
constexpr int BUTTON_GAP = 18;

std::array<SDL_Rect, 3> menuButtons(int width, int height) {
    const int x = (width - BUTTON_WIDTH) / 2;
    const int firstY = height / 2 - (3 * BUTTON_HEIGHT + 2 * BUTTON_GAP) / 2;
    return {{
        {x, firstY, BUTTON_WIDTH, BUTTON_HEIGHT},
        {x, firstY + BUTTON_HEIGHT + BUTTON_GAP, BUTTON_WIDTH, BUTTON_HEIGHT},
        {x, firstY + 2 * (BUTTON_HEIGHT + BUTTON_GAP), BUTTON_WIDTH, BUTTON_HEIGHT}
    }};
}

bool contains(const SDL_Rect& rect, int x, int y) {
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}

SDL_Rect gameRectangle(int width, int height) {
    const float scale = std::min(width / 256.0f, height / 240.0f);
    const int w = static_cast<int>(256 * scale);
    const int h = static_cast<int>(240 * scale);
    return {(width - w) / 2, (height - h) / 2, w, h};
}

std::array<unsigned char, 7> glyphFor(char c) {
    switch (c) {
        case 'L': return {{16,16,16,16,16,16,31}};
        case 'O': return {{14,17,17,17,17,17,14}};
        case 'A': return {{14,17,17,31,17,17,17}};
        case 'D': return {{30,17,17,17,17,17,30}};
        case 'C': return {{14,17,16,16,16,17,14}};
        case 'E': return {{31,16,16,30,16,16,31}};
        case 'N': return {{17,25,25,21,19,19,17}};
        case 'R': return {{30,17,17,30,20,18,17}};
        case 'M': return {{17,27,21,21,17,17,17}};
        case 'Q': return {{14,17,17,17,21,18,13}};
        case 'U': return {{17,17,17,17,17,17,14}};
        case 'I': return {{31,4,4,4,4,4,31}};
        case 'T': return {{31,4,4,4,4,4,4}};
        case 'F': return {{31,16,16,30,16,16,16}};
        case 'S': return {{15,16,16,14,1,1,30}};
        default: return {{0,0,0,0,0,0,0}};
    }
}

void drawMenuButton(SDL_Renderer* renderer, const SDL_Rect& button,
                    const std::string& label) {
    SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
    SDL_RenderFillRect(renderer, &button);
    SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
    SDL_RenderDrawRect(renderer, &button);

    constexpr int scale = 4;
    constexpr int glyphWidth = 5 * scale;
    constexpr int glyphGap = 2 * scale;
    const int textWidth = static_cast<int>(label.size()) * glyphWidth
                        + static_cast<int>(label.size() - 1) * glyphGap;
    const int originX = button.x + (button.w - textWidth) / 2;
    const int originY = button.y + (button.h - 7 * scale) / 2;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    int x = originX;
    for (char c : label) {
        const auto glyph = glyphFor(c);
        for (int y = 0; y < 7; ++y) {
            for (int col = 0; col < 5; ++col) {
                if (glyph[y] & (1U << (4 - col))) {
                    SDL_Rect pixel{x + col * scale, originY + y * scale,
                                   scale, scale};
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
        x += glyphWidth + glyphGap;
    }
}

void drawMenuButtons(SDL_Renderer* renderer, int width, int height) {
    const auto buttons = menuButtons(width, height);
    drawMenuButton(renderer, buttons[0], "LOAD ROM");
    drawMenuButton(renderer, buttons[1], "FULLSCREEN");
    drawMenuButton(renderer, buttons[2], "QUIT");
}

void printCartridgeInfo(const Cartridge& c, const std::filesystem::path& path) {
    std::cout << "Cartridge loaded: " << path.filename().string() << '\n'
              << "Mapper: " << static_cast<int>(c.mapper)
              << (c.mapper == 0 ? " (NROM)\n" : c.mapper == 66 ? " (GxROM)\n" : "\n")
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

    std::cout << "NES Emulator started. Use the QUIT button to exit; F11 toggles fullscreen.\n";
    if (hasCartridge)
        std::cout << "Experimental CPU execution active (partial 6502 instruction set; not cycle-accurate).\n";

    std::array<std::uint32_t, NES_WIDTH * NES_HEIGHT> frame{};
    Uint32 lastCpuDiagnostic = SDL_GetTicks();
    std::uint64_t instructionsSinceDiagnostic = 0;
    bool running = true;
    bool fullscreen = false;
    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 &&
                event.key.keysym.sym == SDLK_F11) {
                const Uint32 mode = fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
                if (SDL_SetWindowFullscreen(window, mode) == 0) {
                    fullscreen = !fullscreen;
                } else {
                    std::cerr << "Fullscreen toggle failed: " << SDL_GetError() << '\n';
                }
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int windowWidth = 0, windowHeight = 0;
                SDL_GetWindowSize(window, &windowWidth, &windowHeight);
                const auto buttons = menuButtons(windowWidth, windowHeight);
                if (contains(buttons[2], event.button.x, event.button.y)) {
                    running = false;
                } else if (contains(buttons[1], event.button.x, event.button.y)) {
                    const Uint32 mode = fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
                    if (SDL_SetWindowFullscreen(window, mode) == 0) {
                        fullscreen = !fullscreen;
                    } else {
                        std::cerr << "Fullscreen toggle failed: " << SDL_GetError() << '\n';
                    }
                } else if (contains(buttons[0], event.button.x, event.button.y)) {
                const std::string selected = chooseRomFile();
                if (!selected.empty()) {
                    Cartridge nextCartridge;
                    std::string error;
                    const std::filesystem::path path(selected);
                    if (!loadCartridge(path, nextCartridge, error)) {
                        std::cerr << "ROM loading failed: " << error << '\n';
                        SDL_SetWindowTitle(window, "NES Emulator - ROM load failed");
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                            "ROM could not be loaded", error.c_str(), window);
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
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                            "ROM loaded", "The ROM file was loaded. Game compatibility is still experimental, so the screen may remain black if the game uses unsupported CPU instructions or PPU features.", window);
                    }
                }
            }
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        std::uint8_t controller1 = 0;
        if (keys[SDL_SCANCODE_Z]) controller1 |= 0x01;          // A
        if (keys[SDL_SCANCODE_X]) controller1 |= 0x02;          // B
        if (keys[SDL_SCANCODE_RSHIFT]) controller1 |= 0x04;     // Select
        if (keys[SDL_SCANCODE_RETURN]) controller1 |= 0x08;     // Start
        if (keys[SDL_SCANCODE_UP]) controller1 |= 0x10;
        if (keys[SDL_SCANCODE_DOWN]) controller1 |= 0x20;
        if (keys[SDL_SCANCODE_LEFT]) controller1 |= 0x40;
        if (keys[SDL_SCANCODE_RIGHT]) controller1 |= 0x80;
        bus.setControllerState(0, controller1);

        if (hasCartridge && !cpu.halted()) {
            for (int instruction = 0; instruction < 30000; ++instruction) {
                if (!cpu.step()) {
                    if (!cpuStopReported) {
                        std::cerr << "CPU stopped: " << cpu.error() << '\n';
                        cpuStopReported = true;
                    }
                    break;
                }
                ++instructionsSinceDiagnostic;
                bus.clockPpu();
                bus.clockPpu();
                bus.clockPpu();
            }
        }

        const Uint32 diagnosticNow = SDL_GetTicks();
        if (hasCartridge && diagnosticNow - lastCpuDiagnostic >= 2000) {
            std::cout << "CPU debug: PC=$" << std::hex << cpu.pc
                      << " next=[" << static_cast<int>(bus.read(cpu.pc)) << " "
                      << static_cast<int>(bus.read(static_cast<std::uint16_t>(cpu.pc + 1))) << " "
                      << static_cast<int>(bus.read(static_cast<std::uint16_t>(cpu.pc + 2))) << "]"
                      << " A=$" << static_cast<int>(cpu.a)
                      << " X=$" << static_cast<int>(cpu.x)
                      << " Y=$" << static_cast<int>(cpu.y)
                      << " P=$" << static_cast<int>(cpu.status)
                      << std::dec << " instructions/2s=" << instructionsSinceDiagnostic
                      << " halted=" << (cpu.halted() ? "yes" : "no") << '\n'
                      << "PPU debug: CTRL=$" << std::hex << static_cast<int>(bus.ppuControlDebug())
                      << " MASK=$" << static_cast<int>(bus.ppuMaskDebug())
                      << " STATUS=$" << static_cast<int>(bus.ppuStatusDebug())
                      << " universal-palette=$" << static_cast<int>(bus.ppuPaletteDebug())
                      << std::dec << " nametable-nonzero=" << bus.ppuNametableNonZeroDebug()
                      << "/4096 chr-nonzero=" << bus.ppuChrNonZeroDebug()
                      << "/8192 palette-nonzero=" << bus.ppuPaletteNonZeroDebug()
                      << "/32\n";
            std::array<std::uint32_t, 64> observedColors{};
            std::size_t colorCount = 0;
            std::size_t nonBlackPixels = 0;
            for (const std::uint32_t pixel : frame) {
                if ((pixel & 0x00FFFFFF) != 0) ++nonBlackPixels;
                bool known = false;
                for (std::size_t i = 0; i < colorCount; ++i) {
                    if (observedColors[i] == pixel) { known = true; break; }
                }
                if (!known && colorCount < observedColors.size())
                    observedColors[colorCount++] = pixel;
            }
            std::cout << "Frame debug: unique-colors=" << colorCount
                      << " non-black-pixels=" << nonBlackPixels << "/"
                      << frame.size() << '\n';
            instructionsSinceDiagnostic = 0;
            lastCpuDiagnostic = diagnosticNow;
        }

        bus.renderFrame(frame);
        SDL_UpdateTexture(screen, nullptr, frame.data(),
            NES_WIDTH * static_cast<int>(sizeof(std::uint32_t)));
        int outputWidth = 0, outputHeight = 0;
        SDL_GetRendererOutputSize(renderer, &outputWidth, &outputHeight);
        SDL_SetRenderDrawColor(renderer, 18, 22, 30, 255);
        SDL_RenderClear(renderer);
        const SDL_Rect gameArea = gameRectangle(outputWidth, outputHeight);
        SDL_RenderCopy(renderer, screen, nullptr, &gameArea);
        if (!hasCartridge) drawMenuButtons(renderer, outputWidth, outputHeight);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(screen);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
