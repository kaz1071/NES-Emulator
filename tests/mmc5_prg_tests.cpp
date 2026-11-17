#include "cartridge.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {
int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void fillPrgBanks(Cartridge& cartridge) {
    cartridge.mapper = 5;
    cartridge.prgRom.resize(16 * 0x2000);
    for (std::size_t bank = 0; bank < 16; ++bank) {
        std::fill_n(cartridge.prgRom.begin() +
                        static_cast<std::ptrdiff_t>(bank * 0x2000),
                    0x2000, static_cast<std::uint8_t>(bank));
    }
}

std::uint8_t readSlot(Cartridge& cartridge, std::size_t slot) {
    return cartridge.readPrg(static_cast<std::uint16_t>(0x8000 + slot * 0x2000));
}
}

int main() {
    Cartridge cartridge{};
    fillPrgBanks(cartridge);

    // Mode 3: four independent 8 KiB windows.
    cartridge.writePrg(0x5100, 3);
    cartridge.writePrg(0x5114, 0x80 | 3);
    cartridge.writePrg(0x5115, 0x80 | 5);
    cartridge.writePrg(0x5116, 0x80 | 7);
    cartridge.writePrg(0x5117, 0x80 | 9);
    for (std::size_t slot = 0; slot < 4; ++slot)
        expect(readSlot(cartridge, slot) == 3 + slot * 2,
               "mode 3 maps four independently selected 8KB banks");

    // Mode 2: one 16 KiB window and two 8 KiB windows.
    cartridge.writePrg(0x5100, 2);
    cartridge.writePrg(0x5115, 0x80 | 6);
    cartridge.writePrg(0x5116, 0x80 | 10);
    cartridge.writePrg(0x5117, 0x80 | 12);
    expect(readSlot(cartridge, 0) == 6 && readSlot(cartridge, 1) == 7,
           "mode 2 aligns its 16KB window");
    expect(readSlot(cartridge, 2) == 10 && readSlot(cartridge, 3) == 12,
           "mode 2 maps its final two 8KB windows");

    // Mode 1: two 16 KiB windows.
    cartridge.writePrg(0x5100, 1);
    cartridge.writePrg(0x5115, 0x80 | 4);
    cartridge.writePrg(0x5117, 0x80 | 8);
    expect(readSlot(cartridge, 0) == 4 && readSlot(cartridge, 1) == 5,
           "mode 1 maps the lower 16KB window");
    expect(readSlot(cartridge, 2) == 8 && readSlot(cartridge, 3) == 9,
           "mode 1 maps the upper 16KB window");

    // Mode 0: one 32 KiB window.
    cartridge.writePrg(0x5100, 0);
    cartridge.writePrg(0x5117, 0x80 | 4);
    for (std::size_t slot = 0; slot < 4; ++slot)
        expect(readSlot(cartridge, slot) == 4 + slot,
               "mode 0 aligns and maps one 32KB window");

    // A register with bit 7 clear selects PRG-RAM rather than PRG-ROM.
    cartridge.writePrg(0x5100, 3);
    cartridge.mmc5PrgRam[0] = 0xA6;
    cartridge.writePrg(0x5114, 0x00);
    expect(readSlot(cartridge, 0) == 0xA6,
           "mode 3 register bit 7 selects PRG-RAM");

    const CartridgeState saved = cartridge.saveState();
    Cartridge restored{};
    fillPrgBanks(restored);
    expect(restored.loadState(saved), "MMC5 PRG registers restore from save state");
    expect(readSlot(restored, 0) == 0xA6,
           "MMC5 PRG-RAM mapping survives save-state restoration");

    if (failures != 0) {
        std::cerr << failures << " MMC5 PRG test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All MMC5 PRG tests passed\n";
    return EXIT_SUCCESS;
}
