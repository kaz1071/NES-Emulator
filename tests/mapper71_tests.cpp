#include "cartridge.hpp"

#include <cstdint>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
}

int main() {
    Cartridge cart;
    cart.mapper = 71;
    cart.prgRom.resize(4 * 0x4000);
    cart.chrData.resize(0x2000, 0);
    cart.usesChrRam = true;

    for (std::size_t bank = 0; bank < 4; ++bank) {
        cart.prgRom[bank * 0x4000] = static_cast<std::uint8_t>(0x40 + bank);
        cart.prgRom[bank * 0x4000 + 0x3FFF] = static_cast<std::uint8_t>(0x50 + bank);
    }

    check(cart.readPrg(0x8000) == 0x40, "lower PRG window starts at bank zero");
    check(cart.readPrg(0xC000) == 0x43, "upper PRG window is fixed to final bank");

    cart.writePrg(0x8000, 2);
    check(cart.readPrg(0x8000) == 0x42, "write selects lower 16 KiB bank");
    check(cart.readPrg(0xBFFF) == 0x52, "selected bank spans the complete lower window");
    check(cart.readPrg(0xC000) == 0x43, "bank switch does not affect fixed upper window");

    cart.writePrg(0xFFFF, 1);
    check(cart.readPrg(0x8000) == 0x41, "bank register is writable throughout the upper CPU address range");
    check(cart.readPrg(0xC000) == 0x43, "final PRG bank remains fixed after register write");

    const auto state = cart.saveState();
    cart.writePrg(0x8000, 3);
    check(cart.loadState(state), "mapper 71 state restores");
    check(cart.readPrg(0x8000) == 0x41, "save state restores selected PRG bank");
    check(cart.mapper71PrgBank == 1, "save state restores mapper 71 bank register");

    if (failures) return 1;
    std::cout << "All mapper 71 Camerica tests passed.\n";
    return 0;
}
