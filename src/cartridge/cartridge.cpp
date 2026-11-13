#include "cartridge.hpp"

#include <cstddef>
#include <utility>
#include <fstream>
#include <iterator>

namespace {
constexpr std::size_t HEADER_SIZE = 16;
constexpr std::size_t TRAINER_SIZE = 512;
constexpr std::size_t PRG_BANK_SIZE = 16 * 1024;
constexpr std::size_t CHR_BANK_SIZE = 8 * 1024;
constexpr std::size_t GXMROM_PRG_BANK_SIZE = 32 * 1024;
}

std::uint8_t Cartridge::readPrg(std::uint16_t address) const {
    if (prgRom.empty() || address < 0x8000) return 0;
    std::size_t offset = static_cast<std::size_t>(address - 0x8000);
    if (mapper == 66) {
        const std::size_t bankCount = prgRom.size() / GXMROM_PRG_BANK_SIZE;
        if (bankCount == 0) return 0;
        offset += (static_cast<std::size_t>(prgBank) % bankCount) * GXMROM_PRG_BANK_SIZE;
    } else {
        offset %= prgRom.size();
    }
    return prgRom[offset % prgRom.size()];
}

void Cartridge::writePrg(std::uint16_t address, std::uint8_t value) {
    if (mapper == 66 && address >= 0x8000) {
        prgBank = static_cast<std::uint8_t>((value >> 4) & 0x03);
        chrBank = static_cast<std::uint8_t>(value & 0x03);
    }
}

std::uint8_t Cartridge::readChr(std::uint16_t address) const {
    if (chrData.empty() || address >= 0x2000) return 0;
    std::size_t offset = address;
    if (mapper == 66 && !usesChrRam) {
        const std::size_t bankCount = chrData.size() / CHR_BANK_SIZE;
        if (bankCount != 0) {
            offset += (static_cast<std::size_t>(chrBank) % bankCount) * CHR_BANK_SIZE;
        }
    }
    return chrData[offset % chrData.size()];
}

void Cartridge::writeChr(std::uint16_t address, std::uint8_t value) {
    if (usesChrRam && address < 0x2000 && address < chrData.size()) {
        chrData[address] = value;
    }
}

bool loadCartridge(const std::filesystem::path& path, Cartridge& cartridge,
                   std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "Could not open ROM file: " + path.string();
        return false;
    }

    std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    if (bytes.size() < HEADER_SIZE) {
        error = "File is too small to contain an iNES header.";
        return false;
    }

    if (bytes[0] != 'N' || bytes[1] != 'E' ||
        bytes[2] != 'S' || bytes[3] != 0x1A) {
        error = "This file does not have a valid iNES signature.";
        return false;
    }

    const std::uint8_t flags6 = bytes[6];
    const std::uint8_t flags7 = bytes[7];

    if ((flags7 & 0x0C) == 0x08) {
        error = "NES 2.0 ROMs are not supported yet; use a standard iNES ROM.";
        return false;
    }

    const std::uint8_t mapper =
        static_cast<std::uint8_t>((flags6 >> 4) | (flags7 & 0xF0));

    if (mapper != 0 && mapper != 66) {
        error = "Unsupported mapper " + std::to_string(mapper) +
                ". This version supports mapper 0 (NROM) and mapper 66 (GxROM).";
        return false;
    }

    const std::size_t prgSize =
        static_cast<std::size_t>(bytes[4]) * PRG_BANK_SIZE;
    const std::size_t chrSize =
        static_cast<std::size_t>(bytes[5]) * CHR_BANK_SIZE;

    if (mapper == 0 && prgSize != PRG_BANK_SIZE && prgSize != 2 * PRG_BANK_SIZE) {
        error = "Mapper 0 requires 1 or 2 PRG-ROM banks (16 or 32 KiB).";
        return false;
    }
    if (mapper == 66 &&
        (prgSize < GXMROM_PRG_BANK_SIZE ||
         prgSize > 4 * GXMROM_PRG_BANK_SIZE ||
         prgSize % GXMROM_PRG_BANK_SIZE != 0)) {
        error = "Mapper 66 requires 1 to 4 complete 32 KiB PRG-ROM banks.";
        return false;
    }

    if (mapper == 0 && chrSize > CHR_BANK_SIZE) {
        error = "Mapper 0 supports at most one 8 KiB CHR-ROM bank.";
        return false;
    }
    if (mapper == 66 && chrSize > 4 * CHR_BANK_SIZE) {
        error = "Mapper 66 supports at most four 8 KiB CHR-ROM banks.";
        return false;
    }

    const std::size_t trainerSize = (flags6 & 0x04) ? TRAINER_SIZE : 0;
    const std::size_t dataOffset = HEADER_SIZE + trainerSize;
    const std::size_t requiredSize = dataOffset + prgSize + chrSize;

    if (bytes.size() < requiredSize) {
        error = "ROM file is truncated: the header declares more data than the file contains.";
        return false;
    }

    Cartridge loaded;
    loaded.mapper = mapper;
    loaded.mirroring = (flags6 & 0x08)
        ? Mirroring::FourScreen
        : ((flags6 & 0x01) ? Mirroring::Vertical : Mirroring::Horizontal);

    const auto prgBegin = bytes.begin() + static_cast<std::ptrdiff_t>(dataOffset);
    loaded.prgRom.assign(prgBegin,
                         prgBegin + static_cast<std::ptrdiff_t>(prgSize));

    loaded.usesChrRam = (chrSize == 0);
    if (loaded.usesChrRam) {
        loaded.chrData.resize(CHR_BANK_SIZE, 0);
    } else {
        const auto chrBegin =
            prgBegin + static_cast<std::ptrdiff_t>(prgSize);
        loaded.chrData.assign(chrBegin,
                              chrBegin + static_cast<std::ptrdiff_t>(chrSize));
    }

    cartridge = std::move(loaded);
    return true;
}
