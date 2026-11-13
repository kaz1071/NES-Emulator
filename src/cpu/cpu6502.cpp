#include "cpu6502.hpp"

void CPU6502::connectCartridge(const Cartridge* loadedCartridge) {
    cartridge = loadedCartridge;
}

std::uint8_t CPU6502::read(std::uint16_t address) const {
    if (address < 0x2000) {
        return ram[address & 0x07FF];
    }

    // NROM maps 16 KiB or 32 KiB of PRG-ROM at $8000-$FFFF.
    if (address >= 0x8000 && cartridge != nullptr &&
        !cartridge->prgRom.empty()) {
        const std::size_t offset =
            static_cast<std::size_t>(address - 0x8000) %
            cartridge->prgRom.size();
        return cartridge->prgRom[offset];
    }

    return 0;
}

void CPU6502::write(std::uint16_t address, std::uint8_t value) {
    if (address < 0x2000) {
        ram[address & 0x07FF] = value;
    }
    // Cartridge PRG-ROM and currently unimplemented I/O are read-only here.
}

void CPU6502::setZeroNegative(std::uint8_t value) {
    status &= static_cast<std::uint8_t>(~(FLAG_ZERO | FLAG_NEGATIVE));
    if (value == 0) {
        status |= FLAG_ZERO;
    }
    if ((value & 0x80) != 0) {
        status |= FLAG_NEGATIVE;
    }
}

void CPU6502::reset() {
    a = 0;
    x = 0;
    y = 0;
    sp = 0xFD;
    status = 0x24; // Unused and interrupt-disable flags set.
    isHalted = false;
    lastError.clear();
    ram.fill(0);

    // The reset vector is stored at CPU addresses $FFFC-$FFFD.
    const std::uint8_t low = read(0xFFFC);
    const std::uint8_t high = read(0xFFFD);
    pc = static_cast<std::uint16_t>(
        low | (static_cast<std::uint16_t>(high) << 8));
}

bool CPU6502::step() {
    if (isHalted) {
        return false;
    }

    const std::uint8_t opcode = read(pc++);

    switch (opcode) {
        case 0xA9: // LDA immediate
            a = read(pc++);
            setZeroNegative(a);
            return true;

        case 0xA2: // LDX immediate
            x = read(pc++);
            setZeroNegative(x);
            return true;

        case 0xA0: // LDY immediate
            y = read(pc++);
            setZeroNegative(y);
            return true;

        case 0x8D: { // STA absolute
            const std::uint8_t low = read(pc++);
            const std::uint8_t high = read(pc++);
            const auto address = static_cast<std::uint16_t>(
                low | (static_cast<std::uint16_t>(high) << 8));
            write(address, a);
            return true;
        }

        case 0xE8: // INX
            ++x;
            setZeroNegative(x);
            return true;

        case 0x4C: { // JMP absolute
            const std::uint8_t low = read(pc++);
            const std::uint8_t high = read(pc++);
            pc = static_cast<std::uint16_t>(
                low | (static_cast<std::uint16_t>(high) << 8));
            return true;
        }

        case 0xEA: // NOP
            return true;

        case 0x00: // BRK (halted for this early CPU foundation)
            isHalted = true;
            lastError = "BRK instruction reached; CPU stopped.";
            return false;

        default:
            isHalted = true;
            lastError = "Unsupported 6502 opcode $" ;
            {
                constexpr char digits[] = "0123456789ABCDEF";
                lastError += digits[(opcode >> 4) & 0x0F];
                lastError += digits[opcode & 0x0F];
            }
            return false;
    }
}

bool CPU6502::halted() const {
    return isHalted;
}

const std::string& CPU6502::error() const {
    return lastError;
}
