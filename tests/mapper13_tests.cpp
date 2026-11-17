#include "cartridge.hpp"

#include <array>
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
    cart.mapper = 13;
    cart.usesChrRam = true;
    cart.prgRom.resize(32 * 1024);
    cart.chrData.resize(16 * 1024, 0);

    for (std::size_t bank = 0; bank < 4; ++bank)
        cart.chrData[bank * 0x1000] = static_cast<std::uint8_t>(0x20 + bank);

    check(cart.readChr(0x0000) == 0x20, "lower CHR window starts in fixed bank zero");
    check(cart.readChr(0x1000) == 0x20, "upper CHR window starts in bank zero");

    cart.writePrg(0x8000, 2);
    check(cart.readChr(0x0000) == 0x20, "bank register does not affect lower CHR window");
    check(cart.readChr(0x1000) == 0x22, "bank register selects upper 4 KiB bank");

    cart.writeChr(0x1000, 0xA5);
    check(cart.chrData[0x2000] == 0xA5, "upper-window CHR write reaches selected RAM bank");
    check(cart.readChr(0x1000) == 0xA5, "upper-window CHR read returns selected RAM data");

    cart.writePrg(0xFFFF, 3);
    check(cart.readChr(0x1000) == 0x23, "all two register bits select the fourth bank");
    cart.writeChr(0x1000, 0x5A);
    check(cart.chrData[0x3000] == 0x5A, "fourth CHR bank is writable");

    const auto state = cart.saveState();
    cart.writePrg(0x8000, 1);
    cart.writeChr(0x1000, 0x11);
    check(cart.loadState(state), "mapper 13 save state restores");
    check(cart.readChr(0x1000) == 0x5A, "save state restores selected bank and all 16 KiB CHR RAM");
    check(cart.mapper13ChrBank == 3, "save state restores mapper 13 bank register");

    if (failures) return 1;
    std::cout << "All mapper 13 CPROM tests passed.\n";
    return 0;
}
