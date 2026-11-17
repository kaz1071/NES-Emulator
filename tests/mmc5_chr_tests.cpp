#include "cartridge.hpp"

#include <algorithm>
#include <cstddef>
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
}

int main() {
    Cartridge cartridge{};
    cartridge.mapper = 5;
    cartridge.chrData.resize(16 * 0x400);
    for (std::size_t bank = 0; bank < 16; ++bank) {
        std::fill_n(cartridge.chrData.begin() + static_cast<std::ptrdiff_t>(bank * 0x400),
                    0x400, static_cast<std::uint8_t>(bank));
    }

    // 1 KiB mode: every sprite slot has its own register.
    cartridge.writePrg(0x5101, 3);
    for (std::uint8_t slot = 0; slot < 8; ++slot)
        cartridge.writePrg(static_cast<std::uint16_t>(0x5120 + slot), slot);
    for (std::uint16_t slot = 0; slot < 8; ++slot)
        expect(cartridge.readMmc5Chr(static_cast<std::uint16_t>(slot * 0x400), false) == slot,
               "1KB mode selects each sprite CHR register");

    // 2 KiB mode uses the even-aligned pair selected by registers 1, 3, 5, 7.
    cartridge.writePrg(0x5101, 2);
    cartridge.writePrg(0x5121, 5);
    cartridge.writePrg(0x5123, 8);
    cartridge.writePrg(0x5125, 10);
    cartridge.writePrg(0x5127, 14);
    expect(cartridge.readMmc5Chr(0x0000, false) == 4 &&
           cartridge.readMmc5Chr(0x0400, false) == 5,
           "2KB mode aligns and maps the first pair");
    expect(cartridge.readMmc5Chr(0x0800, false) == 8 &&
           cartridge.readMmc5Chr(0x0C00, false) == 9,
           "2KB mode maps the second pair");

    // 4 KiB mode uses the final register in each four-slot group.
    cartridge.writePrg(0x5101, 1);
    cartridge.writePrg(0x5123, 11);
    expect(cartridge.readMmc5Chr(0x0000, false) == 8 &&
           cartridge.readMmc5Chr(0x0C00, false) == 11,
           "4KB mode aligns a four-bank group");

    // 8 KiB mode uses register 7 and maps a complete eight-bank group.
    cartridge.writePrg(0x5101, 0);
    cartridge.writePrg(0x5127, 9);
    expect(cartridge.readMmc5Chr(0x0000, false) == 8 &&
           cartridge.readMmc5Chr(0x1C00, false) == 15,
           "8KB mode aligns and maps all eight banks");

    // Background CHR registers are independent from sprite CHR registers.
    cartridge.writePrg(0x5101, 3);
    cartridge.writePrg(0x5120, 8);
    cartridge.writePrg(0x5128, 2);
    cartridge.writePrg(0x5129, 4);
    cartridge.writePrg(0x512A, 6);
    cartridge.writePrg(0x512B, 8);
    expect(cartridge.readMmc5Chr(0x0000, true) == 2 &&
           cartridge.readMmc5Chr(0x0400, true) == 4 &&
           cartridge.readMmc5Chr(0x0800, true) == 6 &&
           cartridge.readMmc5Chr(0x0C00, true) == 8,
           "background CHR registers select independent banks");
    expect(cartridge.readMmc5Chr(0x0000, false) == 8,
           "background bank writes do not alter sprite banks");

    const CartridgeState saved = cartridge.saveState();
    Cartridge restored{};
    restored.mapper = 5;
    restored.chrData = cartridge.chrData;
    expect(restored.loadState(saved), "MMC5 CHR state restores");
    expect(restored.readMmc5Chr(0x0800, true) == 6,
           "background CHR registers survive save states");

    // The two high bits in $5130 extend each CHR bank register.
    cartridge.chrData.resize(512 * 0x400);
    cartridge.chrData[0] = 0x5A;
    cartridge.chrData[256 * 0x400] = 0xA5;
    cartridge.writePrg(0x5101, 3);
    cartridge.writePrg(0x5130, 1);
    cartridge.writePrg(0x5120, 0);
    expect(cartridge.readMmc5Chr(0x0000, false) == 0xA5,
           "$5130 high bits extend the selected CHR bank");

    if (failures != 0) {
        std::cerr << failures << " MMC5 CHR test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All MMC5 CHR tests passed\n";
    return EXIT_SUCCESS;
}
