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
    cart.mapper = 94;
    cart.prgRom.resize(8 * 0x4000);
    cart.chrData.resize(0x2000, 0);
    cart.usesChrRam = true;
    for (std::size_t bank = 0; bank < 8; ++bank) {
        cart.prgRom[bank * 0x4000] = static_cast<std::uint8_t>(0x20 + bank);
        cart.prgRom[bank * 0x4000 + 0x3FFF] = static_cast<std::uint8_t>(0x30 + bank);
    }

    check(cart.readPrg(0x8000) == 0x20, "lower window starts at bank zero");
    check(cart.readPrg(0xC000) == 0x27, "upper window is fixed to final bank");
    cart.writePrg(0x8000, 0x1C);
    check(cart.readPrg(0x8000) == 0x27, "bits 2-4 select bank seven");
    check(cart.readPrg(0xBFFF) == 0x37, "selected bank fills lower 16 KiB window");
    check(cart.readPrg(0xC000) == 0x27, "bank switch leaves fixed upper window unchanged");

    cart.writePrg(0xFFFF, 0x08);
    check(cart.readPrg(0x8000) == 0x22, "register writes throughout cartridge space select bank two");
    const auto state = cart.saveState();
    cart.writePrg(0x8000, 0x00);
    check(cart.loadState(state), "mapper 94 state restores");
    check(cart.readPrg(0x8000) == 0x22, "save state restores selected bank");
    check(cart.mapper94PrgBank == 2, "save state restores mapper 94 register");

    if (failures) return 1;
    std::cout << "All mapper 94 UN1ROM tests passed.\n";
    return 0;
}
