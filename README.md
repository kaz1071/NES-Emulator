# NES Emulator

A custom NES emulator built from scratch in C++ using SDL2, designed for Linux.

The project is in early development. It currently has an SDL2 application, an iNES cartridge loader for NROM (mapper 0), and the first 6502 CPU foundation. CPU execution is not connected to the emulation loop yet.

## Current status

- [x] C++20 and CMake project setup
- [x] SDL2 application window
- [x] Standard iNES header parsing
- [x] NROM (mapper 0) PRG-ROM loading
- [x] CHR-ROM loading and CHR-RAM allocation
- [x] Trainer offset and mirroring detection
- [x] Initial 6502 CPU registers, reset vector, RAM mirroring, and a small starter set of instructions
- [ ] Complete NES memory bus and full Ricoh 2A03 instruction/timing support
- [ ] PPU graphics
- [ ] Controller input
- [ ] APU audio
- [ ] Linux release build

## Build on Linux

Install the build tools and SDL2 development package:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libsdl2-dev
```

Clone the repository:

```bash
git clone https://github.com/kaz1071/NES-Emulator.git
cd NES-Emulator
```

Configure and compile:

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

Run the empty emulator window:

```bash
./build/nes-emulator
```

Load a compatible standard iNES NROM ROM:

```bash
./build/nes-emulator /path/to/game.nes
```

The current loader supports mapper 0 cartridges with 16 or 32 KiB of PRG-ROM and either 8 KiB of CHR-ROM or CHR-RAM. NES 2.0 headers and other mappers are not supported yet. A successfully loaded ROM is reported in the terminal, and the CPU reset vector is read from the cartridge. The CPU currently implements only a small starter set of instructions and does not run the game yet; graphics are not implemented.

Press **Escape** to close the window.

## Project direction

The emulator core is being implemented in this repository rather than wrapping an existing emulator. We are starting with NROM (mapper 0), then completing CPU instruction/timing support, the memory bus, PPU, controllers, and audio.

## Contributions

Issues and suggestions are welcome while the project is being developed.
