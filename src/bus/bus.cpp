#include "bus.hpp"
#include <cstddef>

void NESBus::connectCartridge(const Cartridge* loadedCartridge) {
    cartridge = loadedCartridge;
}

void NESBus::reset() {
    ram.fill(0);
    prgRam.fill(0);
}

std::uint8_t NESBus::read(std::uint16_t address) const {
    if (address < 0x2000) return ram[address & 0x07FF];
    if (address < 0x4000) return 0; // PPU register placeholders
    if (address < 0x4020) return 0; // APU, DMA, and controller placeholders
    if (address < 0x6000) return 0; // Expansion area not implemented
    if (address < 0x8000) return prgRam[address - 0x6000];

    if (cartridge != nullptr && !cartridge->prgRom.empty()) {
        const std::size_t offset =
            static_cast<std::size_t>(address - 0x8000) % cartridge->prgRom.size();
        return cartridge->prgRom[offset];
    }
    return 0;
}

void NESBus::write(std::uint16_t address, std::uint8_t value) {
    if (address < 0x2000) {
        ram[address & 0x07FF] = value;
    } else if (address >= 0x6000 && address < 0x8000) {
        prgRam[address - 0x6000] = value;
    }
    // PPU/APU/controller devices will receive writes when implemented.
}
