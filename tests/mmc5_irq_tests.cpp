#include "cartridge.hpp"

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

    // MMC5 scanline zero is a valid compare target.
    cartridge.writePrg(0x5203, 0);
    cartridge.writePrg(0x5204, 0x80);
    cartridge.clockScanline(true);
    expect(cartridge.irqPending(), "scanline-zero compare raises enabled IRQ");
    expect(cartridge.readPrg(0x5204) == 0xC0,
           "$5204 reports pending IRQ and in-frame status");
    expect(!cartridge.irqPending(), "reading $5204 acknowledges pending IRQ");
    expect(cartridge.readPrg(0x5204) == 0x40,
           "$5204 keeps in-frame status after acknowledgement");

    // A nonzero compare is reached on the matching visible scanline.
    cartridge.writePrg(0x5203, 2);
    cartridge.clockScanline(true); // scanline 0 of the next frame
    expect(!cartridge.irqPending(), "nonzero compare does not fire on scanline zero");
    cartridge.clockScanline(true); // scanline 1
    expect(!cartridge.irqPending(), "nonzero compare waits for its target");
    cartridge.clockScanline(true); // scanline 2
    expect(cartridge.irqPending(), "nonzero compare fires on matching scanline");

    // Disabling IRQ delivery masks the CPU line without erasing status.
    cartridge.writePrg(0x5204, 0);
    expect(!cartridge.irqPending(), "disabled MMC5 IRQ is not asserted to the CPU");
    expect((cartridge.readPrg(0x5204) & 0x80) != 0,
           "disabling IRQ delivery preserves pending status");

    cartridge.endMmc5Frame();
    expect(cartridge.readPrg(0x5204) == 0,
           "ending the frame clears in-frame and pending status");

    // A disabled renderer ends the frame and prevents a stale IRQ.
    cartridge.writePrg(0x5203, 0);
    cartridge.writePrg(0x5204, 0x80);
    cartridge.clockScanline(true);
    cartridge.clockScanline(false);
    expect(!cartridge.irqPending(), "rendering-disabled boundary clears MMC5 IRQ");

    if (failures != 0) {
        std::cerr << failures << " MMC5 IRQ test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All MMC5 IRQ tests passed\n";
    return EXIT_SUCCESS;
}
