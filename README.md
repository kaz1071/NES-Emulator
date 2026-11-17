# PixelCore - NES

A custom NES emulator built from scratch in C++ using SDL2, designed for Linux. PixelCore is the umbrella name for a planned family of emulators.

The project is in early development. It currently includes an SDL2 application, a ROM file picker, an iNES cartridge loader for NROM (mapper 0), MMC1 (mapper 1), UxROM (mapper 2), CNROM (mapper 3), MMC3 (mapper 4), AxROM (mapper 7), NTDEC 2722 (mapper 40), MMC5 (mapper 5, PRG/CHR banking), MMC2 (mapper 9), MMC4 (mapper 10), Color Dreams (mapper 11), CPROM (mapper 13), Camerica (mapper 71), UN1ROM (mapper 94), 100-in-1 Contra Function 16 (mapper 15), and GxROM (mapper 66), an experimental 6502 CPU, and a scanline-based PPU renderer.


### Mapper 15 — 100-in-1 Contra Function 16 (0.56.0)

- Adds all four documented PRG banking modes (32 KiB, UNROM-style 16 KiB, 8 KiB, and 16 KiB).
- Implements switchable vertical/horizontal mirroring and mode-dependent CHR-RAM write protection.
- Requires CHR-RAM and supports 32 KiB to 1 MiB PRG-ROM; mapper registers are preserved in save states.
- Includes regression tests for each banking mode, mirroring, CHR protection, and state restoration.

### UN1ROM mapper 94 (0.55.0)

- Adds switchable 16 KiB PRG-ROM banking with the final 16 KiB bank fixed.
- Decodes the UN1ROM bank register from bits 2-4 and preserves it in save states.
- Supports 32 to 128 KiB PRG-ROM and includes regression tests.

### Camerica mapper 71 (0.54.0)

- Adds switchable 16 KiB PRG-ROM at `$8000-$BFFF` with the final 16 KiB bank fixed at `$C000-$FFFF`.
- Supports up to 512 KiB PRG-ROM and preserves the selected bank in save states.
- Leaves CHR-ROM/CHR-RAM and header mirroring unchanged.

### CPROM mapper 13 (0.53.0)

- Adds fixed 32 KiB PRG-ROM support and 16 KiB CHR-RAM allocation.
- Keeps the lower 4 KiB CHR window fixed and switches the upper 4 KiB window with the mapper register.
- Includes CHR-RAM save-state coverage and regression tests.

### MMC4 mapper 10, MMC2 mapper 9, and Color Dreams mapper 11 (0.52.0)

- Adds MMC4 16 KiB PRG banking, fixed upper PRG bank, latch-controlled 4 KiB CHR banks, mirroring control, and save-state support.
- Adds MMC2 PRG banking, four 4 KiB CHR bank registers, FD/FE latch switching, mirroring control, and save-state support.
- Includes automated MMC2/MMC4 bank, latch, and state tests.

- Implements all four MMC5 PRG banking modes selected by `$5100`: 32 KiB, 16 KiB, mixed 16/8 KiB, and four independent 8 KiB windows.
- Supports banked PRG-RAM at `$6000-$7FFF` through `$5113`, plus RAM-selected windows in the upper PRG area.
- Expands MMC5 PRG-RAM storage to 1 MiB and preserves it in save states.
- Enforces the `$5102/$5103` PRG-RAM write-protection sequence.
- Implements the MMC5 8 KiB, 4 KiB, 2 KiB, and 1 KiB CHR banking modes.
- Separates background CHR registers (`$5128-$512B`) from sprite CHR registers (`$5120-$5127`).
- Implements the hardware multiplier at `$5205/$5206`, including save-state support.
- Adds two MMC5 expansion pulse channels plus PCM DAC output, PRG-read PCM mode, zero-byte PCM IRQ/status, and save-state support.
- Corrects ExRAM CPU permissions for all `$5104` modes: modes 0/1 are rendering-time write-only, mode 2 is read/write, and mode 3 is read-only.
- Adds regression tests for PRG/CHR banking, banked RAM, write protection, multiplier results, and save-state restoration.

### Color Dreams mapper 11 (0.50.0)

- Adds 32 KiB PRG-ROM bank switching and 8 KiB CHR-ROM banking.
- Supports CHR-RAM cartridges and preserves bank selection in save states.
- Adds regression coverage for PRG/CHR selection and state restoration.

## Current status

- [x] C++20 and CMake project setup
- [x] SDL2 application window
- [x] Large centered **LOAD ROM** and **QUIT** buttons
- [x] Linux desktop file picker
- [x] Local preloaded-ROM menu (ROM files stay outside GitHub)
- [x] Download menu that opens user-provided game pages in the desktop browser
- [x] Binary save states for CPU, RAM, PPU, controller, and mapper state
- [x] Short rolling rewind history with a hold-to-rewind keyboard control
- [x] Standard iNES header parsing
- [x] NROM (mapper 0) PRG-ROM loading
- [x] MMC1 (mapper 1) serial registers, PRG/CHR banking, and mirroring
- [x] UxROM (mapper 2) switchable 16 KiB PRG bank with fixed final bank
- [x] CNROM (mapper 3) fixed PRG-ROM with switchable 8 KiB CHR-ROM banks
- [x] AxROM (mapper 7) switchable 32 KiB PRG-ROM banks and one-screen mirroring control
- [x] MMC3 (mapper 4) PRG/CHR banking, mirroring, PRG-RAM protection, and approximate scanline IRQ
- [x] MMC5 (mapper 5) PRG/CHR banking modes, protected cartridge RAM, and save-state support
- [x] MMC5 vertical split-screen rendering and approximate scanline IRQ, including scanline-zero compares
- [x] Automated MMC5 IRQ regression tests (scanline compare, status acknowledgement, enable masking, and frame reset)
- [x] Mapper 40 fixed/switchable PRG banks and CPU-cycle IRQ
- [x] MMC2 (mapper 9) and MMC4 (mapper 10) latch-controlled CHR banking
- [x] Color Dreams (mapper 11) 32 KiB PRG and 8 KiB CHR bank switching
- [x] CPROM (mapper 13) 32 KiB PRG and switchable 4 KiB CHR-RAM window
- [x] Camerica (mapper 71) switchable 16 KiB PRG bank with fixed final bank
- [x] UN1ROM (mapper 94) switchable 16 KiB PRG bank with fixed final bank
- [x] Mapper 15 multicart PRG banking modes, mirroring, and CHR-RAM protection
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
- [x] Initial NES APU audio: pulse, triangle, and noise channels mixed to SDL2 output
- [x] Initial DMC sample playback, looping, and IRQ support
- [x] DMC sample-fetch DMA with approximate 3/4-cycle CPU stalls
- [x] Delayed frame-sequencer reset after `$4017` writes, with immediate IRQ inhibit
- [ ] Cycle-accurate DMC DMA bus sequencing and APU timing
- [ ] Additional cartridge mappers (AxROM and others)

## Build on Linux

Install the build tools, SDL2 development package, and a file-picker utility:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libsdl2-dev zenity
```

Clone and build:

```bash
git clone https://github.com/kaz1071/PixelCore---NES.git
cd PixelCore---NES
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
cmake --install build
```

Start the emulator from the Chromebook/Linux app launcher by searching for **PixelCore - NES**. You can also start it from the terminal:

```bash
~/.local/bin/nes-emulator
```

The install step adds a pixel-art controller icon and a desktop launcher that points to the executable's configured install location. The path is generated from the install prefix on each build, so it is not tied to the developer's home directory. The window title is **PixelCore - NES**.

Click **LOAD ROM** to choose a `.nes` file, choose **DOWNLOAD ROMS** to open one of the linked game pages in your web browser, or place the five supported local game files in `~/.local/share/pixelcore-nes/roms/` and choose **PRELOADED ROMS**. You can override this folder with the `PIXELCORE_NES_ROM_DIR` environment variable. The app also checks the installed shared-data folder and the `roms/` folder in a source checkout, without depending on the directory it was launched from. While a game is running, press **5** to choose a file and save a `.savestate`, press **8** to load a save-state for the currently loaded ROM, or hold **R** to rewind. Press **6** to cycle the rewind buffer through 3, 5, 10, and 15 seconds. The default is 10 seconds. The buffer fills as you play and uses additional memory for longer durations; it is temporary and is not a substitute for a saved state. The preloaded list is ordered as Zelda, Super Mario Bros. + Duck Hunt, Super Mario Bros. 2, The Lost Levels, and Super Mario Bros. 3. The `roms/` folder is ignored by Git, so ROM files are not uploaded to the public repository. Click **QUIT** to close the window; Escape also works. The file picker uses `zenity`, or `kdialog` if that is installed instead. If neither is installed, install Zenity with `sudo apt install zenity`. You can still launch a ROM directly from the terminal:

```bash
./build/nes-emulator /path/to/game.nes
```

## Compatibility notes

The cartridge loader currently recognizes mapper 0 (NROM), mapper 1 (MMC1), mapper 2 (UxROM), mapper 3 (CNROM), mapper 4 (MMC3), mapper 5 (MMC5 initial banking), mapper 7 (AxROM), mapper 9 (MMC2), mapper 10 (MMC4), mapper 11 (Color Dreams), mapper 13 (CPROM), mapper 40 (NTDEC 2722), mapper 66 (GxROM), mapper 71 (Camerica), mapper 94 (UN1ROM), and mapper 15 (100-in-1 Contra Function 16). MMC2 and MMC4 use FD/FE PPU address latches to switch paired 4 KiB CHR banks; MMC4 additionally switches a 16 KiB PRG window. MMC1 includes serial register writes, PRG/CHR banking, and one-screen mirroring. MMC3 includes 8 KiB PRG banking, 1 KiB CHR banking, mirroring control, PRG-RAM protection, and an approximate scanline IRQ path. Mapper 40 includes its fixed PRG windows, switchable 8 KiB bank, and approximate 4,096-CPU-cycle IRQ. GxROM includes 32 KiB PRG and 8 KiB CHR bank switching. Color Dreams mapper 11 selects among 32 KiB PRG and 8 KiB CHR banks. CPROM mapper 13 uses 32 KiB fixed PRG-ROM, 16 KiB CHR-RAM, and a switchable upper 4 KiB CHR window. Camerica mapper 71 uses a switchable lower 16 KiB PRG window and a fixed final 16 KiB upper window; board-specific mirroring variants are not yet implemented. UN1ROM mapper 94 uses bits 2-4 of writes to select its lower 16 KiB PRG bank and fixes the final bank in the upper window. Mapper 15 implements the four documented PRG banking modes, data-bit mirroring control, and CHR-RAM write protection in modes 0 and 3. UxROM switches the lower 16 KiB PRG window while keeping the final PRG bank fixed in the upper window. CNROM keeps 16 or 32 KiB of PRG-ROM fixed and switches among up to four 8 KiB CHR-ROM banks; CHR-RAM cartridges remain supported without CHR banking. AxROM switches among 32 KiB PRG-ROM banks and selects lower or upper single-screen mirroring; this implementation expects CHR-RAM. MMC5 includes initial PRG/CHR banking, protected PRG-RAM, ExRAM/extended-attribute support, fill and vertical-split rendering, and an approximate scanline IRQ. MMC5 PCM read-mode timing and cycle-accurate MMC5 timing remain approximate. NES 2.0 headers and other mappers are not supported. A ROM that loads successfully is not necessarily fully playable: CPU instruction timing is not cycle-accurate, PPU rendering and MMC3 IRQ timing are approximate, OAM DMA stalls are approximate, APU timing is approximate, and DMC DMA uses an approximate 3/4-cycle CPU stall rather than full bus-cycle arbitration, and some mapper board variants are unsupported.

The emulator core is being implemented in this repository rather than wrapping an existing emulator.

## Version 0.37.0

- Added initial MMC5 (mapper 5) PRG/CHR bank registers and protected cartridge RAM. MMC5 supports per-nametable CIRAM/ExRAM/fill selection, basic ExRAM access, extended-attribute background rendering with per-tile 4 KB CHR banks and palettes, and an initial vertical split-screen renderer with independent vertical scroll and 4 KB CHR banking. Split-screen and scanline IRQ timing are scanline-renderer approximations; expansion audio remains unsupported.

## Version 0.36.0

- Make the desktop launcher use the configured install location of `nes-emulator`. CMake generates the path from the install prefix, so it follows each user's own installation instead of assuming a particular home directory or relying on the desktop environment's PATH.

## Version 0.34.0

- Added AxROM (mapper 7) support with switchable 32 KiB PRG-ROM banks, CHR-RAM, and lower/upper single-screen mirroring.

## Version 0.33.0

- Added CNROM (mapper 3) support with fixed 16/32 KiB PRG-ROM and up to four switchable 8 KiB CHR-ROM banks.

## Version 0.32.0

- Added UxROM (mapper 2) support with switchable lower 16 KiB PRG banking and a fixed final 16 KiB bank.

## Version 0.29.0

- Added initial DMC sample playback with configurable rate, looping, status reporting, and IRQ generation. DMC DMA CPU stall timing remains future work.

## Version 0.28.0

- Added initial NES APU sound with both pulse channels, triangle, and noise. Audio is mixed and streamed through SDL2. DMC playback and cycle-accurate audio timing remain future work.

## Version 0.27.0

- Rebranded the Linux app as PixelCore - NES.
- Added a pixel-art controller icon and desktop-app launcher installation.

## Version 0.24.0

- Added selectable rewind history lengths: 3, 5, 10, or 15 seconds. Press **6** while a game is running to cycle the setting; the default is 10 seconds.

## Version 0.23.0

- Added portable-within-this-build `.savestate` files that capture CPU registers/cycles, system RAM, PRG-RAM, controller shift state, PPU registers/memory/render state, CHR-RAM, and supported mapper registers/IRQ state.
- Save states are checked against the loaded ROM fingerprint and mapper before restoring.
- Added a rolling rewind buffer (about three seconds, sampled every other frame); hold **R** to rewind, **5** to save a state, **8** to load one, and **F6** to select rewind length.

## Version 0.22.0

- Added a DOWNLOAD ROMS menu with the five requested external game-page links. Selecting a title opens its page in the default Linux browser; the emulator does not host or bundle those ROM files.

## Version 0.21.0

- Added a local PRELOADED ROMS menu that scans the ignored `roms/` directory in a fixed game order.
- Added initial Mapper 40 support for The Lost Levels ROM supplied for testing.
- ROM files are deliberately not committed to the public repository.

## Version 0.18.0

- Added large, vertically stacked LOAD ROM and QUIT buttons centered in the window.
- Added on-screen success and error messages when selecting a ROM.
- Added initial Mapper 66 (GxROM) support with PRG/CHR bank switching.
- Added basic controller 1 support: arrows, Z, X, Right Shift, and Enter.
- Kept the menu buttons and text monochrome.
- Removed the diagnostic color grid; the PPU output is black until background rendering is enabled.
- Preserved the NES color palette for actual game graphics.


Version 0.17.0 added an approximate PPU clock (341 dots × 262 scanlines) and PPUSTATUS vblank flag. Version 0.19.0 adds the initial NMI delivery path. Neither implementation is cycle-accurate.


Version 0.18.0 updates the window controls: Escape no longer exits the emulator, a centered FULLSCREEN button toggles desktop fullscreen, 0 is a fullscreen shortcut, and QUIT remains the explicit exit control. The NES picture scales to fit the available window while preserving its aspect ratio.


## Version 0.19.0

- Added an initial PPU vblank-to-CPU NMI interrupt path.
- Added 6502 NMI entry and RTI return handling.
- NMI delivery is still built on approximate PPU timing; the CPU instruction set and cycle timing remain incomplete.


## Version 0.20.0

- Added periodic CPU diagnostics while a ROM is running: program counter, registers, instruction count, and halted state are printed every two seconds.
