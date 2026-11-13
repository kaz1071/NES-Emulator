#include "bus.hpp"
#include <cstddef>

void NESBus::connectCartridge(Cartridge* loadedCartridge) {
    cartridge = loadedCartridge;
    ppu.connectCartridge(loadedCartridge);
}

void NESBus::reset() {
    ram.fill(0);
    prgRam.fill(0);
    ppu.reset();
}

std::uint8_t NESBus::read(std::uint16_t address) {
    if (address < 0x2000) return ram[address & 0x07FF];
    if (address < 0x4000) return ppu.cpuRead(address);
    if (address < 0x4020) return 0; // APU, DMA, and controller placeholders
    if (address < 0x6000) return 0; // Expansion area not implemented
    if (address < 0x8000) return prgRam[address - 0x6000];

    if (cartridge != nullptr) return cartridge->readPrg(address);
    return 0;
}

void NESBus::write(std::uint16_t address, std::uint8_t value) {
    if (address < 0x2000) {
        ram[address & 0x07FF] = value;
    } else if (address < 0x4000) {
        ppu.cpuWrite(address, value);
    } else if (address >= 0x6000 && address < 0x8000) {
        prgRam[address - 0x6000] = value;
    } else if (address >= 0x8000 && cartridge != nullptr) {
        cartridge->writePrg(address, value);
    }
    // APU/controller devices are not implemented.
}

void NESBus::renderFrame(std::array<std::uint32_t, 256 * 240>& pixels) const {
    ppu.renderFrame(pixels);
}
