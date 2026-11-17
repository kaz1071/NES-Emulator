#pragma once

#include <cstdint>
#include <array>
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

struct CartridgeState {
    Mirroring mirroring = Mirroring::Horizontal;
    std::uint8_t mapper = 0, prgBank = 0, chrBank = 0;
    std::uint8_t mmc1Shift = 0x10, mmc1Control = 0x0C;
    std::uint8_t mmc1ChrBank0 = 0, mmc1ChrBank1 = 0, mmc1PrgBank = 0;
    std::array<std::uint8_t, 8> mmc3BankRegisters{};
    std::uint8_t mmc3BankSelect = 0;
    bool mmc3PrgRamEnabled = true, mmc3PrgRamWriteProtected = false;
    std::uint8_t mmc3IrqLatch = 0, mmc3IrqCounter = 0;
    bool mmc3IrqReload = false, mmc3IrqEnabled = false, mmc3IrqPending = false;
    std::uint8_t mapper40PrgBank = 0;
    std::uint8_t mapper13ChrBank = 0;
    std::uint8_t mapper71PrgBank = 0;
    std::uint8_t mapper94PrgBank = 0;
    std::uint8_t mapper94PrgBank = 0;
    std::array<std::uint8_t, 4> mmc2ChrBanks{};
    std::uint8_t mmc2PrgBank = 0;
    bool mmc2Latch0Fe = false, mmc2Latch1Fe = false;
    std::uint16_t mapper40IrqCounter = 0;
    bool mapper40IrqEnabled = false, mapper40IrqPending = false;
    bool usesChrRam = false;
    std::array<std::uint8_t, 16384> chrRam{};
    std::array<std::uint8_t, 5> mmc5PrgRegisters{};
    std::array<std::uint8_t, 8> mmc5ChrRegisters{};
    std::array<std::uint8_t, 4> mmc5BgChrRegisters{};
    std::array<std::uint8_t, 1024 * 1024> mmc5PrgRam{};
    std::uint8_t mmc5PrgMode = 3, mmc5ChrMode = 3;
    std::uint8_t mmc5RamProtect1 = 0, mmc5RamProtect2 = 0;
    std::uint8_t mmc5ChrHigh = 0;
    std::array<std::uint8_t, 1024> mmc5ExRam{};
    std::uint8_t mmc5NametableMap = 0, mmc5ExRamMode = 0;
    std::uint8_t mmc5FillTile = 0, mmc5FillAttribute = 0;
    std::uint8_t mmc5VerticalSplit = 0, mmc5VerticalSplitScroll = 0, mmc5VerticalSplitBank = 0;
    std::uint8_t mmc5IrqCompare = 0, mmc5IrqScanline = 0;
    std::uint8_t mmc5MultiplyA = 0, mmc5MultiplyB = 0;
    bool mmc5IrqEnabled = false, mmc5IrqPending = false, mmc5InFrame = false;
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
    std::array<std::uint8_t, 8> mmc3BankRegisters{};
    std::uint8_t mmc3BankSelect = 0;
    bool mmc3PrgRamEnabled = true;
    bool mmc3PrgRamWriteProtected = false;
    std::uint8_t mmc3IrqLatch = 0, mmc3IrqCounter = 0;
    bool mmc3IrqReload = false, mmc3IrqEnabled = false, mmc3IrqPending = false;
    std::uint8_t mapper40PrgBank = 0;
    std::uint8_t mapper13ChrBank = 0;
    std::uint8_t mapper71PrgBank = 0;
    std::uint16_t mapper40IrqCounter = 0;
    bool mapper40IrqEnabled = false;
    bool mapper40IrqPending = false;
    std::array<std::uint8_t, 4> mmc2ChrBanks{};
    std::uint8_t mmc2PrgBank = 0;
    mutable bool mmc2Latch0Fe = false, mmc2Latch1Fe = false;
    std::array<std::uint8_t, 5> mmc5PrgRegisters{};
    std::array<std::uint8_t, 8> mmc5ChrRegisters{};
    std::array<std::uint8_t, 4> mmc5BgChrRegisters{};
    std::array<std::uint8_t, 1024 * 1024> mmc5PrgRam{};
    std::uint8_t mmc5PrgMode = 3, mmc5ChrMode = 3;
    std::uint8_t mmc5RamProtect1 = 0, mmc5RamProtect2 = 0;
    std::uint8_t mmc5ChrHigh = 0;
    std::array<std::uint8_t, 1024> mmc5ExRam{};
    std::uint8_t mmc5NametableMap = 0, mmc5ExRamMode = 0;
    std::uint8_t mmc5FillTile = 0, mmc5FillAttribute = 0;
    std::uint8_t mmc5VerticalSplit = 0, mmc5VerticalSplitScroll = 0, mmc5VerticalSplitBank = 0;
    std::uint8_t mmc5IrqCompare = 0, mmc5IrqScanline = 0;
    std::uint8_t mmc5MultiplyA = 0, mmc5MultiplyB = 0;
    bool mmc5IrqEnabled = false, mmc5IrqPending = false, mmc5InFrame = false;
    CartridgeState saveState() const;
    bool loadState(const CartridgeState& state);
    std::uint64_t romFingerprint() const;
    bool prgRamEnabled() const;
    bool prgRamWritable() const;
    bool irqPending() const;
    void clockScanline(bool rendering = true);
    void endMmc5Frame();
    void clockCpuCycle();

    std::uint8_t readPrg(std::uint16_t address);
    void writePrg(std::uint16_t address, std::uint8_t value);
    std::uint8_t readChr(std::uint16_t address) const;
    std::uint8_t readMmc5Chr(std::uint16_t address, bool background) const;
    std::uint8_t readMmc5ExtendedChr(std::uint16_t address, std::uint8_t bank) const;
    std::uint8_t readMmc5SplitChr(std::uint16_t address) const;
    void writeChr(std::uint16_t address, std::uint8_t value);
};

bool loadCartridge(const std::filesystem::path& path, Cartridge& cartridge,
                   std::string& error);
