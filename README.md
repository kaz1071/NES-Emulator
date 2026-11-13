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
- [ ] Accurate PPU frame timing and cycle-level CPU timing
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

The cartridge loader supports mapper 0 (NROM) with 16 or 32 KiB of PRG-ROM and mapper 66 (GxROM) with up to 128 KiB of PRG-ROM and 32 KiB of CHR-ROM. Mapper 66 bank switching is implemented for its 32 KiB PRG and 8 KiB CHR banks. NES 2.0 headers and other mappers are not supported. A ROM that loads successfully is not necessarily playable yet: CPU instructions and cycle timing are incomplete, PPU rendering/timing is approximate, and DMA and audio are not implemented. Approximate vblank and initial NMI delivery are present, along with basic keyboard input for controller 1. Commercial games are not expected to run correctly at this stage.

The emulator core is being implemented in this repository rather than wrapping an existing emulator.

## Version 0.18.0

- Added large, vertically stacked LOAD ROM and QUIT buttons centered in the window.
- Added on-screen success and error messages when selecting a ROM.
- Added initial Mapper 66 (GxROM) support with PRG/CHR bank switching.
- Added basic controller 1 support: arrows, Z, X, Right Shift, and Enter.
- Kept the menu buttons and text monochrome.
- Removed the diagnostic color grid; the PPU output is black until background rendering is enabled.
- Preserved the NES color palette for actual game graphics.


Version 0.17.0 added an approximate PPU clock (341 dots × 262 scanlines) and PPUSTATUS vblank flag. Version 0.19.0 adds the initial NMI delivery path. Neither implementation is cycle-accurate.


Version 0.18.0 updates the window controls: Escape no longer exits the emulator, a centered FULLSCREEN button toggles desktop fullscreen, F11 is a fullscreen shortcut, and QUIT remains the explicit exit control. The NES picture scales to fit the available window while preserving its aspect ratio.


## Version 0.19.0

- Added an initial PPU vblank-to-CPU NMI interrupt path.
- Added 6502 NMI entry and RTI return handling.
- NMI delivery is still built on approximate PPU timing; the CPU instruction set and cycle timing remain incomplete.
