#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

enum class Mirroring {
    Horizontal,
    Vertical,
    FourScreen
};

struct Cartridge {
    std::vector<std::uint8_t> prgRom;
    std::vector<std::uint8_t> chrData;
    bool usesChrRam = false;
    Mirroring mirroring = Mirroring::Horizontal;
    std::uint8_t mapper = 0;
};

bool loadCartridge(const std::filesystem::path& path, Cartridge& cartridge,
                   std::string& error);
