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


void PPU2C02::renderFrame(std::array<std::uint32_t, 256 * 240>& pixels) const {
    static constexpr std::array<std::uint32_t, 64> colors = {
        0xFF666666,0xFF002A88,0xFF1412A7,0xFF3B00A4,0xFF5C007E,0xFF6E0040,0xFF6C0600,0xFF561D00,
        0xFF333500,0xFF0B4800,0xFF005200,0xFF004F08,0xFF00404D,0xFF000000,0xFF000000,0xFF000000,
        0xFFADADAD,0xFF155FD9,0xFF4240FF,0xFF7527FE,0xFFA01ACC,0xFFB71E7B,0xFFB53120,0xFF994E00,
        0xFF6B6D00,0xFF388700,0xFF0C9300,0xFF008F32,0xFF007C8D,0xFF000000,0xFF000000,0xFF000000,
        0xFFFFFEFF,0xFF64B0FF,0xFF9290FF,0xFFC676FF,0xFFF36AFF,0xFFFE6ECC,0xFFFE8170,0xFFEA9E22,
        0xFFBCBE00,0xFF88D800,0xFF5CE430,0xFF45E082,0xFF48CDDE,0xFF4F4F4F,0xFF000000,0xFF000000,
        0xFFFFFEFF,0xFFC0DFFF,0xFFD3D2FF,0xFFE8C8FF,0xFFFBC2FF,0xFFFEC4EA,0xFFFECCC5,0xFFF7D8A5,
        0xFFE4E594,0xFFCFEF96,0xFFBDF4AB,0xFFB3F3CC,0xFFB5EBF2,0xFFB8B8B8,0xFF000000,0xFF000000
    };
    const std::uint16_t nametableBase =
        static_cast<std::uint16_t>(0x2000 + (control & 0x03) * 0x0400);
    const std::uint16_t patternBase = (control & 0x10) ? 0x1000 : 0x0000;
    const std::uint32_t backdropColor = colors[ppuRead(0x3F00) & 0x3F];

    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 256; ++x) {
            std::uint32_t output = backdropColor;
            if ((mask & 0x08) != 0 && (x >= 8 || (mask & 0x02) != 0)) {
                const int tileX = x / 8;
                const int tileY = y / 8;
                const int pixelX = x & 7;
                const int pixelY = y & 7;
                const std::uint8_t tile = ppuRead(static_cast<std::uint16_t>(
                    nametableBase + tileY * 32 + tileX));
                const std::uint16_t patternAddress = static_cast<std::uint16_t>(
                    patternBase + tile * 16 + pixelY);
                const std::uint8_t low = ppuRead(patternAddress);
                const std::uint8_t high = ppuRead(
                    static_cast<std::uint16_t>(patternAddress + 8));
                const int bit = 7 - pixelX;
                const std::uint8_t pixel = static_cast<std::uint8_t>(
                    ((low >> bit) & 1) | (((high >> bit) & 1) << 1));
                if (pixel != 0) {
                    const std::uint16_t attributeAddress = static_cast<std::uint16_t>(
                        nametableBase + 0x03C0 + (tileY / 4) * 8 + (tileX / 4));
                    const std::uint8_t attribute = ppuRead(attributeAddress);
                    const int shift = ((tileY & 2) ? 4 : 0) + ((tileX & 2) ? 2 : 0);
                    const std::uint8_t palette = static_cast<std::uint8_t>(
                        (attribute >> shift) & 0x03);
                    const std::uint16_t paletteAddress = static_cast<std::uint16_t>(
                        0x3F00 + palette * 4 + pixel);
                    output = colors[ppuRead(paletteAddress) & 0x3F];
                }
            }
            pixels[static_cast<std::size_t>(y) * 256 + x] = output;
        }
    }
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
