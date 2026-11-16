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

bool Cartridge::prgRamEnabled() const {
    if (mapper == 1) return (mmc1PrgBank & 0x10) == 0;
    if (mapper == 4) return mmc3PrgRamEnabled;
    return true;
}

bool Cartridge::prgRamWritable() const {
    return prgRamEnabled() && !(mapper == 4 && mmc3PrgRamWriteProtected);
}

bool Cartridge::irqPending() const {
    return mapper == 4 && mmc3IrqPending;
}

void Cartridge::clockScanline() {
    if (mapper != 4) return;
    if (mmc3IrqCounter == 0 || mmc3IrqReload) {
        mmc3IrqCounter = mmc3IrqLatch;
        mmc3IrqReload = false;
    } else {
        --mmc3IrqCounter;
    }
    if (mmc3IrqCounter == 0 && mmc3IrqEnabled) mmc3IrqPending = true;
}

std::uint8_t Cartridge::readPrg(std::uint16_t address) const {
    if (prgRom.empty() || address < 0x8000) return 0;
    std::size_t offset = static_cast<std::size_t>(address - 0x8000);
    if (mapper == 1) {
        const std::size_t count = prgRom.size() / PRG_BANK_SIZE;
        if (!count) return 0;
        const std::size_t slot = offset / PRG_BANK_SIZE;
        const std::size_t bank = mmc1PrgBank & 0x0F;
        const std::size_t mode = (mmc1Control >> 2) & 3;
        std::size_t selected = mode < 2 ? ((bank & 0x0E) + slot) % count
            : mode == 2 ? (slot == 0 ? 0 : bank % count)
            : (slot == 0 ? bank % count : count - 1);
        offset = selected * PRG_BANK_SIZE + offset % PRG_BANK_SIZE;
    } else if (mapper == 4) {
        const std::size_t count = prgRom.size() / 0x2000;
        if (!count) return 0;
        const std::size_t slot = offset / 0x2000;
        const std::size_t last = count - 1, secondLast = count > 1 ? count - 2 : 0;
        const std::size_t b6 = mmc3BankRegisters[6] % count;
        const std::size_t b7 = mmc3BankRegisters[7] % count;
        std::size_t selected;
        if (!(mmc3BankSelect & 0x40))
            selected = slot == 0 ? b6 : slot == 1 ? b7 : slot == 2 ? secondLast : last;
        else
            selected = slot == 0 ? secondLast : slot == 1 ? b7 : slot == 2 ? b6 : last;
        offset = selected * 0x2000 + (offset & 0x1FFF);
    } else if (mapper == 66) {
        const std::size_t count = prgRom.size() / GXMROM_PRG_BANK_SIZE;
        if (!count) return 0;
        offset += (static_cast<std::size_t>(prgBank) % count) * GXMROM_PRG_BANK_SIZE;
    } else offset %= prgRom.size();
    return prgRom[offset % prgRom.size()];
}

void Cartridge::writePrg(std::uint16_t address, std::uint8_t value) {
    if (address < 0x8000) return;
    if (mapper == 66) {
        prgBank = static_cast<std::uint8_t>((value >> 4) & 3);
        chrBank = static_cast<std::uint8_t>(value & 3);
        return;
    }
    if (mapper == 1) {
        if (value & 0x80) {
            mmc1Shift = 0x10;
            mmc1Control |= 0x0C;
            return;
        }
        const bool complete = (mmc1Shift & 1) != 0;
        mmc1Shift = static_cast<std::uint8_t>((mmc1Shift >> 1) | ((value & 1) << 4));
        if (!complete) return;
        const std::uint8_t data = mmc1Shift & 0x1F;
        if (address < 0xA000) {
            mmc1Control = data;
            if (mirroring != Mirroring::FourScreen) {
                switch (data & 3) {
                    case 0: mirroring = Mirroring::OneScreenLower; break;
                    case 1: mirroring = Mirroring::OneScreenUpper; break;
                    case 2: mirroring = Mirroring::Vertical; break;
                    default: mirroring = Mirroring::Horizontal; break;
                }
            }
        } else if (address < 0xC000) mmc1ChrBank0 = data;
        else if (address < 0xE000) mmc1ChrBank1 = data;
        else mmc1PrgBank = data;
        mmc1Shift = 0x10;
        return;
    }
    if (mapper != 4) return;
    const bool odd = (address & 1) != 0;
    switch (address & 0xE000) {
        case 0x8000:
            if (!odd) mmc3BankSelect = value;
            else {
                const std::uint8_t reg = mmc3BankSelect & 7;
                mmc3BankRegisters[reg] = reg < 2 ? static_cast<std::uint8_t>(value & 0xFE) : value;
            }
            break;
        case 0xA000:
            if (!odd) {
                if (mirroring != Mirroring::FourScreen)
                    mirroring = (value & 1) ? Mirroring::Horizontal : Mirroring::Vertical;
            } else {
                mmc3PrgRamEnabled = (value & 0x80) != 0;
                mmc3PrgRamWriteProtected = (value & 0x40) != 0;
            }
            break;
        case 0xC000:
            if (!odd) mmc3IrqLatch = value;
            else mmc3IrqReload = true;
            break;
        case 0xE000:
            if (!odd) { mmc3IrqEnabled = false; mmc3IrqPending = false; }
            else mmc3IrqEnabled = true;
            break;
        default: break;
    }
}

std::uint8_t Cartridge::readChr(std::uint16_t address) const {
    if (chrData.empty() || address >= 0x2000) return 0;
    std::size_t offset = address;
    if (mapper == 1) {
        const std::size_t count = chrData.size() / 0x1000;
        if (!count) return 0;
        if (mmc1Control & 0x10) {
            const std::size_t bank = address < 0x1000 ? mmc1ChrBank0 : mmc1ChrBank1;
            offset = (bank % count) * 0x1000 + (address & 0x0FFF);
        } else offset = (((mmc1ChrBank0 & 0x1E) * 0x1000) + address) % chrData.size();
    } else if (mapper == 4) {
        const std::size_t count = chrData.size() / 0x400;
        if (!count) return 0;
        const std::size_t slot = address / 0x400;
        const bool inv = (mmc3BankSelect & 0x80) != 0;
        std::size_t bank;
        if (!inv) {
            if (slot < 2) bank = (mmc3BankRegisters[0] & 0xFE) + slot;
            else if (slot < 4) bank = (mmc3BankRegisters[1] & 0xFE) + slot - 2;
            else bank = mmc3BankRegisters[slot - 2];
        } else {
            if (slot < 4) bank = mmc3BankRegisters[slot + 2];
            else if (slot < 6) bank = (mmc3BankRegisters[0] & 0xFE) + slot - 4;
            else bank = (mmc3BankRegisters[1] & 0xFE) + slot - 6;
        }
        offset = (bank % count) * 0x400 + (address & 0x03FF);
    } else if (mapper == 66 && !usesChrRam) {
        const std::size_t count = chrData.size() / CHR_BANK_SIZE;
        if (count) offset += (static_cast<std::size_t>(chrBank) % count) * CHR_BANK_SIZE;
    }
    return chrData[offset % chrData.size()];
}

void Cartridge::writeChr(std::uint16_t address, std::uint8_t value) {
    if (!usesChrRam || address >= 0x2000 || chrData.empty()) return;
    std::size_t offset = address;
    if (mapper == 1) {
        const std::size_t count = chrData.size() / 0x1000;
        if (!count) return;
        if (mmc1Control & 0x10) {
            const std::size_t bank = address < 0x1000 ? mmc1ChrBank0 : mmc1ChrBank1;
            offset = (bank % count) * 0x1000 + (address & 0x0FFF);
        } else offset = (((mmc1ChrBank0 & 0x1E) * 0x1000) + address) % chrData.size();
    } else if (mapper == 4) {
        const std::size_t count = chrData.size() / 0x400;
        if (!count) return;
        const std::size_t slot = address / 0x400;
        const bool inv = (mmc3BankSelect & 0x80) != 0;
        std::size_t bank;
        if (!inv) {
            if (slot < 2) bank = (mmc3BankRegisters[0] & 0xFE) + slot;
            else if (slot < 4) bank = (mmc3BankRegisters[1] & 0xFE) + slot - 2;
            else bank = mmc3BankRegisters[slot + 2];
        } else {
            if (slot < 4) bank = mmc3BankRegisters[slot + 2];
            else if (slot < 6) bank = (mmc3BankRegisters[0] & 0xFE) + slot - 4;
            else bank = (mmc3BankRegisters[1] & 0xFE) + slot - 6;
        }
        offset = (bank % count) * 0x400 + (address & 0x03FF);
    }
    chrData[offset % chrData.size()] = value;
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

    if (mapper != 0 && mapper != 1 && mapper != 4 && mapper != 66) {
        error = "Unsupported mapper " + std::to_string(mapper) +
                ". This version supports mapper 0 (NROM), mapper 1 (MMC1), mapper 4 (MMC3), and mapper 66 (GxROM).";
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

    if (mapper == 4 && (prgSize < 16 * 1024 || prgSize > 512 * 1024 || prgSize % 8192 != 0)) {
        error = "Mapper 4 requires 16 to 512 KiB of PRG-ROM in 8 KiB banks.";
        return false;
    }
    if (mapper == 4 && chrSize > 256 * 1024) {
        error = "Mapper 4 supports at most 256 KiB of CHR-ROM.";
        return false;
    }

    if (mapper == 1 &&
        (prgSize < PRG_BANK_SIZE || prgSize > 16 * PRG_BANK_SIZE ||
         prgSize % PRG_BANK_SIZE != 0)) {
        error = "Mapper 1 requires 1 to 16 complete 16 KiB PRG-ROM banks.";
        return false;
    }
    if (mapper == 1 && chrSize > 32 * 4 * 1024) {
        error = "Mapper 1 supports at most 128 KiB of CHR-ROM.";
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
