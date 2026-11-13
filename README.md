# NES Emulator

A custom NES emulator built from scratch in C++ using SDL2, designed for Linux.

The project is in early development. The first milestone is a working SDL2 application; CPU, memory, cartridge, PPU, audio, and controller emulation will be added in stages.

## Current status

- [x] C++20 and CMake project setup
- [x] SDL2 application window
- [ ] iNES cartridge / ROM loading
- [ ] NES memory bus
- [ ] 6502 CPU (Ricoh 2A03)
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

Run it:

```bash
./build/nes-emulator
```

Press **Escape** to close the window.

## Project direction

The emulator core will be implemented in this repository rather than wrapping an existing emulator. Initial cartridge support will target the simple NROM (mapper 0) format before expanding to other mappers.

## Contributions

Issues and suggestions are welcome while the project is being developed.
