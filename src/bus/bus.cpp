#include "bus.hpp"

void NESBus::connectCartridge(Cartridge* loadedCartridge) {
    cartridge = loadedCartridge;
    ppu.connectCartridge(loadedCartridge);
}

NESBus::State NESBus::saveState() const {
    State state{};
    state.ram = ram;
    state.prgRam = prgRam;
    state.controllerState = controllerState;
    state.controllerShift = controllerShift;
    state.controllerStrobe = controllerStrobe;
    state.oamDmaPending = oamDmaPending;
    state.oamDmaPage = oamDmaPage;
    state.ppu = ppu.saveState();
    state.apu = apu.saveState();
    if (cartridge != nullptr) state.cartridge = cartridge->saveState();
    return state;
}

bool NESBus::loadState(const State& state) {
    if (cartridge == nullptr || !cartridge->loadState(state.cartridge)) return false;
    ram = state.ram;
    prgRam = state.prgRam;
    controllerState = state.controllerState;
    controllerShift = state.controllerShift;
    controllerStrobe = state.controllerStrobe;
    oamDmaPending = state.oamDmaPending;
    oamDmaPage = state.oamDmaPage;
    ppu.loadState(state.ppu);
    apu.loadState(state.apu);
    return true;
}

void NESBus::reset() {
    ram.fill(0);
    prgRam.fill(0);
    controllerState.fill(0);
    controllerShift.fill(0);
    controllerStrobe = false;
    oamDmaPending = false;
    oamDmaPage = 0;
    ppu.reset();
    apu.reset();
}

void NESBus::clockPpu() {
    ppu.clock();
}

void NESBus::clockCpuCycle() {
    if (cartridge != nullptr) cartridge->clockCpuCycle();
    apu.clock();
}

std::vector<std::int16_t> NESBus::takeAudioSamples() {
    return apu.takeSamples();
}

bool NESBus::consumeNmi() {
    return ppu.consumeNmi();
}

bool NESBus::irqPending() const {
    return apu.irqPending() || (cartridge != nullptr && cartridge->irqPending());
}

void NESBus::setControllerState(std::uint8_t port, std::uint8_t buttons) {
    if (port < controllerState.size()) {
        controllerState[port] = buttons;
        if (controllerStrobe) controllerShift[port] = buttons;
    }
}

std::uint8_t NESBus::read(std::uint16_t address) {
    if (address < 0x2000) return ram[address & 0x07FF];
    if (address < 0x4000) return ppu.cpuRead(address);
    if (address == 0x4015) return apu.readStatus();
    if (address == 0x5015 && cartridge != nullptr && cartridge->mapper == 5) return apu.readMmc5Status();
    if (address == 0x4016 || address == 0x4017) {
        const std::size_t port = address - 0x4016;
        if (controllerStrobe) controllerShift[port] = controllerState[port];
        const std::uint8_t result = static_cast<std::uint8_t>(
            0x40 | (controllerShift[port] & 1));
        if (!controllerStrobe) {
            controllerShift[port] = static_cast<std::uint8_t>(
                (controllerShift[port] >> 1) | 0x80);
        }
        return result;
    }
    if (address < 0x4020) return 0; // Other APU and I/O registers are placeholders
    if (address < 0x6000) {
        if (cartridge != nullptr && cartridge->mapper == 5 &&
            (address == 0x5204 || address >= 0x5C00)) return cartridge->readPrg(address);
        return 0;
    }
    if (address < 0x8000 && cartridge != nullptr && cartridge->mapper == 40)
        return cartridge->readPrg(address);
    if (address < 0x8000) {
        if (cartridge != nullptr && cartridge->mapper == 5) return cartridge->readPrg(address);
        if (cartridge != nullptr && !cartridge->prgRamEnabled()) return 0;
        return prgRam[address - 0x6000];
    }
    if (cartridge != nullptr) return cartridge->readPrg(address);
    return 0;
}

void NESBus::write(std::uint16_t address, std::uint8_t value) {
    if (address < 0x2000) {
        ram[address & 0x07FF] = value;
    } else if (address < 0x4000) {
        ppu.cpuWrite(address, value);
    } else if (address == 0x4014) {
        // The CPU performs the 513/514-cycle transfer after this write ends.
        // Keep the request here so DMA reads and OAM writes can be interleaved
        // with PPU clocks by the CPU rather than copied instantaneously.
        oamDmaPage = value;
        oamDmaPending = true;
    } else if (address == 0x4016) {
        const bool nextStrobe = (value & 1) != 0;
        if (nextStrobe || controllerStrobe) {
            controllerShift = controllerState;
        }
        controllerStrobe = nextStrobe;
    } else if ((address >= 0x4000 && address <= 0x4013) ||
               address == 0x4015 || address == 0x4017) {
        apu.write(address, value);
    } else if (address >= 0x5000 && address < 0x6000 && cartridge != nullptr && cartridge->mapper == 5) {
        if ((address >= 0x5000 && address <= 0x5007) || address == 0x5015) apu.writeMmc5(address, value);
        else cartridge->writePrg(address, value);
    } else if (address >= 0x6000 && address < 0x8000) {
        if (cartridge != nullptr && (cartridge->mapper == 40 || cartridge->mapper == 5)) { cartridge->writePrg(address, value); return; }
        if (cartridge == nullptr || cartridge->prgRamWritable())
            prgRam[address - 0x6000] = value;
    } else if (address >= 0x8000 && cartridge != nullptr) {
        cartridge->writePrg(address, value);
    }
}

void NESBus::renderFrame(std::array<std::uint32_t, 256 * 240>& pixels) const {
    ppu.renderFrame(pixels);
}

bool NESBus::consumeOamDmaRequest(std::uint8_t& page) {
    if (!oamDmaPending) return false;
    page = oamDmaPage;
    oamDmaPending = false;
    return true;
}

void NESBus::writeOamDmaByte(std::uint8_t value) {
    // DMA writes through OAMDATA, preserving the current OAMADDR and wraparound.
    ppu.cpuWrite(0x2004, value);
}


bool NESBus::consumeDmcDmaRequest(std::uint16_t& address) {
    return apu.consumeDmcDmaRequest(address);
}

void NESBus::completeDmcDma(std::uint8_t value) {
    apu.completeDmcDma(value);
}
