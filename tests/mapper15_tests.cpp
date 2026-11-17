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
    cart.mapper = 15;
    cart.prgRom.resize(64 * 0x2000);
    cart.chrData.resize(0x2000, 0);
    cart.usesChrRam = true;
    for (std::size_t bank = 0; bank < 64; ++bank) {
        cart.prgRom[bank * 0x2000] = static_cast<std::uint8_t>(bank);
        cart.prgRom[bank * 0x2000 + 0x1FFF] = static_cast<std::uint8_t>(bank);
    }

    // Mode 0: 32 KiB groups, selected in 32 KiB units.
    cart.writePrg(0x8000, 0x04); // 32 KiB group 2
    check(cart.readPrg(0x8000) == 8, "mode 0 selects the lower 8 KiB page");
    check(cart.readPrg(0xC000) == 10, "mode 0 maps the full 32 KiB group");
    cart.writeChr(0x0010, 0xA5);
    check(cart.readChr(0x0010) == 0, "mode 0 protects CHR-RAM writes");

    // Mode 1: UNROM-style lower selectable 16 KiB and 128 KiB-block upper bank.
    cart.writePrg(0x8001, 0x02);
    check(cart.readPrg(0x8000) == 4, "mode 1 lower 16 KiB uses selected bank");
    check(cart.readPrg(0xC000) == 14, "mode 1 upper 16 KiB is fixed within selected 128 KiB block");
    cart.writeChr(0x0010, 0x5A);
    check(cart.readChr(0x0010) == 0x5A, "mode 1 allows CHR-RAM writes");

    // Mode 2: every 8 KiB slot maps the selected bank, with D7 as A13.
    cart.writePrg(0x8002, 0x85);
    check(cart.readPrg(0x8000) == 11, "mode 2 uses D7 as PRG A13");
    check(cart.readPrg(0xE000) == 11, "mode 2 repeats the selected 8 KiB bank");

    // Mode 3: 16 KiB pair selected by D0-D5.
    cart.writePrg(0x8003, 0x06);
    check(cart.readPrg(0x8000) == 12, "mode 3 selects lower 8 KiB half");
    check(cart.readPrg(0xA000) == 13, "mode 3 selects upper 8 KiB half");
    cart.writeChr(0x0010, 0xCC);
    check(cart.readChr(0x0010) == 0x5A, "mode 3 protects CHR-RAM writes");

    cart.writePrg(0x8003, 0x46);
    check(cart.mirroring == Mirroring::Horizontal, "data bit 6 selects horizontal mirroring");
    cart.writePrg(0x8003, 0x06);
    check(cart.mirroring == Mirroring::Vertical, "cleared data bit 6 selects vertical mirroring");

    const auto state = cart.saveState();
    cart.writePrg(0x8001, 0x00);
    check(cart.loadState(state), "mapper 15 state restores");
    check(cart.mapper15Register == 0x06 && cart.mapper15Mode == 3,
          "save state restores mapper 15 data and mode");

    if (failures) return 1;
    std::cout << "All mapper 15 tests passed.\n";
    return 0;
}
