#include <SDL.h>
#include "cartridge/cartridge.hpp"
#include "cpu/cpu6502.hpp"
#include "bus/bus.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <deque>
#include <type_traits>
#include <iterator>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
struct EmulatorSnapshot {
    CPU6502::State cpu;
    NESBus::State bus;
    std::int32_t cpuCycleRemainder = 0;
};
static_assert(std::is_trivially_copyable_v<CPU6502::State>);
static_assert(std::is_trivially_copyable_v<NESBus::State>);

EmulatorSnapshot captureSnapshot(const CPU6502& cpu, const NESBus& bus,
                                 int cpuCycleRemainder) {
    return {cpu.saveState(), bus.saveState(), cpuCycleRemainder};
}

bool restoreSnapshot(const EmulatorSnapshot& snapshot, CPU6502& cpu,
                     NESBus& bus, int& cpuCycleRemainder) {
    if (!bus.loadState(snapshot.bus)) return false;
    cpu.loadState(snapshot.cpu);
    cpuCycleRemainder = snapshot.cpuCycleRemainder;
    return true;
}

bool saveStateFile(const std::filesystem::path& path, const Cartridge& cartridge,
                   const CPU6502& cpu, const NESBus& bus, int cpuCycleRemainder,
                   std::string& error) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) { error = "Could not create save-state file."; return false; }
    constexpr char magic[8] = {'N','E','S','S','A','V','E','1'};
    constexpr std::uint32_t version = 1;
    const std::uint64_t fingerprint = cartridge.romFingerprint();
    const std::uint8_t mapper = cartridge.mapper;
    const auto cpuState = cpu.saveState();
    const auto busState = bus.saveState();
    const std::int32_t remainder = cpuCycleRemainder;
    file.write(magic, sizeof(magic));
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    file.write(reinterpret_cast<const char*>(&fingerprint), sizeof(fingerprint));
    file.write(reinterpret_cast<const char*>(&mapper), sizeof(mapper));
    file.write(reinterpret_cast<const char*>(&cpuState), sizeof(cpuState));
    file.write(reinterpret_cast<const char*>(&busState), sizeof(busState));
    file.write(reinterpret_cast<const char*>(&remainder), sizeof(remainder));
    if (!file) { error = "The save-state file could not be written completely."; return false; }
    return true;
}

bool loadStateFile(const std::filesystem::path& path, const Cartridge& cartridge,
                   CPU6502& cpu, NESBus& bus, int& cpuCycleRemainder,
                   std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) { error = "Could not open save-state file."; return false; }
    char magic[8]{};
    std::uint32_t version = 0;
    std::uint64_t fingerprint = 0;
    std::uint8_t mapper = 0;
    EmulatorSnapshot snapshot{};
    file.read(magic, sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    file.read(reinterpret_cast<char*>(&fingerprint), sizeof(fingerprint));
    file.read(reinterpret_cast<char*>(&mapper), sizeof(mapper));
    file.read(reinterpret_cast<char*>(&snapshot.cpu), sizeof(snapshot.cpu));
    file.read(reinterpret_cast<char*>(&snapshot.bus), sizeof(snapshot.bus));
    file.read(reinterpret_cast<char*>(&snapshot.cpuCycleRemainder),
              sizeof(snapshot.cpuCycleRemainder));
    constexpr char expectedMagic[8] = {'N','E','S','S','A','V','E','1'};
    if (!file || !std::equal(std::begin(magic), std::end(magic), std::begin(expectedMagic))) {
        error = "This is not a valid NES Emulator save-state file.";
        return false;
    }
    if (version != 1) { error = "This save-state version is not supported."; return false; }
    if (mapper != cartridge.mapper || fingerprint != cartridge.romFingerprint()) {
        error = "This save state belongs to a different ROM. Load the matching game first.";
        return false;
    }
    if (!restoreSnapshot(snapshot, cpu, bus, cpuCycleRemainder)) {
        error = "The save state is incompatible with the currently loaded cartridge.";
        return false;
    }
    return true;
}

std::string chooseStateFile(bool save) {
    int pipeFd[2];
    if (pipe(pipeFd) != 0) return {};
    const pid_t child = fork();
    if (child < 0) { close(pipeFd[0]); close(pipeFd[1]); return {}; }
    if (child == 0) {
        close(pipeFd[0]);
        dup2(pipeFd[1], STDOUT_FILENO);
        close(pipeFd[1]);
        const std::string suggested = (std::filesystem::current_path() /
            "game.savestate").string();
        const std::string filenameArg = "--filename=" + suggested;
        char* zenitySave[] = {const_cast<char*>("zenity"),
            const_cast<char*>("--file-selection"), const_cast<char*>("--save"),
            const_cast<char*>("--confirm-overwrite"), const_cast<char*>("--file-filter=NES Save States | *.savestate"),
            const_cast<char*>("--title=Save NES State"), const_cast<char*>(filenameArg.c_str()), nullptr};
        char* zenityLoad[] = {const_cast<char*>("zenity"),
            const_cast<char*>("--file-selection"),
            const_cast<char*>("--file-filter=NES Save States | *.savestate"),
            const_cast<char*>("--title=Load NES State"), nullptr};
        execvp("zenity", save ? zenitySave : zenityLoad);
        char* kdialogSave[] = {const_cast<char*>("kdialog"),
            const_cast<char*>("--getsavefilename"), const_cast<char*>(suggested.c_str()),
            const_cast<char*>("*.savestate|NES Save States"), const_cast<char*>("--title"),
            const_cast<char*>("Save NES State"), nullptr};
        char* kdialogLoad[] = {const_cast<char*>("kdialog"),
            const_cast<char*>("--getopenfilename"), const_cast<char*>(""),
            const_cast<char*>("*.savestate|NES Save States"), const_cast<char*>("--title"),
            const_cast<char*>("Load NES State"), nullptr};
        execvp("kdialog", save ? kdialogSave : kdialogLoad);
        _exit(127);
    }
    close(pipeFd[1]);
    std::string path;
    char buffer[512];
    ssize_t bytes = 0;
    while ((bytes = read(pipeFd[0], buffer, sizeof(buffer))) > 0)
        path.append(buffer, static_cast<std::size_t>(bytes));
    close(pipeFd[0]);
    int status = 0;
    waitpid(child, &status, 0);
    while (!path.empty() && (path.back() == '\n' || path.back() == '\r'))
        path.pop_back();
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return {};
    return path;
}

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

constexpr int BUTTON_WIDTH = 460;
constexpr int BUTTON_HEIGHT = 64;
constexpr int BUTTON_GAP = 12;

std::array<SDL_Rect, 5> menuButtons(int width, int height) {
    const int x = (width - BUTTON_WIDTH) / 2;
    const int totalHeight = 5 * BUTTON_HEIGHT + 4 * BUTTON_GAP;
    const int firstY = (height - totalHeight) / 2;
    return {{
        {x, firstY, BUTTON_WIDTH, BUTTON_HEIGHT},
        {x, firstY + BUTTON_HEIGHT + BUTTON_GAP, BUTTON_WIDTH, BUTTON_HEIGHT},
        {x, firstY + 2 * (BUTTON_HEIGHT + BUTTON_GAP), BUTTON_WIDTH, BUTTON_HEIGHT},
        {x, firstY + 3 * (BUTTON_HEIGHT + BUTTON_GAP), BUTTON_WIDTH, BUTTON_HEIGHT},
        {x, firstY + 4 * (BUTTON_HEIGHT + BUTTON_GAP), BUTTON_WIDTH, BUTTON_HEIGHT}
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
        case 'P': return {{30,17,17,30,16,16,16}};
        case 'B': return {{30,17,17,30,17,17,30}};
        case 'K': return {{17,18,20,24,20,18,17}};
        case 'H': return {{17,17,17,31,17,17,17}};
        case 'V': return {{17,17,17,17,17,10,4}};
        case 'W': return {{17,17,17,21,21,21,10}};
        case '2': return {{14,17,1,2,4,8,31}};
        case '3': return {{30,1,1,14,1,1,30}};
        case '+': return {{0,4,4,31,4,4,0}};
        case 'Z': return {{31,1,2,4,8,16,31}};
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
    for (char ch : label) {
        const auto glyph = glyphFor(ch);
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
    drawMenuButton(renderer, buttons[1], "PRELOADED ROMS");
    drawMenuButton(renderer, buttons[2], "DOWNLOAD ROMS");
    drawMenuButton(renderer, buttons[3], "FULLSCREEN");
    drawMenuButton(renderer, buttons[4], "QUIT");
}

using PreloadedRom = std::pair<std::string, std::filesystem::path>;

std::vector<PreloadedRom> findPreloadedRoms() {
    const auto folder = std::filesystem::current_path() / "roms";
    const std::array<std::pair<const char*, const char*>, 5> games{{
        {"ZELDA", "Legend of Zelda, The (USA) (Rev 1).nes"},
        {"MARIO DUCK HUNT", "Super Mario Bros. + Duck Hunt (USA).nes"},
        {"MARIO BROS 2", "Super Mario Bros. 2 (USA) (Rev 1).nes"},
        {"LOST LEVELS", "Super Mario Bros 2 (Lost Levels) (Unl).nes"},
        {"MARIO BROS 3", "Super Mario Bros. 3 (USA) (Rev 1).nes"}
    }};
    std::vector<PreloadedRom> found;
    for (const auto& game : games) {
        const auto path = folder / game.second;
        std::error_code error;
        if (std::filesystem::is_regular_file(path, error))
            found.emplace_back(game.first, path);
    }
    return found;
}

std::vector<SDL_Rect> preloadedMenuButtons(int width, int height,
                                           std::size_t gameCount) {
    constexpr int buttonWidth = 600, buttonHeight = 56, gap = 10;
    const std::size_t rows = std::max<std::size_t>(1, gameCount) + 1;
    const int totalHeight = static_cast<int>(rows) * buttonHeight
                          + static_cast<int>(rows - 1) * gap;
    const int x = (width - buttonWidth) / 2;
    const int firstY = (height - totalHeight) / 2;
    std::vector<SDL_Rect> buttons;
    buttons.reserve(rows);
    for (std::size_t i = 0; i < rows; ++i)
        buttons.push_back({x, firstY + static_cast<int>(i) * (buttonHeight + gap),
                           buttonWidth, buttonHeight});
    return buttons;
}

void drawPreloadedMenu(SDL_Renderer* renderer, int width, int height,
                       const std::vector<PreloadedRom>& roms) {
    const auto buttons = preloadedMenuButtons(width, height, roms.size());
    if (roms.empty()) {
        drawMenuButton(renderer, buttons[0], "NO ROMS FOUND");
    } else {
        for (std::size_t i = 0; i < roms.size(); ++i)
            drawMenuButton(renderer, buttons[i], roms[i].first);
    }
    drawMenuButton(renderer, buttons.back(), "BACK");
}

using DownloadRom = std::pair<std::string, std::string>;

const std::vector<DownloadRom>& downloadRomLinks() {
    static const std::vector<DownloadRom> links{
        {"MARIO BROS 3", "https://www.romsgames.net/nintendo-rom-super-mario-bros-3/"},
        {"ZELDA", "https://www.romsgames.net/nintendo-rom-legend-of-zelda-the/"},
        {"MARIO BROS 2", "https://www.romsgames.net/nintendo-rom-super-mario-bros-2/"},
        {"DUCK HUNT", "https://www.romsgames.net/nintendo-rom-super-mario-bros-duck-hunt/"},
        {"LOST LEVELS", "https://www.emu-land.net/en/consoles/dendy/roms/super-mario-bros-2-j"}
    };
    return links;
}

std::vector<SDL_Rect> downloadMenuButtons(int width, int height) {
    constexpr int buttonWidth = 600, buttonHeight = 56, gap = 10;
    const std::size_t rows = downloadRomLinks().size() + 1;
    const int totalHeight = static_cast<int>(rows) * buttonHeight
                          + static_cast<int>(rows - 1) * gap;
    const int x = (width - buttonWidth) / 2;
    const int firstY = (height - totalHeight) / 2;
    std::vector<SDL_Rect> buttons;
    buttons.reserve(rows);
    for (std::size_t i = 0; i < rows; ++i)
        buttons.push_back({x, firstY + static_cast<int>(i) * (buttonHeight + gap),
                           buttonWidth, buttonHeight});
    return buttons;
}

void drawDownloadMenu(SDL_Renderer* renderer, int width, int height) {
    const auto buttons = downloadMenuButtons(width, height);
    const auto& links = downloadRomLinks();
    for (std::size_t i = 0; i < links.size(); ++i)
        drawMenuButton(renderer, buttons[i], links[i].first);
    drawMenuButton(renderer, buttons.back(), "BACK");
}

void openWebsite(const std::string& url) {
    const pid_t child = fork();
    if (child < 0) {
        std::cerr << "Could not open website: failed to start browser.\\n";
        return;
    }
    if (child == 0) {
        execlp("xdg-open", "xdg-open", url.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    waitpid(child, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        std::cerr << "Could not open website. Make sure xdg-open and a browser are installed.\\n";
}

void printCartridgeInfo(const Cartridge& c, const std::filesystem::path& path) {
    std::cout << "Cartridge loaded: " << path.filename().string() << '\n'
              << "Mapper: " << static_cast<int>(c.mapper)
              << (c.mapper == 0 ? " (NROM)\n" : c.mapper == 1 ? " (MMC1)\n" :
                  c.mapper == 4 ? " (MMC3)\n" : c.mapper == 40 ? " (NTDEC 2722)\n" : c.mapper == 66 ? " (GxROM)\n" : "\n")
              << "PRG-ROM: " << c.prgRom.size()/1024 << " KiB\n"
              << "CHR: " << (c.usesChrRam ? std::to_string(c.chrData.size()/1024) + " KiB CHR-RAM" :
                  std::to_string(c.chrData.size()/1024) + " KiB CHR-ROM") << '\n'
              << "Mirroring: " << mirroringName(c.mirroring) << '\n';
}
}

int main(int argc, char** argv) {
    bool showDebugOutput = false;
    std::cout << "Show emulator debug output? [y/N]: " << std::flush;
    std::string debugChoice;
    std::getline(std::cin, debugChoice);
    showDebugOutput = !debugChoice.empty() &&
        (debugChoice[0] == 'y' || debugChoice[0] == 'Y');

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
        std::cout << "No ROM loaded. Choose LOAD ROM or PRELOADED ROMS to start a game.\n";
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

    std::cout << "NES Emulator started. Use QUIT to exit; F11 toggles fullscreen; F5 saves a state; F8 loads a state; hold R to rewind.\n";
    if (hasCartridge)
        std::cout << "Experimental CPU execution active (official 6502 opcodes supported; instruction-cycle timing enabled, not cycle-perfect).\n";

    std::array<std::uint32_t, NES_WIDTH * NES_HEIGHT> frame{};
    Uint32 lastCpuDiagnostic = SDL_GetTicks();
    std::uint64_t instructionsSinceDiagnostic = 0;

    // An NTSC frame is 89,342 PPU dots. CPU bus accesses now advance
    // the PPU by three dots each; carry the fractional frame remainder
    // so the target CPU budget alternates between 29,780 and 29,781 cycles.
    int cpuCycleRemainder = 0;

    // Pace the emulator at approximately 60 frames per second.
    const Uint64 frameFrequency = SDL_GetPerformanceFrequency();
    const Uint64 frameDuration = frameFrequency / 60;
    Uint64 nextFrameTime = SDL_GetPerformanceCounter();

    bool running = true;
    bool fullscreen = false;
    bool showPreloadedMenu = false;
    bool showDownloadMenu = false;
    std::vector<PreloadedRom> preloadedRoms;
    std::deque<EmulatorSnapshot> rewindHistory;
    int rewindCaptureDivider = 0;
    constexpr std::size_t MAX_REWIND_SNAPSHOTS = 90;
    if (hasCartridge) rewindHistory.push_back(captureSnapshot(cpu, bus, cpuCycleRemainder));
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

            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && hasCartridge &&
                (event.key.keysym.sym == SDLK_F5 || event.key.keysym.sym == SDLK_F8)) {
                const bool saving = event.key.keysym.sym == SDLK_F5;
                const std::string selectedPath = chooseStateFile(saving);
                if (!selectedPath.empty()) {
                    std::string stateError;
                    const bool success = saving
                        ? saveStateFile(selectedPath, cartridge, cpu, bus,
                                        cpuCycleRemainder, stateError)
                        : loadStateFile(selectedPath, cartridge, cpu, bus,
                                        cpuCycleRemainder, stateError);
                    if (success) {
                        if (!saving) {
                            rewindHistory.clear();
                            rewindHistory.push_back(captureSnapshot(cpu, bus, cpuCycleRemainder));
                            rewindCaptureDivider = 0;
                        }
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                            saving ? "Save state created" : "Save state loaded",
                            saving ? "Your game state was saved to the selected file."
                                   : "Your game has been restored from the selected file.",
                            window);
                    } else {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                            "Save state error", stateError.c_str(), window);
                    }
                }
            }
            if (event.type == SDL_MOUSEBUTTONDOWN &&
                event.button.button == SDL_BUTTON_LEFT) {
                int windowWidth = 0, windowHeight = 0;
                SDL_GetWindowSize(window, &windowWidth, &windowHeight);
                std::string selectedRom;

                if (showDownloadMenu) {
                    const auto buttons = downloadMenuButtons(windowWidth, windowHeight);
                    if (contains(buttons.back(), event.button.x, event.button.y)) {
                        showDownloadMenu = false;
                    } else {
                        const auto& links = downloadRomLinks();
                        for (std::size_t i = 0; i < links.size(); ++i) {
                            if (contains(buttons[i], event.button.x, event.button.y)) {
                                openWebsite(links[i].second);
                                break;
                            }
                        }
                    }
                } else if (showPreloadedMenu) {
                    const auto buttons = preloadedMenuButtons(
                        windowWidth, windowHeight, preloadedRoms.size());
                    if (contains(buttons.back(), event.button.x, event.button.y)) {
                        showPreloadedMenu = false;
                    } else if (!preloadedRoms.empty()) {
                        for (std::size_t i = 0; i < preloadedRoms.size(); ++i) {
                            if (contains(buttons[i], event.button.x, event.button.y)) {
                                selectedRom = preloadedRoms[i].second.string();
                                break;
                            }
                        }
                    }
                } else {
                    const auto buttons = menuButtons(windowWidth, windowHeight);
                    if (contains(buttons[4], event.button.x, event.button.y)) {
                        running = false;
                    } else if (contains(buttons[3], event.button.x, event.button.y)) {
                        const Uint32 mode = fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
                        if (SDL_SetWindowFullscreen(window, mode) == 0)
                            fullscreen = !fullscreen;
                        else
                            std::cerr << "Fullscreen toggle failed: " << SDL_GetError() << '\n';
                    } else if (contains(buttons[2], event.button.x, event.button.y)) {
                        showDownloadMenu = true;
                    } else if (contains(buttons[1], event.button.x, event.button.y)) {
                        preloadedRoms = findPreloadedRoms();
                        showPreloadedMenu = true;
                    } else if (contains(buttons[0], event.button.x, event.button.y)) {
                        selectedRom = chooseRomFile();
                    }
                }

                if (!selectedRom.empty()) {
                    Cartridge nextCartridge;
                    std::string error;
                    const std::filesystem::path path(selectedRom);
                    if (!loadCartridge(path, nextCartridge, error)) {
                        std::cerr << "ROM loading failed: " << error << '\n';
                        SDL_SetWindowTitle(window, "NES Emulator - ROM load failed");
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                            "ROM could not be loaded", error.c_str(), window);
                    } else {
                        cartridge = std::move(nextCartridge);
                        hasCartridge = true;
                        showPreloadedMenu = false;
                        cpuStopReported = false;
                        bus.connectCartridge(&cartridge);
                        bus.reset();
                        cpu.connectBus(&bus);
                        cpu.reset();
                        cpuCycleRemainder = 0;
                        rewindHistory.clear();
                        rewindHistory.push_back(captureSnapshot(cpu, bus, cpuCycleRemainder));
                        rewindCaptureDivider = 0;
                        windowTitle = "NES Emulator - " + path.filename().string();
                        SDL_SetWindowTitle(window, windowTitle.c_str());
                        printCartridgeInfo(cartridge, path);
                        std::cout << "CPU reset complete. 6502 reset vector: $" << std::hex
                                  << cpu.pc << std::dec << '\n'
                                  << "Experimental CPU execution active (official 6502 opcodes supported; instruction-cycle timing enabled, not cycle-perfect).\n";
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                            "ROM loaded", "The ROM file was loaded. Game compatibility is still experimental, so the screen may remain black if the game uses unsupported CPU instructions or PPU features.", window);
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

        const bool rewindHeld = keys[SDL_SCANCODE_R] != 0;
        const bool rewinding = hasCartridge && rewindHeld;
        if (rewinding && rewindHistory.size() > 1) {
            rewindHistory.pop_back();
            if (!restoreSnapshot(rewindHistory.back(), cpu, bus, cpuCycleRemainder)) {
                rewindHistory.clear();
                rewindHistory.push_back(captureSnapshot(cpu, bus, cpuCycleRemainder));
            }
        }

        if (hasCartridge && !cpu.halted() && !rewinding) {
            cpuCycleRemainder += 89342;
            const int cpuCyclesTarget = cpuCycleRemainder / 3;
            cpuCycleRemainder %= 3;
            int cpuCyclesThisFrame = 0;
            while (cpuCyclesThisFrame < cpuCyclesTarget && !cpu.halted()) {
                if (!cpu.step()) {
                    if (!cpuStopReported) {
                        std::cerr << "CPU stopped: " << cpu.error() << '\n';
                        cpuStopReported = true;
                    }
                    break;
                }
                ++instructionsSinceDiagnostic;
                const int instructionCycles = cpu.lastStepCycles();
                cpuCyclesThisFrame += instructionCycles;
            }
        }

        if (hasCartridge && !rewinding && !cpu.halted()) {
            if (++rewindCaptureDivider >= 2) {
                rewindHistory.push_back(captureSnapshot(cpu, bus, cpuCycleRemainder));
                rewindCaptureDivider = 0;
                while (rewindHistory.size() > MAX_REWIND_SNAPSHOTS)
                    rewindHistory.pop_front();
            }
        }

        bus.renderFrame(frame);

        const Uint32 diagnosticNow = SDL_GetTicks();
        if (showDebugOutput && hasCartridge &&
            diagnosticNow - lastCpuDiagnostic >= 2000) {
            // Keep optional diagnostics compact so terminal output cannot overwhelm
            // the desktop/browser. Show CPU state plus visible OAM sprites only.
            std::cout << "CPU: PC=$" << std::hex << cpu.pc
                      << " A=$" << static_cast<int>(cpu.a)
                      << " X=$" << static_cast<int>(cpu.x)
                      << " Y=$" << static_cast<int>(cpu.y)
                      << std::dec << " instructions/2s=" << instructionsSinceDiagnostic
                      << " | PPU: " << bus.ppuScanlineDebug() << ":"
                      << bus.ppuCycleDebug()
                      << " CTRL=$" << std::hex << static_cast<int>(bus.ppuControlDebug())
                      << " MASK=$" << static_cast<int>(bus.ppuMaskDebug())
                      << std::dec << '\n';

            const auto& oam = bus.ppuOamDebug();
            std::cout << "Visible sprites (index:y/tile/attr/x; B=behind):";
            for (std::size_t s = 0; s < 64; ++s) {
                const std::uint8_t sy = oam[s * 4];
                const std::uint8_t tile = oam[s * 4 + 1];
                const std::uint8_t attr = oam[s * 4 + 2];
                const std::uint8_t sx = oam[s * 4 + 3];
                if (sy >= 0xEF ||
                    (sy == 0 && tile == 0 && attr == 0 && sx == 0)) continue;
                std::cout << " [" << std::dec << s << ":$" << std::hex
                          << static_cast<int>(sy) << "/$" << static_cast<int>(tile)
                          << "/$" << static_cast<int>(attr) << "/$" << static_cast<int>(sx)
                          << ((attr & 0x20) ? ",B" : "") << "]";
            }
            std::cout << std::dec << '\n';
            instructionsSinceDiagnostic = 0;
            lastCpuDiagnostic = diagnosticNow;
        }

        SDL_UpdateTexture(screen, nullptr, frame.data(),
            NES_WIDTH * static_cast<int>(sizeof(std::uint32_t)));
        int outputWidth = 0, outputHeight = 0;
        SDL_GetRendererOutputSize(renderer, &outputWidth, &outputHeight);
        SDL_SetRenderDrawColor(renderer, 18, 22, 30, 255);
        SDL_RenderClear(renderer);
        const SDL_Rect gameArea = gameRectangle(outputWidth, outputHeight);
        SDL_RenderCopy(renderer, screen, nullptr, &gameArea);
        if (!hasCartridge) {
            if (showDownloadMenu)
                drawDownloadMenu(renderer, outputWidth, outputHeight);
            else if (showPreloadedMenu)
                drawPreloadedMenu(renderer, outputWidth, outputHeight, preloadedRoms);
            else
                drawMenuButtons(renderer, outputWidth, outputHeight);
        }
        SDL_RenderPresent(renderer);

        // Keep game time close to real NTSC speed when the host can keep up.
        nextFrameTime += frameDuration;
        Uint64 now = SDL_GetPerformanceCounter();
        if (now < nextFrameTime) {
            const Uint64 remaining = nextFrameTime - now;
            const Uint32 delayMs = static_cast<Uint32>(
                (remaining * 1000) / frameFrequency);
            if (delayMs > 0) SDL_Delay(delayMs);
            while (SDL_GetPerformanceCounter() < nextFrameTime) {
                SDL_Delay(0);
            }
        } else {
            // Do not accumulate an ever-growing catch-up backlog on slow hosts.
            nextFrameTime = now;
        }
    }

    SDL_DestroyTexture(screen);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
