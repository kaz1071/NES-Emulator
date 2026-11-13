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
    std::uint64_t debugVblankEvents() const { return vblankEvents; }
    std::uint64_t debugVblankStatusReads() const { return vblankStatusReads; }
    std::uint8_t debugLastStatusRead() const { return lastStatusRead; }
    int debugScanline() const { return scanline; }
    int debugPpuCycle() const { return ppuCycle; }
    std::uint8_t debugPalette() const { return paletteRam[0]; }
    std::uint8_t debugScrollX() const { return scrollX; }
    std::uint8_t debugScrollY() const { return scrollY; }
    std::size_t debugNametableNonZero() const;
    std::size_t debugNametableTableNonZero(std::size_t table) const;
    std::uint8_t debugNametableByte(std::size_t table, std::size_t offset) const;
    std::uint8_t debugPaletteByte(std::size_t index) const;
    std::size_t debugChrNonZero() const;
    std::size_t debugSelectedPatternNonZero() const;
    std::size_t debugTileNonZeroPixels(std::uint8_t tile, std::uint16_t patternBase) const;
    std::size_t debugPaletteNonZero() const;
    std::size_t debugVisibleUniqueTiles() const;
    std::size_t debugVisiblePatternPixels() const;
    std::array<std::size_t, 4> debugVisiblePixelValueCounts() const;
    std::array<std::size_t, 64> debugVisiblePaletteIndexCounts() const;
    std::array<std::size_t, 32> debugVisiblePaletteSlotCounts() const;
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
    std::uint64_t vblankEvents = 0;
    std::uint64_t vblankStatusReads = 0;
    std::uint8_t lastStatusRead = 0;

    std::uint16_t mapNametableAddress(std::uint16_t address) const;
    static std::uint8_t mapPaletteAddress(std::uint16_t address);
};
