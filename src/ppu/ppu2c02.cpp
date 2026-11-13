#include "ppu2c02.hpp"

#include <algorithm>
#include <cstddef>

void PPU2C02::connectCartridge(Cartridge* loadedCartridge) {
    cartridge = loadedCartridge;
}

void PPU2C02::reset() {
    nametableRam.fill(0);
    paletteRam.fill(0);
    oam.fill(0);
    control = mask = status = oamAddress = openBus = readBuffer = 0;
    vramAddress = temporaryAddress = 0;
    fineX = 0;
    writeSecond = false;
}

std::uint16_t PPU2C02::mapNametableAddress(std::uint16_t address) const {
    const std::uint16_t relative = static_cast<std::uint16_t>((address - 0x2000) & 0x0FFF);
    const std::uint16_t table = relative / 0x0400;
    const std::uint16_t offset = relative & 0x03FF;
    std::uint16_t physicalTable = table;

    if (cartridge == nullptr || cartridge->mirroring == Mirroring::Horizontal) {
        physicalTable = table / 2;
    } else if (cartridge->mirroring == Mirroring::Vertical) {
        physicalTable = table % 2;
    }

    return static_cast<std::uint16_t>(physicalTable * 0x0400 + offset);
}

std::uint8_t PPU2C02::mapPaletteAddress(std::uint16_t address) {
    std::uint8_t index = static_cast<std::uint8_t>((address - 0x3F00) & 0x1F);
    if (index == 0x10 || index == 0x14 || index == 0x18 || index == 0x1C) {
        index = static_cast<std::uint8_t>(index - 0x10);
    }
    return index;
}

std::uint8_t PPU2C02::ppuRead(std::uint16_t address) const {
    address &= 0x3FFF;
    if (address < 0x2000) {
        if (cartridge != nullptr && !cartridge->chrData.empty()) {
            return cartridge->chrData[address % cartridge->chrData.size()];
        }
        return 0;
    }
    if (address < 0x3F00) {
        return nametableRam[mapNametableAddress(address)];
    }
    return paletteRam[mapPaletteAddress(address)] & 0x3F;
}

void PPU2C02::ppuWrite(std::uint16_t address, std::uint8_t value) {
    address &= 0x3FFF;
    if (address < 0x2000) {
        if (cartridge != nullptr && cartridge->usesChrRam &&
            address < cartridge->chrData.size()) {
            cartridge->chrData[address] = value;
        }
        return;
    }
    if (address < 0x3F00) {
        nametableRam[mapNametableAddress(address)] = value;
        return;
    }
    paletteRam[mapPaletteAddress(address)] = value & 0x3F;
}

std::uint8_t PPU2C02::cpuRead(std::uint16_t registerAddress) {
    const std::uint8_t reg = static_cast<std::uint8_t>(registerAddress & 0x07);
    std::uint8_t value = openBus;

    if (reg == 2) {
        value = static_cast<std::uint8_t>((status & 0xE0) | (openBus & 0x1F));
        status &= 0x7F;
        writeSecond = false;
    } else if (reg == 4) {
        value = oam[oamAddress];
    } else if (reg == 7) {
        const std::uint16_t address = vramAddress & 0x3FFF;
        const std::uint8_t fetched = ppuRead(address);
        if (address >= 0x3F00) {
            value = fetched;
            readBuffer = ppuRead(static_cast<std::uint16_t>(address - 0x1000));
        } else {
            value = readBuffer;
            readBuffer = fetched;
        }
        vramAddress = static_cast<std::uint16_t>(
            (vramAddress + ((control & 0x04) ? 32 : 1)) & 0x7FFF);
    }

    openBus = value;
    return value;
}

void PPU2C02::cpuWrite(std::uint16_t registerAddress, std::uint8_t value) {
    openBus = value;
    switch (registerAddress & 0x07) {
        case 0:
            control = value;
            temporaryAddress = static_cast<std::uint16_t>(
                (temporaryAddress & 0xF3FF) | ((value & 0x03) << 10));
            break;
        case 1:
            mask = value;
            break;
        case 3:
            oamAddress = value;
            break;
        case 4:
            oam[oamAddress++] = value;
            break;
        case 5:
            if (!writeSecond) {
                fineX = value & 0x07;
                temporaryAddress = static_cast<std::uint16_t>(
                    (temporaryAddress & 0xFFE0) | (value >> 3));
            } else {
                temporaryAddress = static_cast<std::uint16_t>(
                    (temporaryAddress & 0x8C1F) |
                    ((value & 0x07) << 12) |
                    ((value & 0xF8) << 2));
            }
            writeSecond = !writeSecond;
            break;
        case 6:
            if (!writeSecond) {
                temporaryAddress = static_cast<std::uint16_t>(
                    (temporaryAddress & 0x00FF) | ((value & 0x3F) << 8));
            } else {
                temporaryAddress = static_cast<std::uint16_t>(
                    (temporaryAddress & 0x7F00) | value);
                vramAddress = temporaryAddress;
            }
            writeSecond = !writeSecond;
            break;
        case 7:
            ppuWrite(vramAddress, value);
            vramAddress = static_cast<std::uint16_t>(
                (vramAddress + ((control & 0x04) ? 32 : 1)) & 0x7FFF);
            break;
        default:
            break;
    }
}
