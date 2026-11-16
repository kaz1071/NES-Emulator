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
    scrollX = scrollY = 0;
    writeSecond = false;
    ppuCycle = 0;
    scanline = 261;
    nmiPending = false;
    vblankEvents = 0;
    vblankStatusReads = 0;
    lastStatusRead = 0;
    lineScrollAddress.fill(0);
    lineFineX.fill(0);
}

std::uint8_t PPU2C02::backgroundPixelAt(
    int x, int y, std::uint8_t* paletteSlot) const {
    if (paletteSlot != nullptr) *paletteSlot = 0;
    if (x < 0 || x >= 256 || y < 0 || y >= 240) return 0;

    const std::uint16_t scroll = lineScrollAddress[static_cast<std::size_t>(y)];
    const int coarseX = scroll & 0x001F;
    const int coarseY = (scroll >> 5) & 0x001F;
    const int baseTableX = (scroll >> 10) & 1;
    const int tableY = (scroll >> 11) & 1;
    const int fineY = (scroll >> 12) & 7;
    const int pixelX = static_cast<int>(lineFineX[static_cast<std::size_t>(y)]) + x;
    const int tileOffsetX = coarseX + pixelX / 8;
    const int tableX = (baseTableX + tileOffsetX / 32) & 1;
    const int tileX = tileOffsetX & 31;
    const int tileY = coarseY;
    const int finePixelX = pixelX & 7;
    const std::uint16_t tableBase = static_cast<std::uint16_t>(
        0x2000 + (tableY * 2 + tableX) * 0x400);
    const std::uint8_t tile = ppuRead(static_cast<std::uint16_t>(
        tableBase + tileY * 32 + tileX));
    const std::uint16_t pattern = static_cast<std::uint16_t>(
        ((control & 0x10) ? 0x1000 : 0) + tile * 16 + fineY);
    const int bit = 7 - finePixelX;
    const std::uint8_t low = ppuRead(pattern);
    const std::uint8_t high = ppuRead(static_cast<std::uint16_t>(pattern + 8));
    const std::uint8_t pixel = static_cast<std::uint8_t>(
        ((low >> bit) & 1) | (((high >> bit) & 1) << 1));
    if (pixel == 0) return 0;

    const std::uint16_t attributeAddress = static_cast<std::uint16_t>(
        tableBase + 0x3C0 + (tileY / 4) * 8 + tileX / 4);
    const std::uint8_t attribute = ppuRead(attributeAddress);
    const int shift = ((tileY & 2) ? 4 : 0) + ((tileX & 2) ? 2 : 0);
    const std::uint8_t palette = static_cast<std::uint8_t>((attribute >> shift) & 3);
    if (paletteSlot != nullptr) {
        *paletteSlot = static_cast<std::uint8_t>(palette * 4 + pixel);
    }
    return pixel;
}

void PPU2C02::updateSpriteZeroHit() {
    // Sprite-zero hit is raised when opaque sprite 0 and background pixels
    // overlap on a visible pixel. The hit flag is independent of priority.
    if ((status & 0x40) || scanline < 0 || scanline >= 240 ||
        ppuCycle < 1 || ppuCycle > 256 ||
        (mask & 0x18) != 0x18) return;

    const int x = ppuCycle - 1;
    const int y = scanline;
    if (x == 255 || (x < 8 && ((mask & 0x06) != 0x06))) return;

    const int spriteX = oam[3];
    const int spriteY = static_cast<int>(oam[0]) + 1;
    const bool tall = (control & 0x20) != 0;
    const int spriteHeight = tall ? 16 : 8;
    if (x < spriteX || x >= spriteX + 8 ||
        y < spriteY || y >= spriteY + spriteHeight) return;

    // Use this scanline's captured Loopy scroll state for the background.
    if (backgroundPixelAt(x, y) == 0) return;

    // Decode sprite 0, including horizontal/vertical flip and 8x16 mode.
    const std::uint8_t tile = oam[1];
    const std::uint8_t attributes = oam[2];
    const int spriteColumn = x - spriteX;
    const int spriteRow = y - spriteY;
    const int sourceX = (attributes & 0x40) ? 7 - spriteColumn : spriteColumn;
    const int sourceY = (attributes & 0x80)
        ? spriteHeight - 1 - spriteRow : spriteRow;
    std::uint16_t spritePattern;
    if (tall) {
        spritePattern = static_cast<std::uint16_t>(
            ((tile & 1) * 0x1000) +
            ((tile & 0xFE) + sourceY / 8) * 16 + (sourceY & 7));
    } else {
        spritePattern = static_cast<std::uint16_t>(
            ((control & 0x08) ? 0x1000 : 0) + tile * 16 + sourceY);
    }
    const int spriteBit = 7 - sourceX;
    const std::uint8_t spriteLow = ppuRead(spritePattern);
    const std::uint8_t spriteHigh = ppuRead(
        static_cast<std::uint16_t>(spritePattern + 8));
    const std::uint8_t spritePixel = static_cast<std::uint8_t>(
        ((spriteLow >> spriteBit) & 1) |
        (((spriteHigh >> spriteBit) & 1) << 1));
    if (spritePixel != 0) status |= 0x40;
}

void PPU2C02::incrementScrollX() {
    if ((vramAddress & 0x001F) == 31) {
        vramAddress &= static_cast<std::uint16_t>(~0x001F);
        vramAddress ^= 0x0400;
    } else {
        ++vramAddress;
    }
}

void PPU2C02::incrementScrollY() {
    if ((vramAddress & 0x7000) != 0x7000) {
        vramAddress += 0x1000;
        return;
    }

    vramAddress &= static_cast<std::uint16_t>(~0x7000);
    int coarseY = (vramAddress & 0x03E0) >> 5;
    if (coarseY == 29) {
        coarseY = 0;
        vramAddress ^= 0x0800;
    } else if (coarseY == 31) {
        coarseY = 0;
    } else {
        ++coarseY;
    }
    vramAddress = static_cast<std::uint16_t>(
        (vramAddress & ~0x03E0) | (coarseY << 5));
}

void PPU2C02::clock() {
    // NTSC PPU timing: 341 dots across 262 scanlines.
    if (scanline == 241 && ppuCycle == 1) {
        status |= 0x80; // VBlank begins
        ++vblankEvents;
        if (control & 0x80) nmiPending = true;
    }
    if (scanline == 261 && ppuCycle == 1) {
        status &= 0x1F; // Clear VBlank, sprite-zero hit, and overflow
    }

    const bool rendering = (mask & 0x18) != 0;
    const bool renderLine = (scanline >= 0 && scanline < 240) || scanline == 261;
    if (scanline >= 0 && scanline < 240 && ppuCycle == 1) {
        lineScrollAddress[static_cast<std::size_t>(scanline)] = vramAddress;
        lineFineX[static_cast<std::size_t>(scanline)] = fineX;
    }
    if (rendering && renderLine) {
        // This renderer samples one scroll position per scanline rather
        // than emulating the PPU's background fetch/shift-register pipeline.
        // Only advance X across the visible 256-pixel scanline. The hardware's
        // dot-328/dot-336 prefetch increments would otherwise be included in
        // the next saved scanline position, shifting the drawn map by 16 px.
        if (ppuCycle >= 1 && ppuCycle <= 256 &&
            (ppuCycle & 0x07) == 0) {
            incrementScrollX();
        }
        if (ppuCycle == 256) incrementScrollY();
        if (ppuCycle == 257) {
            vramAddress = static_cast<std::uint16_t>(
                (vramAddress & ~0x041F) | (temporaryAddress & 0x041F));
        }
        if (scanline == 261 && ppuCycle >= 280 && ppuCycle <= 304) {
            vramAddress = static_cast<std::uint16_t>(
                (vramAddress & ~0x7BE0) | (temporaryAddress & 0x7BE0));
        }
    }

    updateSpriteZeroHit();
    ++ppuCycle;
    if (ppuCycle >= 341) {
        ppuCycle = 0;
        ++scanline;
        if (scanline >= 262) scanline = 0;
    }
}

bool PPU2C02::consumeNmi() {
    const bool pending = nmiPending;
    nmiPending = false;
    return pending;
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
        if (cartridge != nullptr) return cartridge->readChr(address);
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
        if (cartridge != nullptr) cartridge->writeChr(address, value);
        return;
    }
    if (address < 0x3F00) {
        nametableRam[mapNametableAddress(address)] = value;
        return;
    }
    paletteRam[mapPaletteAddress(address)] = value & 0x3F;
}



std::size_t PPU2C02::debugNametableNonZero() const {
    return static_cast<std::size_t>(std::count_if(nametableRam.begin(), nametableRam.end(),
        [](std::uint8_t value) { return value != 0; }));
}

std::size_t PPU2C02::debugNametableTableNonZero(std::size_t table) const {
    if (table >= 4) return 0;
    const std::size_t begin = table * 0x400;
    return static_cast<std::size_t>(std::count_if(
        nametableRam.begin() + static_cast<std::ptrdiff_t>(begin),
        nametableRam.begin() + static_cast<std::ptrdiff_t>(begin + 0x400),
        [](std::uint8_t value) { return value != 0; }));
}

std::uint8_t PPU2C02::debugNametableByte(std::size_t table, std::size_t offset) const {
    if (table >= 4 || offset >= 0x400) return 0;
    return nametableRam[table * 0x400 + offset];
}

std::uint8_t PPU2C02::debugPaletteByte(std::size_t index) const {
    return index < paletteRam.size() ? paletteRam[index] : 0;
}

std::size_t PPU2C02::debugChrNonZero() const {
    if (cartridge == nullptr) return 0;
    std::size_t count = 0;
    for (std::uint16_t address = 0; address < 0x2000; ++address) {
        if (cartridge->readChr(address) != 0) ++count;
    }
    return count;
}

std::size_t PPU2C02::debugSelectedPatternNonZero() const {
    const std::uint16_t base = (control & 0x10) ? 0x1000 : 0x0000;
    std::size_t count = 0;
    for (std::uint16_t offset = 0; offset < 0x1000; ++offset) {
        if (ppuRead(static_cast<std::uint16_t>(base + offset)) != 0) ++count;
    }
    return count;
}

std::size_t PPU2C02::debugPaletteNonZero() const {
    return static_cast<std::size_t>(std::count_if(paletteRam.begin(), paletteRam.end(),
        [](std::uint8_t value) { return value != 0; }));
}

std::size_t PPU2C02::debugVisibleUniqueTiles() const {
    std::array<bool, 256> seen{};
    const std::uint16_t tableBase = static_cast<std::uint16_t>(
        0x2000 + (control & 0x03) * 0x400);
    std::size_t unique = 0;
    for (int y = 0; y < 30; ++y) {
        for (int x = 0; x < 32; ++x) {
            const auto tile = ppuRead(static_cast<std::uint16_t>(tableBase + y * 32 + x));
            if (!seen[tile]) {
                seen[tile] = true;
                ++unique;
            }
        }
    }
    return unique;
}

std::size_t PPU2C02::debugVisiblePatternPixels() const {
    const std::uint16_t tableBase = static_cast<std::uint16_t>(
        0x2000 + (control & 0x03) * 0x400);
    const std::uint16_t patternBase = (control & 0x10) ? 0x1000 : 0x0000;
    std::size_t pixels = 0;
    for (int y = 0; y < 30; ++y) {
        for (int x = 0; x < 32; ++x) {
            const auto tile = ppuRead(static_cast<std::uint16_t>(tableBase + y * 32 + x));
            pixels += debugTileNonZeroPixels(tile, patternBase);
        }
    }
    return pixels;
}

std::size_t PPU2C02::debugTileNonZeroPixels(std::uint8_t tile, std::uint16_t patternBase) const {
    std::size_t count = 0;
    const std::uint16_t base = static_cast<std::uint16_t>(patternBase + tile * 16);
    for (int row = 0; row < 8; ++row) {
        const std::uint8_t lo = ppuRead(static_cast<std::uint16_t>(base + row));
        const std::uint8_t hi = ppuRead(static_cast<std::uint16_t>(base + row + 8));
        for (int col = 0; col < 8; ++col) {
            const int bit = 7 - col;
            if (((lo >> bit) & 1) || ((hi >> bit) & 1)) ++count;
        }
    }
    return count;
}

std::array<std::size_t, 4> PPU2C02::debugVisiblePixelValueCounts() const {
    std::array<std::size_t, 4> counts{};
    const std::uint16_t tableBase = static_cast<std::uint16_t>(
        0x2000 + (control & 0x03) * 0x400);
    const std::uint16_t patternBase = (control & 0x10) ? 0x1000 : 0x0000;
    for (int y = 0; y < 30 * 8; ++y) {
        for (int x = 0; x < 32 * 8; ++x) {
            const int tileX = x / 8, tileY = y / 8;
            const auto tile = ppuRead(static_cast<std::uint16_t>(
                tableBase + tileY * 32 + tileX));
            const std::uint16_t address = static_cast<std::uint16_t>(
                patternBase + tile * 16 + (y & 7));
            const std::uint8_t lo = ppuRead(address);
            const std::uint8_t hi = ppuRead(static_cast<std::uint16_t>(address + 8));
            const int bit = 7 - (x & 7);
            const std::uint8_t pixel = static_cast<std::uint8_t>(
                ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1));
            ++counts[pixel];
        }
    }
    return counts;
}

std::array<std::size_t, 64> PPU2C02::debugVisiblePaletteIndexCounts() const {
    std::array<std::size_t, 64> counts{};
    const int baseTableX = control & 1, baseTableY = (control >> 1) & 1;
    const int patternBase = (control & 0x10) ? 0x1000 : 0;
    for (int y = 0; y < 240; ++y) for (int x = 0; x < 256; ++x) {
        const int worldX = (x + scrollX) % 512, worldY = (y + scrollY) % 480;
        const int tableX = (baseTableX + worldX / 256) & 1;
        const int tableY = (baseTableY + worldY / 240) & 1;
        const int localX = worldX % 256, localY = worldY % 240;
        const std::uint16_t tableBase = static_cast<std::uint16_t>(
            0x2000 + (tableY * 2 + tableX) * 0x400);
        const int tileX = localX / 8, tileY = localY / 8;
        const auto tile = ppuRead(static_cast<std::uint16_t>(
            tableBase + tileY * 32 + tileX));
        const std::uint16_t pat = static_cast<std::uint16_t>(
            patternBase + tile * 16 + (localY & 7));
        const auto lo = ppuRead(pat);
        const auto hi = ppuRead(static_cast<std::uint16_t>(pat + 8));
        const int bit = 7 - (localX & 7);
        const std::uint8_t pixel = static_cast<std::uint8_t>(
            ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1));
        if (pixel == 0) continue;
        const std::uint16_t attrAddr = static_cast<std::uint16_t>(
            tableBase + 0x3C0 + (tileY / 4) * 8 + tileX / 4);
        const auto attr = ppuRead(attrAddr);
        const int shift = ((tileY & 2) ? 4 : 0) + ((tileX & 2) ? 2 : 0);
        const auto palette = static_cast<std::uint8_t>((attr >> shift) & 3);
        const auto colorIndex = static_cast<std::uint8_t>(
            ppuRead(static_cast<std::uint16_t>(0x3F00 + palette * 4 + pixel)) & 0x3F);
        ++counts[colorIndex];
    }
    return counts;
}

std::array<std::size_t, 32> PPU2C02::debugVisiblePaletteSlotCounts() const {
    std::array<std::size_t, 32> counts{};
    const int baseTableX = control & 1, baseTableY = (control >> 1) & 1;
    const int patternBase = (control & 0x10) ? 0x1000 : 0;
    for (int y = 0; y < 240; ++y) for (int x = 0; x < 256; ++x) {
        const int worldX = (x + scrollX) % 512, worldY = (y + scrollY) % 480;
        const int tableX = (baseTableX + worldX / 256) & 1;
        const int tableY = (baseTableY + worldY / 240) & 1;
        const int localX = worldX % 256, localY = worldY % 240;
        const std::uint16_t tableBase = static_cast<std::uint16_t>(
            0x2000 + (tableY * 2 + tableX) * 0x400);
        const int tileX = localX / 8, tileY = localY / 8;
        const auto tile = ppuRead(static_cast<std::uint16_t>(tableBase + tileY * 32 + tileX));
        const std::uint16_t pat = static_cast<std::uint16_t>(
            patternBase + tile * 16 + (localY & 7));
        const auto lo = ppuRead(pat), hi = ppuRead(static_cast<std::uint16_t>(pat + 8));
        const int bit = 7 - (localX & 7);
        const std::uint8_t pixel = static_cast<std::uint8_t>(
            ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1));
        if (pixel == 0) continue;
        const std::uint16_t attrAddr = static_cast<std::uint16_t>(
            tableBase + 0x3C0 + (tileY / 4) * 8 + tileX / 4);
        const auto attr = ppuRead(attrAddr);
        const int shift = ((tileY & 2) ? 4 : 0) + ((tileX & 2) ? 2 : 0);
        const auto palette = static_cast<std::uint8_t>((attr >> shift) & 3);
        const auto slot = static_cast<std::uint8_t>(palette * 4 + pixel);
        ++counts[mapPaletteAddress(static_cast<std::uint16_t>(0x3F00 + slot))];
    }
    return counts;
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
    const std::uint32_t backdropColor = colors[ppuRead(0x3F00) & 0x3F];
    if ((mask & 0x08) == 0) {
        pixels.fill(0xFF000000);
        return;
    }
    std::array<std::uint8_t, 256 * 240> backgroundOpaque{};
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 256; ++x) {
            std::uint32_t output = backdropColor;
            if (x >= 8 || (mask & 0x02)) {
                std::uint8_t paletteSlot = 0;
                const std::uint8_t pixel = backgroundPixelAt(x, y, &paletteSlot);
                if (pixel != 0) {
                    output = colors[ppuRead(static_cast<std::uint16_t>(
                        0x3F00 + paletteSlot)) & 0x3F];
                    backgroundOpaque[static_cast<std::size_t>(y) * 256 + x] = 1;
                }
            }
            pixels[static_cast<std::size_t>(y) * 256 + x] = output;
        }
    }
    if ((mask & 0x10) == 0) return;
    const bool tall = (control & 0x20) != 0;
    const int height = tall ? 16 : 8;
    // The NES limit is eight sprites per scanline, not eight per frame.
    // Count each sprite on every visible scanline it overlaps.
    std::array<std::uint8_t, 240> spritesOnLine{};
    std::array<std::array<bool, 240>, 64> spriteRowAllowed{};
    // Select the first eight OAM sprites independently on each scanline.
    for (int s = 0; s < 64; ++s) {
        const int sy = static_cast<int>(oam[s * 4]) + 1;
        if (sy >= 240 || sy + height <= 0) continue;
        for (int row = 0; row < height; ++row) {
            const int py = sy + row;
            if (py >= 0 && py < 240 && spritesOnLine[py] < 8) {
                ++spritesOnLine[py];
                spriteRowAllowed[s][py] = true;
            }
        }
    }
    // Draw lower-priority sprites first so lower OAM indices win overlaps.
    for (int s = 63; s >= 0; --s) {
        const std::uint8_t rawY = oam[s * 4], tile = oam[s * 4 + 1], attr = oam[s * 4 + 2];
        const int sx = oam[s * 4 + 3], sy = static_cast<int>(rawY) + 1;
        if (sy >= 240 || sy + height <= 0) continue;
        for (int row = 0; row < height; ++row) {
            const int py = sy + row;
            if (py < 0 || py >= 240 || !spriteRowAllowed[s][py]) continue;
            const int sourceY = (attr & 0x80) ? height - 1 - row : row;
            std::uint16_t pat;
            if (tall) pat = static_cast<std::uint16_t>((tile & 1) * 0x1000 + ((tile & 0xFE) + sourceY / 8) * 16 + (sourceY & 7));
            else pat = static_cast<std::uint16_t>(((control & 8) ? 0x1000 : 0) + tile * 16 + sourceY);
            const std::uint8_t lo = ppuRead(pat), hi = ppuRead(static_cast<std::uint16_t>(pat + 8));
            for (int col = 0; col < 8; ++col) {
                const int px = sx + col;
                if (px < 0 || px >= 256 || (px < 8 && !(mask & 4))) continue;
                const int sourceX = (attr & 0x40) ? 7 - col : col, bit = 7 - sourceX;
                const std::uint8_t pixel = static_cast<std::uint8_t>(((lo >> bit) & 1) | (((hi >> bit) & 1) << 1));
                if (!pixel) continue;
                const std::size_t index = static_cast<std::size_t>(py) * 256 + px;
                if ((attr & 0x20) && backgroundOpaque[index]) continue;
                pixels[index] = colors[ppuRead(static_cast<std::uint16_t>(0x3F10 + (attr & 3) * 4 + pixel)) & 0x3F];
            }
        }
    }
}
std::uint8_t PPU2C02::cpuRead(std::uint16_t registerAddress) {
    const std::uint8_t reg = static_cast<std::uint8_t>(registerAddress & 0x07);
    std::uint8_t value = openBus;

    if (reg == 2) {
        if (status & 0x80) ++vblankStatusReads;
        value = static_cast<std::uint8_t>((status & 0xE0) | (openBus & 0x1F));
        lastStatusRead = value;
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
            if ((value & 0x80) && !(control & 0x80) && (status & 0x80))
                nmiPending = true;
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
                scrollX = value;
                fineX = value & 0x07;
                temporaryAddress = static_cast<std::uint16_t>(
                    (temporaryAddress & 0xFFE0) | (value >> 3));
            } else {
                scrollY = value;
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
