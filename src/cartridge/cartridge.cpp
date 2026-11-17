#include "cartridge.hpp"

#include <cstddef>
#include <algorithm>
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

CartridgeState Cartridge::saveState() const {
    CartridgeState state{};
    state.mirroring = mirroring;
    state.mapper = mapper;
    state.prgBank = prgBank;
    state.chrBank = chrBank;
    state.mmc1Shift = mmc1Shift;
    state.mmc1Control = mmc1Control;
    state.mmc1ChrBank0 = mmc1ChrBank0;
    state.mmc1ChrBank1 = mmc1ChrBank1;
    state.mmc1PrgBank = mmc1PrgBank;
    state.mmc3BankRegisters = mmc3BankRegisters;
    state.mmc3BankSelect = mmc3BankSelect;
    state.mmc3PrgRamEnabled = mmc3PrgRamEnabled;
    state.mmc3PrgRamWriteProtected = mmc3PrgRamWriteProtected;
    state.mmc3IrqLatch = mmc3IrqLatch;
    state.mmc3IrqCounter = mmc3IrqCounter;
    state.mmc3IrqReload = mmc3IrqReload;
    state.mmc3IrqEnabled = mmc3IrqEnabled;
    state.mmc3IrqPending = mmc3IrqPending;
    state.mapper40PrgBank = mapper40PrgBank;
    state.mapper40IrqCounter = mapper40IrqCounter;
    state.mapper40IrqEnabled = mapper40IrqEnabled;
    state.mapper40IrqPending = mapper40IrqPending;
    state.mmc5PrgRegisters = mmc5PrgRegisters;
    state.mmc5ChrRegisters = mmc5ChrRegisters;
    state.mmc5BgChrRegisters = mmc5BgChrRegisters;
    state.mmc5PrgRam = mmc5PrgRam;
    state.mmc5PrgMode = mmc5PrgMode;
    state.mmc5ChrMode = mmc5ChrMode;
    state.mmc5RamProtect1 = mmc5RamProtect1;
    state.mmc5RamProtect2 = mmc5RamProtect2;
    state.mmc5ChrHigh = mmc5ChrHigh;
    state.mmc5ExRam = mmc5ExRam;
    state.mmc5NametableMap = mmc5NametableMap;
    state.mmc5ExRamMode = mmc5ExRamMode;
    state.mmc5FillTile = mmc5FillTile;
    state.mmc5FillAttribute = mmc5FillAttribute;
    state.mmc5VerticalSplit = mmc5VerticalSplit;
    state.mmc5VerticalSplitScroll = mmc5VerticalSplitScroll;
    state.mmc5VerticalSplitBank = mmc5VerticalSplitBank;
    state.mmc5IrqCompare = mmc5IrqCompare;
    state.mmc5IrqScanline = mmc5IrqScanline;
    state.mmc5MultiplyA = mmc5MultiplyA;
    state.mmc5MultiplyB = mmc5MultiplyB;
    state.mmc5IrqEnabled = mmc5IrqEnabled;
    state.mmc5IrqPending = mmc5IrqPending;
    state.mmc5InFrame = mmc5InFrame;
    state.usesChrRam = usesChrRam;
    if (usesChrRam) {
        const std::size_t count = std::min(chrData.size(), state.chrRam.size());
        std::copy_n(chrData.begin(), count, state.chrRam.begin());
    }
    return state;
}

bool Cartridge::loadState(const CartridgeState& state) {
    if (state.mapper != mapper || state.usesChrRam != usesChrRam) return false;
    mirroring = state.mirroring;
    prgBank = state.prgBank;
    chrBank = state.chrBank;
    mmc1Shift = state.mmc1Shift;
    mmc1Control = state.mmc1Control;
    mmc1ChrBank0 = state.mmc1ChrBank0;
    mmc1ChrBank1 = state.mmc1ChrBank1;
    mmc1PrgBank = state.mmc1PrgBank;
    mmc3BankRegisters = state.mmc3BankRegisters;
    mmc3BankSelect = state.mmc3BankSelect;
    mmc3PrgRamEnabled = state.mmc3PrgRamEnabled;
    mmc3PrgRamWriteProtected = state.mmc3PrgRamWriteProtected;
    mmc3IrqLatch = state.mmc3IrqLatch;
    mmc3IrqCounter = state.mmc3IrqCounter;
    mmc3IrqReload = state.mmc3IrqReload;
    mmc3IrqEnabled = state.mmc3IrqEnabled;
    mmc3IrqPending = state.mmc3IrqPending;
    mapper40PrgBank = state.mapper40PrgBank;
    mapper40IrqCounter = state.mapper40IrqCounter;
    mapper40IrqEnabled = state.mapper40IrqEnabled;
    mapper40IrqPending = state.mapper40IrqPending;
    mmc5PrgRegisters = state.mmc5PrgRegisters;
    mmc5ChrRegisters = state.mmc5ChrRegisters;
    mmc5BgChrRegisters = state.mmc5BgChrRegisters;
    mmc5PrgRam = state.mmc5PrgRam;
    mmc5PrgMode = state.mmc5PrgMode;
    mmc5ChrMode = state.mmc5ChrMode;
    mmc5RamProtect1 = state.mmc5RamProtect1;
    mmc5RamProtect2 = state.mmc5RamProtect2;
    mmc5ChrHigh = state.mmc5ChrHigh;
    mmc5ExRam = state.mmc5ExRam;
    mmc5NametableMap = state.mmc5NametableMap;
    mmc5ExRamMode = state.mmc5ExRamMode;
    mmc5FillTile = state.mmc5FillTile;
    mmc5FillAttribute = state.mmc5FillAttribute;
    mmc5VerticalSplit = state.mmc5VerticalSplit;
    mmc5VerticalSplitScroll = state.mmc5VerticalSplitScroll;
    mmc5VerticalSplitBank = state.mmc5VerticalSplitBank;
    mmc5IrqCompare = state.mmc5IrqCompare;
    mmc5IrqScanline = state.mmc5IrqScanline;
    mmc5MultiplyA = state.mmc5MultiplyA;
    mmc5MultiplyB = state.mmc5MultiplyB;
    mmc5IrqEnabled = state.mmc5IrqEnabled;
    mmc5IrqPending = state.mmc5IrqPending;
    mmc5InFrame = state.mmc5InFrame;
    if (usesChrRam) {
        const std::size_t count = std::min(chrData.size(), state.chrRam.size());
        std::copy_n(state.chrRam.begin(), count, chrData.begin());
    }
    return true;
}

std::uint64_t Cartridge::romFingerprint() const {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto add = [&hash](std::uint8_t byte) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    };
    add(mapper);
    for (const auto byte : prgRom) add(byte);
    if (!usesChrRam) for (const auto byte : chrData) add(byte);
    return hash;
}

bool Cartridge::prgRamEnabled() const {
    if (mapper == 1) return (mmc1PrgBank & 0x10) == 0;
    if (mapper == 5) return mmc5RamProtect1 == 2 && mmc5RamProtect2 == 1;
    if (mapper == 4) return mmc3PrgRamEnabled;
    return true;
}

bool Cartridge::prgRamWritable() const {
    return prgRamEnabled() && !(mapper == 4 && mmc3PrgRamWriteProtected);
}

bool Cartridge::irqPending() const {
    return (mapper == 4 && mmc3IrqPending) ||
           (mapper == 40 && mapper40IrqPending) ||
           (mapper == 5 && mmc5IrqEnabled && mmc5IrqPending);
}

void Cartridge::clockCpuCycle() {
    if (mapper == 40 && mapper40IrqEnabled) {
        ++mapper40IrqCounter;
        if (mapper40IrqCounter >= 4096) {
            mapper40IrqEnabled = false;
            mapper40IrqPending = true;
        }
    }
}

void Cartridge::clockScanline(bool rendering) {
    if (mapper == 5) {
        if (!rendering) {
            endMmc5Frame();
            return;
        }
        if (!mmc5InFrame) {
            mmc5InFrame = true;
            mmc5IrqScanline = 0;
            mmc5IrqPending = false;
        } else if (mmc5IrqScanline < 0xFF) {
            ++mmc5IrqScanline;
        }
        // Scanline zero is a valid compare target: the first visible
        // scanline establishes the in-frame state and is compared immediately.
        if (mmc5IrqCompare < 240 && mmc5IrqScanline == mmc5IrqCompare)
            mmc5IrqPending = true;
        return;
    }
    if (mapper != 4) return;
    if (mmc3IrqCounter == 0 || mmc3IrqReload) {
        mmc3IrqCounter = mmc3IrqLatch;
        mmc3IrqReload = false;
    } else {
        --mmc3IrqCounter;
    }
    if (mmc3IrqCounter == 0 && mmc3IrqEnabled) mmc3IrqPending = true;
}

void Cartridge::endMmc5Frame() {
    if (mapper != 5) return;
    mmc5InFrame = false;
    mmc5IrqScanline = 0;
    mmc5IrqPending = false;
}

std::uint8_t Cartridge::readPrg(std::uint16_t address) {
    if (mapper == 5) {
        if (address == 0x5204) {
            const std::uint8_t value = static_cast<std::uint8_t>(
                (mmc5IrqPending ? 0x80 : 0) | (mmc5InFrame ? 0x40 : 0));
            mmc5IrqPending = false;
            return value;
        }
        if (address == 0x5205 || address == 0x5206) {
            const std::uint16_t product =
                static_cast<std::uint16_t>(mmc5MultiplyA) * mmc5MultiplyB;
            return address == 0x5205
                ? static_cast<std::uint8_t>(product & 0xFF)
                : static_cast<std::uint8_t>(product >> 8);
        }
        if (address == 0x5200 || address == 0x5201 || address == 0x5202 || address == 0x5203)
            return 0;
        if (address >= 0x5C00 && address < 0x6000) return mmc5ExRam[address & 0x03FF];
        if (address >= 0x6000 && address < 0x8000) {
            const std::size_t bank = mmc5PrgRegisters[0] & 0x7F;
            const std::size_t offset = bank * 0x2000 + (address & 0x1FFF);
            return mmc5PrgRam[offset];
        }
        if (address < 0x8000 || prgRom.empty()) return 0;
        const std::size_t count = prgRom.size() / 0x2000;
        if (!count) return 0;

        const std::size_t slot = (address - 0x8000) / 0x2000;
        std::size_t registerIndex = slot + 1;
        std::size_t groupSize = 1;

        // $5100 selects how the four CPU PRG windows are assembled.
        // In grouped modes, the final register in each group supplies the
        // aligned bank number and the lower slots fill the group in order.
        switch (mmc5PrgMode & 3) {
            case 0:
                registerIndex = 4; // One 32 KiB window, selected by $5117.
                groupSize = 4;
                break;
            case 1:
                registerIndex = slot < 2 ? 2 : 4; // Two 16 KiB windows.
                groupSize = 2;
                break;
            case 2:
                if (slot < 2) {
                    registerIndex = 2; // One 16 KiB window.
                    groupSize = 2;
                } else {
                    registerIndex = slot == 2 ? 3 : 4; // Two 8 KiB windows.
                }
                break;
            default:
                registerIndex = slot + 1; // Four independent 8 KiB windows.
                break;
        }

        const std::uint8_t bankReg = mmc5PrgRegisters[registerIndex];
        const std::size_t bankBase =
            static_cast<std::size_t>(bankReg & 0x7F) & ~(groupSize - 1);
        const std::size_t bank = bankBase + (slot % groupSize);
        const bool selectsRom = registerIndex == 4 || (bankReg & 0x80) != 0;
        if (!selectsRom)
            return mmc5PrgRam[bank * 0x2000 + (address & 0x1FFF)];
        return prgRom[(bank % count) * 0x2000 + (address & 0x1FFF)];
    }
    if (prgRom.empty() || address < (mapper == 40 ? 0x6000 : 0x8000)) return 0;
    std::size_t offset = static_cast<std::size_t>(address - (mapper == 40 ? 0x6000 : 0x8000));
    if (mapper == 40) {
        const std::size_t count = prgRom.size() / 0x2000;
        if (count < 8) return 0;
        const std::size_t slot = (address - 0x6000) / 0x2000;
        const std::size_t bank = slot == 0 ? 6 : slot == 1 ? 4 : slot == 2 ? 5 :
            slot == 3 ? (mapper40PrgBank & 7) : 7;
        return prgRom[(bank % count) * 0x2000 + (address & 0x1FFF)];
    }
    if (mapper == 7) {
        const std::size_t count = prgRom.size() / GXMROM_PRG_BANK_SIZE;
        if (!count) return 0;
        offset += (static_cast<std::size_t>(prgBank) % count) * GXMROM_PRG_BANK_SIZE;
    } else if (mapper == 3) {
        // CNROM keeps PRG-ROM fixed; writes select an 8 KiB CHR-ROM bank.
    } else if (mapper == 2) {
        const std::size_t count = prgRom.size() / PRG_BANK_SIZE;
        if (count < 2) return 0;
        const std::size_t slot = offset / PRG_BANK_SIZE;
        const std::size_t bank = slot == 0
            ? static_cast<std::size_t>(prgBank) % count
            : count - 1;
        offset = bank * PRG_BANK_SIZE + (offset % PRG_BANK_SIZE);
    } else if (mapper == 1) {
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
    if (mapper == 5) {
        if (address >= 0x5C00 && address < 0x6000) {
            if (mmc5ExRamMode >= 2) mmc5ExRam[address & 0x03FF] = value;
            return;
        }
        if (address >= 0x6000 && address < 0x8000) {
            if (prgRamWritable()) {
                const std::size_t bank = mmc5PrgRegisters[0] & 0x7F;
                const std::size_t offset = bank * 0x2000 + (address & 0x1FFF);
                mmc5PrgRam[offset] = value;
            }
            return;
        }
        if (address == 0x5100) { mmc5PrgMode = value & 3; return; }
        if (address == 0x5101) { mmc5ChrMode = value & 3; return; }
        if (address == 0x5102) { mmc5RamProtect1 = value & 3; return; }
        if (address == 0x5103) { mmc5RamProtect2 = value & 3; return; }
        if (address == 0x5104) { mmc5ExRamMode = value & 3; return; }
        if (address == 0x5105) { mmc5NametableMap = value; return; }
        if (address == 0x5106) { mmc5FillTile = value; return; }
        if (address == 0x5107) { mmc5FillAttribute = value & 3; return; }
        if (address == 0x5130) { mmc5ChrHigh = value & 3; return; }
        if (address == 0x5200) { mmc5VerticalSplit = value; return; }
        if (address == 0x5201) { mmc5VerticalSplitScroll = value; return; }
        if (address == 0x5202) { mmc5VerticalSplitBank = value; return; }
        if (address == 0x5203) { mmc5IrqCompare = value; return; }
        if (address == 0x5205) { mmc5MultiplyA = value; return; }
        if (address == 0x5206) { mmc5MultiplyB = value; return; }
        if (address == 0x5204) { mmc5IrqEnabled = (value & 0x80) != 0; return; }
        if (address >= 0x5113 && address <= 0x5117) {
            mmc5PrgRegisters[address - 0x5113] = value;
            return;
        }
        if (address >= 0x5120 && address <= 0x5127) {
            mmc5ChrRegisters[address - 0x5120] = value;
            return;
        }
        if (address >= 0x5128 && address <= 0x512B) {
            mmc5BgChrRegisters[address - 0x5128] = value;
            return;
        }
        return;
    }
    if (mapper == 40) {
        if (address >= 0x8000 && address < 0xA000) {
            mapper40IrqEnabled = false;
            mapper40IrqCounter = 0;
            mapper40IrqPending = false;
        } else if (address >= 0xA000 && address < 0xC000) {
            if (!mapper40IrqEnabled) {
                mapper40IrqCounter = 0;
                mapper40IrqEnabled = true;
            }
        } else if (address >= 0xE000) {
            mapper40PrgBank = value & 7;
        }
        return;
    }
    if (address < 0x8000) return;
    if (mapper == 2) {
        const std::size_t count = prgRom.size() / PRG_BANK_SIZE;
        if (count > 1) prgBank = static_cast<std::uint8_t>(value & 0x0F);
        return;
    }
    if (mapper == 7) {
        prgBank = static_cast<std::uint8_t>(value & 0x07);
        mirroring = (value & 0x10)
            ? Mirroring::OneScreenUpper
            : Mirroring::OneScreenLower;
        return;
    }
    if (mapper == 3) {
        chrBank = static_cast<std::uint8_t>(value & 0x03);
        return;
    }
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

std::uint8_t Cartridge::readMmc5Chr(std::uint16_t address, bool background) const {
    if (mapper != 5 || chrData.empty() || address >= 0x2000) return 0;
    const std::size_t count = chrData.size() / 0x400;
    if (count == 0) return 0;

    const std::size_t slot = address / 0x400;
    const std::size_t groupSize = std::size_t{1} << (3 - (mmc5ChrMode & 3));
    const std::size_t registerCount = background ? 4 : 8;
    const std::size_t registerSlot = background && mmc5ChrMode != 0 ? slot % 4 : slot;
    const std::size_t registerIndex =
        std::min(registerCount - 1,
                 ((registerSlot / groupSize) + 1) * groupSize - 1);
    // The background and sprite register arrays have different sizes,
    // so select the register value rather than binding them through a ternary.
    const std::uint8_t bankRegister = background
        ? mmc5BgChrRegisters[registerIndex]
        : mmc5ChrRegisters[registerIndex];
    const std::size_t bankBase =
        ((static_cast<std::size_t>(mmc5ChrHigh) << 8) |
         bankRegister) & ~(groupSize - 1);
    const std::size_t bank = (bankBase + (registerSlot % groupSize)) % count;
    const std::size_t offset = bank * 0x400 + (address & 0x03FF);
    return chrData[offset % chrData.size()];
}

std::uint8_t Cartridge::readChr(std::uint16_t address) const {
    if (chrData.empty() || address >= 0x2000) return 0;
    std::size_t offset = address;
    if (mapper == 5) return readMmc5Chr(address, false);
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
    } else if (mapper == 3 && !usesChrRam) {
        const std::size_t count = chrData.size() / CHR_BANK_SIZE;
        if (count) offset += (static_cast<std::size_t>(chrBank) % count) * CHR_BANK_SIZE;
    } else if (mapper == 66 && !usesChrRam) {
        const std::size_t count = chrData.size() / CHR_BANK_SIZE;
        if (count) offset += (static_cast<std::size_t>(chrBank) % count) * CHR_BANK_SIZE;
    }
    return chrData[offset % chrData.size()];
}

std::uint8_t Cartridge::readMmc5ExtendedChr(std::uint16_t address, std::uint8_t bank) const {
    if (mapper != 5 || chrData.empty()) return readChr(address);
    const std::size_t count = chrData.size() / 0x1000;
    if (count == 0) return 0;
    const std::size_t selected = ((static_cast<std::size_t>(mmc5ChrHigh & 3) << 6) |
                                  (bank & 0x3F)) % count;
    const std::size_t offset = selected * 0x1000 + (address & 0x0FFF);
    return chrData[offset % chrData.size()];
}

std::uint8_t Cartridge::readMmc5SplitChr(std::uint16_t address) const {
    if (mapper != 5 || chrData.empty()) return readChr(address);
    const std::size_t count = chrData.size() / 0x1000;
    if (count == 0) return 0;
    const std::size_t selected = static_cast<std::size_t>(mmc5VerticalSplitBank) % count;
    const std::size_t offset = selected * 0x1000 + (address & 0x0FFF);
    return chrData[offset % chrData.size()];
}

void Cartridge::writeChr(std::uint16_t address, std::uint8_t value) {
    if (!usesChrRam || address >= 0x2000 || chrData.empty()) return;
    std::size_t offset = address;
    if (mapper == 5) {
        const std::size_t count = chrData.size() / 0x400;
        if (!count) return;
        const std::size_t slot = address / 0x400;
        const std::size_t groupSize = std::size_t{1} << (3 - (mmc5ChrMode & 3));
        const std::size_t registerIndex =
            std::min<std::size_t>(7, ((slot / groupSize) + 1) * groupSize - 1);
        const std::size_t bankBase =
            ((static_cast<std::size_t>(mmc5ChrHigh) << 8) |
             mmc5ChrRegisters[registerIndex]) & ~(groupSize - 1);
        const std::size_t bank = (bankBase + (slot % groupSize)) % count;
        offset = bank * 0x400 + (address & 0x03FF);
    } else if (mapper == 1) {
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

    if (mapper != 0 && mapper != 1 && mapper != 2 && mapper != 3 && mapper != 4 && mapper != 5 && mapper != 7 && mapper != 40 && mapper != 66) {
        error = "Unsupported mapper " + std::to_string(mapper) +
                ". This version supports mapper 0 (NROM), mapper 1 (MMC1), mapper 2 (UxROM), mapper 3 (CNROM), mapper 4 (MMC3), mapper 5 (MMC5 initial banking), mapper 7 (AxROM), mapper 40 (NTDEC 2722), and mapper 66 (GxROM).";
        return false;
    }

    const std::size_t prgSize =
        static_cast<std::size_t>(bytes[4]) * PRG_BANK_SIZE;
    const std::size_t chrSize =
        static_cast<std::size_t>(bytes[5]) * CHR_BANK_SIZE;

    if (mapper == 5 && (prgSize < 128 * 1024 || prgSize > 1024 * 1024 || prgSize % 8192 != 0)) {
        error = "Mapper 5 requires 128 KiB to 1 MiB of PRG-ROM in 8 KiB banks.";
        return false;
    }
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

    if (mapper == 40 && (prgSize < 64 * 1024 || prgSize % 8192 != 0)) {
        error = "Mapper 40 requires at least 64 KiB of PRG-ROM in 8 KiB banks.";
        return false;
    }

    if (mapper == 7 &&
        (prgSize < GXMROM_PRG_BANK_SIZE ||
         prgSize > 8 * GXMROM_PRG_BANK_SIZE ||
         prgSize % GXMROM_PRG_BANK_SIZE != 0)) {
        error = "Mapper 7 requires 1 to 8 complete 32 KiB PRG-ROM banks.";
        return false;
    }
    if (mapper == 7 && chrSize != 0) {
        error = "Mapper 7 AxROM cartridges require CHR-RAM (zero CHR-ROM banks).";
        return false;
    }

    if (mapper == 3 &&
        (prgSize != PRG_BANK_SIZE && prgSize != 2 * PRG_BANK_SIZE)) {
        error = "Mapper 3 requires 16 or 32 KiB of fixed PRG-ROM.";
        return false;
    }
    if (mapper == 3 && chrSize > 4 * CHR_BANK_SIZE) {
        error = "Mapper 3 supports at most four 8 KiB CHR-ROM banks (32 KiB).";
        return false;
    }

    if (mapper == 2 &&
        (prgSize < 2 * PRG_BANK_SIZE || prgSize > 16 * PRG_BANK_SIZE ||
         prgSize % PRG_BANK_SIZE != 0)) {
        error = "Mapper 2 requires 2 to 16 complete 16 KiB PRG-ROM banks.";
        return false;
    }
    if (mapper == 2 && chrSize > CHR_BANK_SIZE) {
        error = "Mapper 2 supports at most one 8 KiB CHR-ROM bank.";
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
    if (mapper == 5) {
        const std::uint8_t lastBank = static_cast<std::uint8_t>((prgSize / 8192) - 1);
        loaded.mmc5PrgRegisters = {0, 0x80, 0x81, 0x82,
                                   static_cast<std::uint8_t>(0x80 | lastBank)};
    }
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
