#include "ppu2c02.hpp"

#include <algorithm>
#include <cstddef>

namespace {
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
}

void PPU2C02::connectCartridge(Cartridge* loadedCartridge) {
    cartridge = loadedCartridge;
}

PPU2C02::State PPU2C02::saveState() const {
    State state{};
    state.nametableRam = nametableRam;
    state.paletteRam = paletteRam;
    state.oam = oam;
    state.control = control;
    state.mask = mask;
    state.status = status;
    state.oamAddress = oamAddress;
    state.openBus = openBus;
    state.readBuffer = readBuffer;
    state.vramAddress = vramAddress;
    state.temporaryAddress = temporaryAddress;
    state.fineX = fineX;
    state.scrollX = scrollX;
    state.scrollY = scrollY;
    state.writeSecond = writeSecond;
    state.ppuCycle = ppuCycle;
    state.scanline = scanline;
    state.nmiPending = nmiPending;
    state.vblankEvents = vblankEvents;
    state.vblankStatusReads = vblankStatusReads;
    state.lastStatusRead = lastStatusRead;
    state.lineScrollAddress = lineScrollAddress;
    state.lineFineX = lineFineX;
    state.lineMask = lineMask;
    state.framePixels = framePixels;
    state.lineOam = lineOam;
    state.lineSpriteControl = lineSpriteControl;
    return state;
}

void PPU2C02::loadState(const State& state) {
    nametableRam = state.nametableRam;
    paletteRam = state.paletteRam;
    oam = state.oam;
    control = state.control;
    mask = state.mask;
    status = state.status;
    oamAddress = state.oamAddress;
    openBus = state.openBus;
    readBuffer = state.readBuffer;
    vramAddress = state.vramAddress;
    temporaryAddress = state.temporaryAddress;
    fineX = state.fineX;
    scrollX = state.scrollX;
    scrollY = state.scrollY;
    writeSecond = state.writeSecond;
    ppuCycle = state.ppuCycle;
    scanline = state.scanline;
    nmiPending = state.nmiPending;
    vblankEvents = state.vblankEvents;
    vblankStatusReads = state.vblankStatusReads;
    lastStatusRead = state.lastStatusRead;
    lineScrollAddress = state.lineScrollAddress;
    lineFineX = state.lineFineX;
    lineMask = state.lineMask;
    framePixels = state.framePixels;
    lineOam = state.lineOam;
    lineSpriteControl = state.lineSpriteControl;
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
    lineMask.fill(0);
    framePixels.fill(0xFF000000);
    for (auto& line : lineOam) line.fill(0);
    lineSpriteControl.fill(0);
}

PPUBackgroundDebugInfo PPU2C02::debugBackgroundInfo(int x, int y) const {
    PPUBackgroundDebugInfo info{};
    if (x < 0 || x >= 256 || y < 0 || y >= 240) return info;

    const std::size_t line = static_cast<std::size_t>(y);
    info.scrollAddress = lineScrollAddress[line];
    info.fineX = lineFineX[line];
    const int pixelX = static_cast<int>(info.fineX) + x;
    const int tileOffsetX = (info.scrollAddress & 0x001F) + pixelX / 8;
    info.tableX = static_cast<std::uint8_t>(
        (((info.scrollAddress >> 10) & 1) + tileOffsetX / 32) & 1);
    info.tableY = static_cast<std::uint8_t>((info.scrollAddress >> 11) & 1);
    info.tileX = static_cast<std::uint8_t>(tileOffsetX & 31);
    info.tileY = static_cast<std::uint8_t>((info.scrollAddress >> 5) & 31);
    info.finePixelX = static_cast<std::uint8_t>(pixelX & 7);
    info.finePixelY = static_cast<std::uint8_t>((info.scrollAddress >> 12) & 7);

    const bool splitEnabled = cartridge != nullptr && cartridge->mapper == 5 &&
        (cartridge->mmc5VerticalSplit & 0x80) != 0 &&
        cartridge->mmc5ExRamMode <= 1 &&
        (lineMask[line] & 0x18) != 0;
    const int splitTileX = (pixelX / 8) & 31;
    const int splitThreshold = cartridge != nullptr
        ? static_cast<int>(cartridge->mmc5VerticalSplit & 0x1F) : 32;
    const bool splitOnRight = cartridge != nullptr &&
        (cartridge->mmc5VerticalSplit & 0x40) != 0;
    const bool inSplitRegion = splitEnabled &&
        (splitOnRight ? splitTileX >= splitThreshold : splitTileX < splitThreshold);
    if (inSplitRegion) {
        const int splitY = (static_cast<int>(cartridge->mmc5VerticalSplitScroll) + y) & 0xFF;
        const int splitTileY = splitY / 8;
        const int splitFineY = splitY & 7;
        info.tileX = static_cast<std::uint8_t>(splitTileX);
        info.tileY = static_cast<std::uint8_t>(splitTileY);
        info.finePixelY = static_cast<std::uint8_t>(splitFineY);
        info.tile = cartridge->mmc5ExRam[
            (static_cast<std::size_t>(splitTileY) * 32 + splitTileX) & 0x3FF];
        const std::uint16_t splitPattern = static_cast<std::uint16_t>(
            info.tile * 16 + splitFineY);
        const int bit = 7 - info.finePixelX;
        const std::uint8_t low = cartridge->readMmc5SplitChr(splitPattern);
        const std::uint8_t high = cartridge->readMmc5SplitChr(
            static_cast<std::uint16_t>(splitPattern + 8));
        info.pixel = static_cast<std::uint8_t>(
            ((low >> bit) & 1) | (((high >> bit) & 1) << 1));
        const std::size_t attrAddress = 0x3C0 +
            static_cast<std::size_t>(splitTileY / 4) * 8 +
            static_cast<std::size_t>(splitTileX / 4);
        const std::uint8_t attribute = cartridge->mmc5ExRam[attrAddress & 0x3FF];
        const int shift = ((splitTileY & 2) ? 4 : 0) + ((splitTileX & 2) ? 2 : 0);
        info.palette = static_cast<std::uint8_t>((attribute >> shift) & 3);
        return info;
    }
    const std::uint16_t tableBase = static_cast<std::uint16_t>(
        0x2000 + (info.tableY * 2 + info.tableX) * 0x400);
    info.tile = ppuRead(static_cast<std::uint16_t>(
        tableBase + info.tileY * 32 + info.tileX));
    const std::uint16_t pattern = static_cast<std::uint16_t>(
        ((lineSpriteControl[line] & 0x10) ? 0x1000 : 0) +
        info.tile * 16 + info.finePixelY);
    const bool extended = cartridge != nullptr && cartridge->mapper == 5 &&
                          cartridge->mmc5ExRamMode == 1;
    const std::uint8_t extendedAttribute = extended
        ? cartridge->mmc5ExRam[static_cast<std::size_t>(info.tileY) * 32 + info.tileX]
        : 0;
    const int bit = 7 - info.finePixelX;
    const std::uint8_t low = extended
        ? cartridge->readMmc5ExtendedChr(pattern, extendedAttribute & 0x3F)
        : (cartridge != nullptr && cartridge->mapper == 5
            ? cartridge->readMmc5Chr(pattern, true) : ppuRead(pattern));
    const std::uint8_t high = extended
        ? cartridge->readMmc5ExtendedChr(static_cast<std::uint16_t>(pattern + 8), extendedAttribute & 0x3F)
        : (cartridge != nullptr && cartridge->mapper == 5
            ? cartridge->readMmc5Chr(static_cast<std::uint16_t>(pattern + 8), true)
            : ppuRead(static_cast<std::uint16_t>(pattern + 8)));
    info.pixel = static_cast<std::uint8_t>(
        ((low >> bit) & 1) | (((high >> bit) & 1) << 1));

    if (extended) {
        info.palette = static_cast<std::uint8_t>(extendedAttribute >> 6);
    } else {
        const std::uint16_t attributeAddress = static_cast<std::uint16_t>(
            tableBase + 0x3C0 + (info.tileY / 4) * 8 + info.tileX / 4);
        const std::uint8_t attribute = ppuRead(attributeAddress);
        const int shift = ((info.tileY & 2) ? 4 : 0) + ((info.tileX & 2) ? 2 : 0);
        info.palette = static_cast<std::uint8_t>((attribute >> shift) & 3);
    }
    return info;
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
    const bool mmc5SplitActive = cartridge != nullptr && cartridge->mapper == 5 &&
        (cartridge->mmc5VerticalSplit & 0x80) != 0 &&
        cartridge->mmc5ExRamMode <= 1 &&
        (lineMask[static_cast<std::size_t>(y)] & 0x18) != 0;
    const int splitTileX = (pixelX / 8) & 31;
    const int splitThreshold = cartridge != nullptr
        ? static_cast<int>(cartridge->mmc5VerticalSplit & 0x1F) : 32;
    const bool splitOnRight = cartridge != nullptr &&
        (cartridge->mmc5VerticalSplit & 0x40) != 0;
    const bool inSplitRegion = mmc5SplitActive &&
        (splitOnRight ? splitTileX >= splitThreshold : splitTileX < splitThreshold);
    if (inSplitRegion) {
        // The split region has its own vertical origin and a dedicated 4 KiB CHR bank.
        // ExRAM supplies the tile map and its conventional attribute-table bytes.
        const int splitY = (static_cast<int>(cartridge->mmc5VerticalSplitScroll) + y) & 0xFF;
        const int splitTileY = splitY / 8;
        const int splitFineY = splitY & 7;
        const std::size_t exramTileAddress =
            static_cast<std::size_t>(splitTileY) * 32 + splitTileX;
        const std::uint8_t splitTile = cartridge->mmc5ExRam[exramTileAddress & 0x3FF];
        const std::uint16_t splitPattern = static_cast<std::uint16_t>(
            splitTile * 16 + splitFineY);
        const int bit = 7 - (pixelX & 7);
        const std::uint8_t splitLow = cartridge->readMmc5SplitChr(splitPattern);
        const std::uint8_t splitHigh = cartridge->readMmc5SplitChr(
            static_cast<std::uint16_t>(splitPattern + 8));
        const std::uint8_t splitPixel = static_cast<std::uint8_t>(
            ((splitLow >> bit) & 1) | (((splitHigh >> bit) & 1) << 1));
        if (splitPixel == 0) return 0;
        const std::size_t attributeAddress = 0x3C0 +
            static_cast<std::size_t>(splitTileY / 4) * 8 +
            static_cast<std::size_t>(splitTileX / 4);
        const std::uint8_t attribute = cartridge->mmc5ExRam[attributeAddress & 0x3FF];
        const int attributeShift = ((splitTileY & 2) ? 4 : 0) +
                                   ((splitTileX & 2) ? 2 : 0);
        const std::uint8_t splitPalette = static_cast<std::uint8_t>(
            (attribute >> attributeShift) & 3);
        if (paletteSlot != nullptr)
            *paletteSlot = static_cast<std::uint8_t>(splitPalette * 4 + splitPixel);
        return splitPixel;
    }

    const int tableX = (baseTableX + tileOffsetX / 32) & 1;
    const int tileX = tileOffsetX & 31;
    const int tileY = coarseY;
    const int finePixelX = pixelX & 7;
    const std::uint16_t tableBase = static_cast<std::uint16_t>(
        0x2000 + (tableY * 2 + tableX) * 0x400);
    const std::uint8_t tile = ppuRead(static_cast<std::uint16_t>(
        tableBase + tileY * 32 + tileX));
    const std::uint16_t pattern = static_cast<std::uint16_t>(
        ((lineSpriteControl[static_cast<std::size_t>(y)] & 0x10) ? 0x1000 : 0) + tile * 16 + fineY);
    const bool extended = cartridge != nullptr && cartridge->mapper == 5 &&
                          cartridge->mmc5ExRamMode == 1;
    const std::uint8_t extendedAttribute = extended
        ? cartridge->mmc5ExRam[static_cast<std::size_t>(tileY) * 32 + tileX]
        : 0;
    const int bit = 7 - finePixelX;
    const std::uint8_t low = extended
        ? cartridge->readMmc5ExtendedChr(pattern, extendedAttribute & 0x3F)
        : ppuRead(pattern);
    const std::uint8_t high = extended
        ? cartridge->readMmc5ExtendedChr(static_cast<std::uint16_t>(pattern + 8), extendedAttribute & 0x3F)
        : ppuRead(static_cast<std::uint16_t>(pattern + 8));
    const std::uint8_t pixel = static_cast<std::uint8_t>(
        ((low >> bit) & 1) | (((high >> bit) & 1) << 1));
    if (pixel == 0) return 0;

    std::uint8_t palette = 0;
    if (extended) {
        palette = static_cast<std::uint8_t>(extendedAttribute >> 6);
    } else {
        const std::uint16_t attributeAddress = static_cast<std::uint16_t>(
            tableBase + 0x3C0 + (tileY / 4) * 8 + tileX / 4);
        const std::uint8_t attribute = ppuRead(attributeAddress);
        const int shift = ((tileY & 2) ? 4 : 0) + ((tileX & 2) ? 2 : 0);
        palette = static_cast<std::uint8_t>((attribute >> shift) & 3);
    }
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

    const auto& scanlineOam = lineOam[static_cast<std::size_t>(y)];
    const std::uint8_t scanlineControl = lineSpriteControl[static_cast<std::size_t>(y)];
    const int spriteX = scanlineOam[3];
    const int spriteY = static_cast<int>(scanlineOam[0]) + 1;
    const bool tall = (scanlineControl & 0x20) != 0;
    const int spriteHeight = tall ? 16 : 8;
    if (x < spriteX || x >= spriteX + 8 ||
        y < spriteY || y >= spriteY + spriteHeight) return;

    // Use this scanline's captured Loopy scroll state for the background.
    if (backgroundPixelAt(x, y) == 0) return;

    // Decode sprite 0, including horizontal/vertical flip and 8x16 mode.
    const std::uint8_t tile = scanlineOam[1];
    const std::uint8_t attributes = scanlineOam[2];
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
            ((scanlineControl & 0x08) ? 0x1000 : 0) + tile * 16 + sourceY);
    }
    const int spriteBit = 7 - sourceX;
    const std::uint8_t spriteLow = cartridge != nullptr && cartridge->mapper == 5
        ? cartridge->readMmc5Chr(spritePattern, false) : ppuRead(spritePattern);
    const std::uint16_t spriteHighAddress = static_cast<std::uint16_t>(spritePattern + 8);
    const std::uint8_t spriteHigh = cartridge != nullptr && cartridge->mapper == 5
        ? cartridge->readMmc5Chr(spriteHighAddress, false) : ppuRead(spriteHighAddress);
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
    if (scanline == 240 && ppuCycle == 1 && cartridge != nullptr && cartridge->mapper == 5)
        cartridge->endMmc5Frame();
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
        const auto line = static_cast<std::size_t>(scanline);
        lineScrollAddress[line] = vramAddress;
        lineFineX[line] = fineX;
        lineMask[line] = mask;
        // Keep the sprite state that was active when this scanline began.
        // A single end-of-frame OAM snapshot loses games' mid-frame sprite updates.
        lineOam[line] = oam;
        lineSpriteControl[line] = control;
        renderScanline(scanline);
        // Mapper IRQ counters use this renderer's visible-scanline boundary.
        // MMC5 hardware detects fetch sequences, so this remains approximate.
        if (cartridge != nullptr) cartridge->clockScanline(rendering);
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
    } else if (cartridge->mirroring == Mirroring::OneScreenLower) {
        physicalTable = 0;
    } else if (cartridge->mirroring == Mirroring::OneScreenUpper) {
        physicalTable = 1;
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
        if (cartridge != nullptr && cartridge->mapper == 5) {
            const std::uint16_t relative = static_cast<std::uint16_t>((address - 0x2000) & 0x0FFF);
            const std::size_t table = relative / 0x400;
            const std::size_t offset = relative & 0x3FF;
            const std::uint8_t source = (cartridge->mmc5NametableMap >> (table * 2)) & 3;
            if (source <= 1) return nametableRam[source * 0x400 + offset];
            if (source == 2) return cartridge->mmc5ExRamMode == 0 ? cartridge->mmc5ExRam[offset] : 0;
            if (offset < 0x3C0) return cartridge->mmc5FillTile;
            return static_cast<std::uint8_t>(cartridge->mmc5FillAttribute * 0x55);
        }
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
        if (cartridge != nullptr && cartridge->mapper == 5) {
            const std::uint16_t relative = static_cast<std::uint16_t>((address - 0x2000) & 0x0FFF);
            const std::size_t table = relative / 0x400;
            const std::size_t offset = relative & 0x3FF;
            const std::uint8_t source = (cartridge->mmc5NametableMap >> (table * 2)) & 3;
            if (source <= 1) nametableRam[source * 0x400 + offset] = value;
            else if (source == 2 && cartridge->mmc5ExRamMode == 0) cartridge->mmc5ExRam[offset] = value;
            return;
        }
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

void PPU2C02::renderScanline(int y) {
    if (y < 0 || y >= 240) return;
    const std::size_t row = static_cast<std::size_t>(y) * 256;
    const std::uint8_t lineRenderingMask = lineMask[static_cast<std::size_t>(y)];
    const std::uint32_t backdrop = colors[ppuRead(0x3F00) & 0x3F];
    if ((lineRenderingMask & 0x08) == 0) {
        std::fill_n(framePixels.begin() + static_cast<std::ptrdiff_t>(row),
                    256, 0xFF000000);
        return;
    }

    std::array<std::uint8_t, 256> backgroundOpaque{};
    for (int x = 0; x < 256; ++x) {
        std::uint32_t output = backdrop;
        if (x >= 8 || (lineRenderingMask & 0x02)) {
            std::uint8_t paletteSlot = 0;
            const std::uint8_t pixel = backgroundPixelAt(x, y, &paletteSlot);
            if (pixel != 0) {
                output = colors[ppuRead(static_cast<std::uint16_t>(
                    0x3F00 + paletteSlot)) & 0x3F];
                backgroundOpaque[static_cast<std::size_t>(x)] = 1;
            }
        }
        framePixels[row + static_cast<std::size_t>(x)] = output;
    }
    if ((lineRenderingMask & 0x10) == 0) return;

    const auto& scanlineOam = lineOam[static_cast<std::size_t>(y)];
    const std::uint8_t scanlineControl = lineSpriteControl[static_cast<std::size_t>(y)];
    const bool tall = (scanlineControl & 0x20) != 0;
    const int height = tall ? 16 : 8;
    std::array<int, 8> selected{};
    int selectedCount = 0;
    for (int s = 0; s < 64 && selectedCount < 8; ++s) {
        const int sy = static_cast<int>(scanlineOam[s * 4]) + 1;
        if (y >= sy && y < sy + height) selected[selectedCount++] = s;
    }

    for (int i = selectedCount - 1; i >= 0; --i) {
        const int s = selected[i];
        const std::uint8_t tile = scanlineOam[s * 4 + 1];
        const std::uint8_t attr = scanlineOam[s * 4 + 2];
        const int sx = scanlineOam[s * 4 + 3];
        const int sy = static_cast<int>(scanlineOam[s * 4]) + 1;
        const int rowInSprite = y - sy;
        const int sourceY = (attr & 0x80) ? height - 1 - rowInSprite : rowInSprite;
        std::uint16_t pattern;
        if (tall) {
            pattern = static_cast<std::uint16_t>(
                (tile & 1) * 0x1000 +
                ((tile & 0xFE) + sourceY / 8) * 16 +
                (sourceY & 7));
        } else {
            pattern = static_cast<std::uint16_t>(
                ((scanlineControl & 0x08) ? 0x1000 : 0) +
                tile * 16 + sourceY);
        }
        const std::uint8_t low = cartridge != nullptr && cartridge->mapper == 5
            ? cartridge->readMmc5Chr(pattern, false) : ppuRead(pattern);
        const std::uint16_t highAddress = static_cast<std::uint16_t>(pattern + 8);
        const std::uint8_t high = cartridge != nullptr && cartridge->mapper == 5
            ? cartridge->readMmc5Chr(highAddress, false) : ppuRead(highAddress);
        for (int col = 0; col < 8; ++col) {
            const int px = sx + col;
            if (px < 0 || px >= 256 ||
                (px < 8 && !(lineRenderingMask & 0x04))) continue;
            const int sourceX = (attr & 0x40) ? 7 - col : col;
            const int bit = 7 - sourceX;
            const std::uint8_t pixel = static_cast<std::uint8_t>(
                ((low >> bit) & 1) | (((high >> bit) & 1) << 1));
            if (pixel == 0) continue;
            if ((attr & 0x20) && backgroundOpaque[static_cast<std::size_t>(px)]) continue;
            framePixels[row + static_cast<std::size_t>(px)] =
                colors[ppuRead(static_cast<std::uint16_t>(
                    0x3F10 + (attr & 3) * 4 + pixel)) & 0x3F];
        }
    }
}

void PPU2C02::renderFrame(std::array<std::uint32_t, 256 * 240>& pixels) const {
    pixels = framePixels;
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
            if ((mask & 0x18) == 0 && cartridge != nullptr && cartridge->mapper == 5)
                cartridge->endMmc5Frame();
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
