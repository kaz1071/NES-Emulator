#include "cpu6502.hpp"
#include <cstdint>

void CPU6502::connectBus(NESBus* connectedBus) {
    bus = connectedBus;
}

std::uint8_t CPU6502::read(std::uint16_t address) {
    return bus != nullptr ? bus->read(address) : 0;
}

void CPU6502::write(std::uint16_t address, std::uint8_t value) {
    if (bus != nullptr) bus->write(address, value);
}

void CPU6502::setFlag(std::uint8_t flag, bool value) {
    if (value) status |= flag;
    else status &= static_cast<std::uint8_t>(~flag);
    status |= FLAG_UNUSED;
}

bool CPU6502::getFlag(std::uint8_t flag) const { return (status & flag) != 0; }

void CPU6502::setZeroNegative(std::uint8_t value) {
    setFlag(FLAG_ZERO, value == 0);
    setFlag(FLAG_NEGATIVE, (value & 0x80) != 0);
}

void CPU6502::push(std::uint8_t value) {
    write(static_cast<std::uint16_t>(0x0100 | sp), value);
    --sp;
}

std::uint8_t CPU6502::pop() {
    ++sp;
    return read(static_cast<std::uint16_t>(0x0100 | sp));
}

void CPU6502::adc(std::uint8_t value) {
    const std::uint16_t sum = static_cast<std::uint16_t>(a) + value + (getFlag(FLAG_CARRY) ? 1 : 0);
    const std::uint8_t result = static_cast<std::uint8_t>(sum);
    setFlag(FLAG_CARRY, sum > 0xFF);
    setFlag(FLAG_OVERFLOW, ((~(a ^ value) & (a ^ result)) & 0x80) != 0);
    a = result;
    setZeroNegative(a);
}

void CPU6502::compare(std::uint8_t reg, std::uint8_t value) {
    setFlag(FLAG_CARRY, reg >= value);
    setZeroNegative(static_cast<std::uint8_t>(reg - value));
}

void CPU6502::reset() {
    a = x = y = 0;
    sp = 0xFD;
    status = FLAG_UNUSED | FLAG_INTERRUPT;
    isHalted = false;
    lastError.clear();
    const std::uint8_t low = read(0xFFFC);
    const std::uint8_t high = read(0xFFFD);
    pc = static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8));
}

bool CPU6502::step() {
    if (isHalted) return false;

    if (bus != nullptr && bus->consumeNmi()) {
        push(static_cast<std::uint8_t>((pc >> 8) & 0xFF));
        push(static_cast<std::uint8_t>(pc & 0xFF));
        push(static_cast<std::uint8_t>((status & ~FLAG_BREAK) | FLAG_UNUSED));
        setFlag(FLAG_INTERRUPT, true);
        const std::uint8_t low = read(0xFFFA);
        const std::uint8_t high = read(0xFFFB);
        pc = static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8));
        return true;
    }

    const auto fetch = [this]() { return read(pc++); };
    const auto fetchWord = [&fetch]() {
        const std::uint8_t low = fetch();
        const std::uint8_t high = fetch();
        return static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8));
    };
    const auto zp = [&fetch]() { return static_cast<std::uint16_t>(fetch()); };
    const auto zpx = [this, &fetch]() { return static_cast<std::uint16_t>(static_cast<std::uint8_t>(fetch() + x)); };
    const auto zpy = [this, &fetch]() { return static_cast<std::uint16_t>(static_cast<std::uint8_t>(fetch() + y)); };
    const auto absx = [this, &fetchWord]() { return static_cast<std::uint16_t>(fetchWord() + x); };
    const auto absy = [this, &fetchWord]() { return static_cast<std::uint16_t>(fetchWord() + y); };
    const auto indy = [this, &fetch]() {
        const std::uint8_t pointer = fetch();
        const std::uint8_t low = read(pointer);
        const std::uint8_t high = read(static_cast<std::uint8_t>(pointer + 1));
        const std::uint16_t base = static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8));
        return static_cast<std::uint16_t>(base + y);
    };
    const auto branch = [this, &fetch](bool condition) {
        const auto offset = static_cast<std::int8_t>(fetch());
        if (condition) pc = static_cast<std::uint16_t>(pc + offset);
    };

    const std::uint8_t opcode = fetch();
    switch (opcode) {
        // Loads (all LDA/LDX/LDY forms update Zero and Negative flags)
        case 0xA9: a = fetch(); setZeroNegative(a); break;
        case 0xA5: a = read(zp()); setZeroNegative(a); break;
        case 0xB5: a = read(zpx()); setZeroNegative(a); break;
        case 0xAD: a = read(fetchWord()); setZeroNegative(a); break;
        case 0xBD: a = read(absx()); setZeroNegative(a); break;
        case 0xB9: a = read(absy()); setZeroNegative(a); break;
        case 0xB1: a = read(indy()); setZeroNegative(a); break;
        case 0xA2: x = fetch(); setZeroNegative(x); break;
        case 0xA6: x = read(zp()); setZeroNegative(x); break;
        case 0xB6: x = read(zpy()); setZeroNegative(x); break;
        case 0xAE: x = read(fetchWord()); setZeroNegative(x); break;
        case 0xBE: x = read(absy()); setZeroNegative(x); break;
        case 0xA0: y = fetch(); setZeroNegative(y); break;
        case 0xA4: y = read(zp()); setZeroNegative(y); break;
        case 0xB4: y = read(zpx()); setZeroNegative(y); break;
        case 0xAC: y = read(fetchWord()); setZeroNegative(y); break;
        case 0xBC: y = read(absx()); setZeroNegative(y); break;

        // Stores
        case 0x85: write(zp(), a); break; case 0x95: write(zpx(), a); break;
        case 0x8D: write(fetchWord(), a); break; case 0x9D: write(absx(), a); break;
        case 0x99: write(absy(), a); break;
        case 0x86: write(zp(), x); break; case 0x96: write(zpy(), x); break;
        case 0x8E: write(fetchWord(), x); break;
        case 0x84: write(zp(), y); break; case 0x94: write(zpx(), y); break;
        case 0x8C: write(fetchWord(), y); break;

        // Transfers and register increments/decrements
        case 0xAA: x = a; setZeroNegative(x); break;
        case 0xA8: y = a; setZeroNegative(y); break;
        case 0x8A: a = x; setZeroNegative(a); break;
        case 0x98: a = y; setZeroNegative(a); break;
        case 0xBA: x = sp; setZeroNegative(x); break;
        case 0x9A: sp = x; break;
        case 0xE8: ++x; setZeroNegative(x); break;
        case 0xCA: --x; setZeroNegative(x); break;
        case 0xC8: ++y; setZeroNegative(y); break;
        case 0x88: --y; setZeroNegative(y); break;

        // Immediate logic, arithmetic, and comparisons
        case 0x29: a &= fetch(); setZeroNegative(a); break;
        case 0x09: a |= fetch(); setZeroNegative(a); break;
        case 0x49: a ^= fetch(); setZeroNegative(a); break;
        case 0x69: adc(fetch()); break;
        case 0xE9: adc(static_cast<std::uint8_t>(~fetch())); break;
        case 0xC9: compare(a, fetch()); break;
        case 0xE0: compare(x, fetch()); break;
        case 0xC0: compare(y, fetch()); break;

        // Branches
        case 0x90: branch(!getFlag(FLAG_CARRY)); break;
        case 0xB0: branch(getFlag(FLAG_CARRY)); break;
        case 0xF0: branch(getFlag(FLAG_ZERO)); break;
        case 0xD0: branch(!getFlag(FLAG_ZERO)); break;
        case 0x30: branch(getFlag(FLAG_NEGATIVE)); break;
        case 0x10: branch(!getFlag(FLAG_NEGATIVE)); break;
        case 0x70: branch(getFlag(FLAG_OVERFLOW)); break;
        case 0x50: branch(!getFlag(FLAG_OVERFLOW)); break;

        // Processor flags
        case 0x18: setFlag(FLAG_CARRY, false); break;
        case 0x38: setFlag(FLAG_CARRY, true); break;
        case 0x58: setFlag(FLAG_INTERRUPT, false); break;
        case 0x78: setFlag(FLAG_INTERRUPT, true); break;
        case 0xB8: setFlag(FLAG_OVERFLOW, false); break;
        case 0xD8: setFlag(FLAG_DECIMAL, false); break;
        case 0xF8: setFlag(FLAG_DECIMAL, true); break;

        // Stack operations
        case 0x48: push(a); break;
        case 0x68: a = pop(); setZeroNegative(a); break;
        case 0x08: push(static_cast<std::uint8_t>(status | FLAG_BREAK | FLAG_UNUSED)); break;
        case 0x28: status = static_cast<std::uint8_t>((pop() | FLAG_UNUSED) & ~FLAG_BREAK); break;
        case 0x40: {
            status = static_cast<std::uint8_t>((pop() | FLAG_UNUSED) & ~FLAG_BREAK);
            const std::uint8_t low = pop();
            const std::uint8_t high = pop();
            pc = static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8));
            break;
        }

        // Subroutine and jump instructions
        case 0x20: {
            const auto target = fetchWord();
            const auto returnAddress = static_cast<std::uint16_t>(pc - 1);
            push(static_cast<std::uint8_t>((returnAddress >> 8) & 0xFF));
            push(static_cast<std::uint8_t>(returnAddress & 0xFF));
            pc = target;
            break;
        }
        case 0x60: {
            const std::uint8_t low = pop();
            const std::uint8_t high = pop();
            pc = static_cast<std::uint16_t>(
                (low | (static_cast<std::uint16_t>(high) << 8)) + 1);
            break;
        }
        case 0x4C: pc = fetchWord(); break;
        case 0xEA: break;
        case 0x00:
            isHalted = true;
            lastError = "BRK instruction reached; CPU stopped.";
            return false;
        default: {
            isHalted = true;
            lastError = "Unsupported 6502 opcode $";
            constexpr char digits[] = "0123456789ABCDEF";
            lastError += digits[(opcode >> 4) & 0x0F];
            lastError += digits[opcode & 0x0F];
            return false;
        }
    }
    status |= FLAG_UNUSED;
    return true;
}

bool CPU6502::halted() const { return isHalted; }
const std::string& CPU6502::error() const { return lastError; }
