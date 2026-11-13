#pragma once

#include "../cartridge/cartridge.hpp"
#include <array>
#include <cstdint>
#include <cstddef>

class PPU2C02 {
public:
    void connectCartridge(Cartridge* cartridge);
    void reset();
    void clock();
    bool consumeNmi();
    std::uint8_t debugControl() const { return control; }
    std::uint8_t debugMask() const { return mask; }
    std::uint8_t debugStatus() const { return status; }
    std::uint8_t debugPalette() const { return paletteRam[0]; }
    std::size_t debugNametableNonZero() const;
    std::size_t debugNametableTableNonZero(std::size_t table) const;
    std::size_t debugChrNonZero() const;
    std::size_t debugSelectedPatternNonZero() const;
    std::size_t debugPaletteNonZero() const;
    void renderFrame(std::array<std::uint32_t, 256 * 240>& pixels) const;

    std::uint8_t cpuRead(std::uint16_t registerAddress);
    void cpuWrite(std::uint16_t registerAddress, std::uint8_t value);

    std::uint8_t ppuRead(std::uint16_t address) const;
    void ppuWrite(std::uint16_t address, std::uint8_t value);

private:
    Cartridge* cartridge = nullptr;
    std::array<std::uint8_t, 4096> nametableRam{};
    std::array<std::uint8_t, 32> paletteRam{};
    std::array<std::uint8_t, 256> oam{};

    std::uint8_t control = 0;
    std::uint8_t mask = 0;
    std::uint8_t status = 0;
    std::uint8_t oamAddress = 0;
    std::uint8_t openBus = 0;
    std::uint8_t readBuffer = 0;
    std::uint16_t vramAddress = 0;
    std::uint16_t temporaryAddress = 0;
    std::uint8_t fineX = 0;
    std::uint8_t scrollX = 0;
    std::uint8_t scrollY = 0;
    bool writeSecond = false;
    int ppuCycle = 0;
    int scanline = 261;
    bool nmiPending = false;

    std::uint16_t mapNametableAddress(std::uint16_t address) const;
    static std::uint8_t mapPaletteAddress(std::uint16_t address);
};
