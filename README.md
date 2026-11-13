# NES Emulator

A custom NES emulator built from scratch in C++ using SDL2, designed for Linux.

The project is in early development. It currently includes an SDL2 application, a ROM file picker, an iNES cartridge loader for NROM (mapper 0) and GxROM (mapper 66), an experimental partial 6502 CPU, and an initial PPU renderer.

## Current status

- [x] C++20 and CMake project setup
- [x] SDL2 application window
- [x] Large centered **LOAD ROM** and **QUIT** buttons
- [x] Linux desktop file picker
- [x] Standard iNES header parsing
- [x] NROM (mapper 0) PRG-ROM loading
- [x] GxROM (mapper 66) bank switching for up to 128 KiB PRG-ROM and 32 KiB CHR-ROM
- [x] CHR-ROM loading and CHR-RAM allocation
- [x] Trainer offset and mirroring detection
- [x] Growing partial 6502 CPU instruction set
- [x] Initial NES memory bus
- [x] PPU registers, CHR access, nametable/palette memory, and mirroring
- [x] 256×240 background renderer, basic scrolling, and sprites
- [x] Black screen instead of diagnostic color grid when the PPU background is disabled
- [x] Experimental CPU instruction execution loop
- [ ] Full 6502 instruction and cycle support
- [ ] PPU frame timing, vblank, and NMI
- [x] Basic NES controller 1 input (keyboard-mapped)
- [ ] APU audio
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

Click the large centered **LOAD ROM** button and choose a `.nes` file. Click **QUIT** to close the window; Escape also works. The file picker uses `zenity`, or `kdialog` if that is installed instead. If neither is installed, install Zenity with `sudo apt install zenity`. You can still launch a ROM directly from the terminal:

```bash
./build/nes-emulator /path/to/game.nes
```

## Compatibility notes

The cartridge loader supports mapper 0 (NROM) with 16 or 32 KiB of PRG-ROM and mapper 66 (GxROM) with up to 128 KiB of PRG-ROM and 32 KiB of CHR-ROM. Mapper 66 bank switching is implemented for its 32 KiB PRG and 8 KiB CHR banks. NES 2.0 headers and other mappers are not supported. A ROM that loads successfully is not necessarily playable yet: CPU instructions are incomplete, CPU pacing is not cycle-accurate, and PPU timing/vblank/NMI, DMA, and audio are not implemented. Controller 1 has basic keyboard input. Commercial games are not expected to run correctly at this stage.

The emulator core is being implemented in this repository rather than wrapping an existing emulator.

Press **Escape** to close the window.

## Version 0.17.0

- Added large, vertically stacked LOAD ROM and QUIT buttons centered in the window.
- Added on-screen success and error messages when selecting a ROM.
- Added initial Mapper 66 (GxROM) support with PRG/CHR bank switching.
- Added basic controller 1 support: arrows, Z, X, Right Shift, and Enter.
- Kept the menu buttons and text monochrome.
- Removed the diagnostic color grid; the PPU output is black until background rendering is enabled.
- Preserved the NES color palette for actual game graphics.


Version 0.17.0 adds an approximate PPU clock (341 dots × 262 scanlines) and updates the PPUSTATUS vblank flag, which allows games that poll vblank to make progress. This is not cycle-accurate and NMI delivery is not implemented yet.
