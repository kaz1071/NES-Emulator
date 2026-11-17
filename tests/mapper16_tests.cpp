#include "cartridge.hpp"

#include <cstdint>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main() {
    Cartridge cart;
    cart.mapper = 16;
    cart.prgRom.resize(8 * 0x4000);
    cart.chrData.resize(32 * 0x400, 0);
    for (std::size_t bank = 0; bank < 8; ++bank) {
        cart.prgRom[bank * 0x4000] = static_cast<std::uint8_t>(bank);
        cart.prgRom[bank * 0x4000 + 0x3FFF] = static_cast<std::uint8_t>(bank);
    }
    for (std::size_t bank = 0; bank < 32; ++bank)
        cart.chrData[bank * 0x400] = static_cast<std::uint8_t>(bank);

    cart.writePrg(0x8008, 3);
    check(cart.readPrg(0x8000) == 3, "switchable lower 16 KiB PRG bank");
    check(cart.readPrg(0xC000) == 7, "final 16 KiB PRG bank remains fixed");
    cart.writePrg(0x6002, 9);
    check(cart.readChr(0x0800) == 9, "1 KiB CHR bank register maps its window");
    cart.writePrg(0x8009, 0);
    check(cart.mirroring == Mirroring::Vertical, "register 9 selects vertical mirroring");
    cart.writePrg(0x6009, 1);
    check(cart.mirroring == Mirroring::Horizontal, "mirroring register mirrors at low address window");
    cart.writePrg(0x8009, 2);
    check(cart.mirroring == Mirroring::OneScreenLower, "register 9 selects one-screen lower");
    cart.writePrg(0x8009, 3);
    check(cart.mirroring == Mirroring::OneScreenUpper, "register 9 selects one-screen upper");

    cart.writePrg(0x800B, 2);
    cart.writePrg(0x800C, 0);
    cart.writePrg(0x800A, 1);
    cart.clockCpuCycle();
    check(!cart.irqPending(), "IRQ waits for counter underflow");
    cart.clockCpuCycle();
    cart.clockCpuCycle();
    check(cart.irqPending(), "16-bit counter asserts IRQ on underflow");
    cart.writePrg(0x800A, 0);
    check(!cart.irqPending(), "disabling IRQ acknowledges pending request");

    cart.writePrg(0x800B, 5);
    cart.writePrg(0x800C, 0);
    cart.writePrg(0x800A, 1);
    const auto state = cart.saveState();
    cart.writePrg(0x8008, 1);
    cart.writePrg(0x800A, 0);
    check(cart.loadState(state), "mapper 16 state restores");
    check(cart.mapper16PrgBank == 3 && cart.mapper16IrqCounter == 5 && cart.mapper16IrqEnabled,
          "save state restores PRG and IRQ registers");

    if (failures) return 1;
    std::cout << "All mapper 16 tests passed.\n";
    return 0;
}
