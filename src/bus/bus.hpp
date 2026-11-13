#pragma once
#include "../cartridge/cartridge.hpp"
#include <array>
#include <cstdint>

class NESBus {
public:
    void connectCartridge(const Cartridge* cartridge);
    void reset();
    std::uint8_t read(std::uint16_t address) const;
    void write(std::uint16_t address, std::uint8_t value);
private:
    std::array<std::uint8_t, 2048> ram{};
    std::array<std::uint8_t, 8192> prgRam{};
    const Cartridge* cartridge = nullptr;
};
