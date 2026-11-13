# NES Emulator

A custom NES emulator built from scratch in C++ using SDL2, designed for Linux.

The project is in early development. It currently includes an SDL2 application, a ROM file picker, an iNES cartridge loader for NROM (mapper 0), an experimental partial 6502 CPU, and an initial PPU renderer.

## Current status

- [x] C++20 and CMake project setup
- [x] SDL2 application window
- [x] In-window **LOAD ROM** button using the Linux desktop file picker
- [x] Standard iNES header parsing
- [x] NROM (mapper 0) PRG-ROM loading
- [x] CHR-ROM loading and CHR-RAM allocation
- [x] Trainer offset and mirroring detection
- [x] Growing partial 6502 CPU instruction set
- [x] Initial NES memory bus
- [x] PPU registers, CHR access, nametable/palette memory, and mirroring
- [x] 256×240 background renderer, basic scrolling, and sprites
- [x] Experimental CPU instruction execution loop
- [ ] Full 6502 instruction and cycle support
- [ ] PPU frame timing, vblank, and NMI
- [ ] Controller input and APU audio
- [ ] Additional cartridge mappers

## Build on Linux

Install the build tools, SDL2 development package, and a file-picker utility:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libsdl2-dev zenity
```

Clone and build:

```bash
git clone https://github.com/kaz1071/NES-Emulator.git
cd NES-Emulator
cmake -S . -B build
cmake --build build -j$(nproc)
```

Start the emulator:

```bash
./build/nes-emulator
```

Click **LOAD ROM** in the top-left corner and choose a `.nes` file. The file picker uses `zenity`, or `kdialog` if that is installed instead. If neither is installed, install Zenity with `sudo apt install zenity`. You can still launch a ROM directly from the terminal:

```bash
./build/nes-emulator /path/to/game.nes
```

## Compatibility notes

The cartridge loader currently supports mapper 0 (NROM) with 16 or 32 KiB of PRG-ROM and either 8 KiB of CHR-ROM or CHR-RAM. NES 2.0 headers and other mappers are not supported. A ROM that loads successfully is not necessarily playable yet: CPU instructions are incomplete, CPU pacing is not cycle-accurate, and PPU timing/vblank/NMI, controllers, DMA, and audio are not implemented. Commercial games are not expected to run correctly at this stage.

The emulator core is being implemented in this repository rather than wrapping an existing emulator.

Press **Escape** to close the window.

## Version 0.11.0

- Added an in-window LOAD ROM button.
- Opens the Linux desktop file chooser through Zenity, with KDialog as a fallback.
- Loads a selected compatible ROM into the running emulator and resets the bus and CPU.
- Retains command-line ROM loading.
