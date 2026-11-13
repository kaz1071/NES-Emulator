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
- [x] Initial 6502 CPU registers, reset vector, RAM mirroring, and a growing starter instruction set
- [x] Initial CPU memory bus with mirrored RAM, PRG-RAM, and NROM PRG-ROM mapping
- [x] Initial PPU registers, CHR access, nametable/palette memory, and mirroring
- [x] Initial 256×240 background tile renderer and SDL texture output
- [x] Diagnostic NES-palette test pattern while the PPU background is disabled
- [ ] Scrolling, sprites, PPU timing/NMI, and full Ricoh 2A03 instruction/timing support
- [ ] Complete PPU graphics and frame timing
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

The current loader supports mapper 0 cartridges with 16 or 32 KiB of PRG-ROM and either 8 KiB of CHR-ROM or CHR-RAM. NES 2.0 headers and other mappers are not supported yet. A successfully loaded ROM is reported in the terminal, and the CPU reset vector is read from the cartridge. The CPU now accesses memory through an initial NES bus. The bus maps mirrored 2 KiB RAM, an 8 KiB PRG-RAM window, and NROM PRG-ROM. The initial PPU now handles CPU register access, CHR-ROM/CHR-RAM, nametable and palette memory, and cartridge nametable mirroring. A first static 256×240 background tile renderer now draws nametable tiles, attribute palettes, and pattern-table pixels into the SDL window. While the PPU background is disabled (the startup state until game code runs), the window displays a diagnostic pattern using the NES color palette instead of appearing blank. Scrolling, sprites, frame timing, vblank timing, and NMI are not implemented yet. The CPU is still not running game code, so this is a renderer foundation rather than playable gameplay. APU, controller, and expansion-area accesses remain placeholders; the CPU is not yet connected to a timed NES system loop.

Press **Escape** to close the window.

## Project direction

The emulator core is being implemented in this repository rather than wrapping an existing emulator. We are starting with NROM (mapper 0), then completing CPU instruction/timing support and PPU rendering, followed by controllers and audio.

## Contributions

Issues and suggestions are welcome while the project is being developed.


## Version 0.9.0

- Added basic background scrolling from PPUSCROLL writes across logical nametables.
- Added 8x8 and 8x16 sprite rendering from OAM and CHR data.
- Supports horizontal/vertical sprite flipping, transparent sprite pixels, background priority, and up to eight visible sprites.
- CPU game execution, PPU frame timing/vblank/NMI, sprite-zero hit, and full hardware accuracy are not implemented yet.
