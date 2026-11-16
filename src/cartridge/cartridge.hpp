#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

enum class Mirroring {
    Horizontal,
    Vertical,
    FourScreen,
    OneScreenLower,
    OneScreenUpper
};

struct Cartridge {
    std::vector<std::uint8_t> prgRom;
    std::vector<std::uint8_t> chrData;
    bool usesChrRam = false;
    Mirroring mirroring = Mirroring::Horizontal;
    std::uint8_t mapper = 0;
    std::uint8_t prgBank = 0;
    std::uint8_t chrBank = 0;

    // MMC1 (mapper 1) serial registers.
    std::uint8_t mmc1Shift = 0x10;
    std::uint8_t mmc1Control = 0x0C;
    std::uint8_t mmc1ChrBank0 = 0;
    std::uint8_t mmc1ChrBank1 = 0;
    std::uint8_t mmc1PrgBank = 0;
    bool prgRamEnabled() const;

    std::uint8_t readPrg(std::uint16_t address) const;
    void writePrg(std::uint16_t address, std::uint8_t value);
    std::uint8_t readChr(std::uint16_t address) const;
    void writeChr(std::uint16_t address, std::uint8_t value);
};

bool loadCartridge(const std::filesystem::path& path, Cartridge& cartridge,
                   std::string& error);
