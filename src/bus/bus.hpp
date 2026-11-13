#pragma once
#include "../cartridge/cartridge.hpp"
#include "../ppu/ppu2c02.hpp"
#include <array>
#include <cstdint>
#include <cstddef>

class NESBus {
public:
    void connectCartridge(Cartridge* cartridge);
    void reset();
    void clockPpu();
    bool consumeNmi();
    std::uint8_t read(std::uint16_t address);
    void write(std::uint16_t address, std::uint8_t value);
    void setControllerState(std::uint8_t port, std::uint8_t buttons);
    void renderFrame(std::array<std::uint32_t, 256 * 240>& pixels) const;
    std::uint8_t ppuControlDebug() const { return ppu.debugControl(); }
    std::uint8_t ppuMaskDebug() const { return ppu.debugMask(); }
    std::uint8_t ppuStatusDebug() const { return ppu.debugStatus(); }
    std::uint8_t ppuPaletteDebug() const { return ppu.debugPalette(); }
    std::size_t ppuNametableNonZeroDebug() const { return ppu.debugNametableNonZero(); }
    std::size_t ppuChrNonZeroDebug() const { return ppu.debugChrNonZero(); }
    std::size_t ppuSelectedPatternNonZeroDebug() const { return ppu.debugSelectedPatternNonZero(); }
    std::size_t ppuPaletteNonZeroDebug() const { return ppu.debugPaletteNonZero(); }
private:
    std::array<std::uint8_t, 2048> ram{};
    std::array<std::uint8_t, 8192> prgRam{};
    std::array<std::uint8_t, 2> controllerState{};
    std::array<std::uint8_t, 2> controllerShift{};
    bool controllerStrobe = false;
    Cartridge* cartridge = nullptr;
    PPU2C02 ppu;
};
