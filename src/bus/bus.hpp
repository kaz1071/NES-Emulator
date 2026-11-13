#pragma once
#include "../cartridge/cartridge.hpp"
#include "../ppu/ppu2c02.hpp"
#include <array>
#include <cstdint>

class NESBus {
public:
    void connectCartridge(Cartridge* cartridge);
    void reset();
    std::uint8_t read(std::uint16_t address);
    void write(std::uint16_t address, std::uint8_t value);
    void renderFrame(std::array<std::uint32_t, 256 * 240>& pixels) const;
private:
    std::array<std::uint8_t, 2048> ram{};
    std::array<std::uint8_t, 8192> prgRam{};
    Cartridge* cartridge = nullptr;
    PPU2C02 ppu;
};
