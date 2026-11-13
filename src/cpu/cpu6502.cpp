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
    traceAddresses.fill(0);
    traceOpcodes.fill(0);
    traceIndex = 0;
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
    const auto indx = [this, &fetch]() {
        const std::uint8_t pointer = static_cast<std::uint8_t>(fetch() + x);
        const std::uint8_t low = read(pointer);
        const std::uint8_t high = read(static_cast<std::uint8_t>(pointer + 1));
        return static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8));
    };
    const auto branch = [this, &fetch](bool condition) {
        const auto offset = static_cast<std::int8_t>(fetch());
        if (condition) pc = static_cast<std::uint16_t>(pc + offset);
    };

    const std::uint16_t instructionAddress = pc;
    const std::uint8_t opcode = fetch();
    traceAddresses[traceIndex] = instructionAddress;
    traceOpcodes[traceIndex] = opcode;
    traceIndex = (traceIndex + 1) % traceAddresses.size();
    switch (opcode) {

        // Read/modify/write and logic instructions commonly used by NES games.
        case 0x01: a |= read(indx()); setZeroNegative(a); break;
        case 0x05: a |= read(zp()); setZeroNegative(a); break;
        case 0x0D: a |= read(fetchWord()); setZeroNegative(a); break;
        case 0x11: a |= read(indy()); setZeroNegative(a); break;
        case 0x15: a |= read(zpx()); setZeroNegative(a); break;
        case 0x19: a |= read(absy()); setZeroNegative(a); break;
        case 0x1D: a |= read(absx()); setZeroNegative(a); break;

        case 0x21: a &= read(indx()); setZeroNegative(a); break;
        case 0x25: a &= read(zp()); setZeroNegative(a); break;
        case 0x2D: a &= read(fetchWord()); setZeroNegative(a); break;
        case 0x31: a &= read(indy()); setZeroNegative(a); break;
        case 0x35: a &= read(zpx()); setZeroNegative(a); break;
        case 0x39: a &= read(absy()); setZeroNegative(a); break;
        case 0x3D: a &= read(absx()); setZeroNegative(a); break;

        case 0x41: a ^= read(indx()); setZeroNegative(a); break;
        case 0x45: a ^= read(zp()); setZeroNegative(a); break;
        case 0x4D: a ^= read(fetchWord()); setZeroNegative(a); break;
        case 0x51: a ^= read(indy()); setZeroNegative(a); break;
        case 0x55: a ^= read(zpx()); setZeroNegative(a); break;
        case 0x59: a ^= read(absy()); setZeroNegative(a); break;
        case 0x5D: a ^= read(absx()); setZeroNegative(a); break;

        case 0x61: adc(read(indx())); break;
        case 0x6D: adc(read(fetchWord())); break;
        case 0x71: adc(read(indy())); break;
        case 0x75: adc(read(zpx())); break;
        case 0x79: adc(read(absy())); break;
        case 0x7D: adc(read(absx())); break;

        case 0x81: write(indx(), a); break;
        case 0x91: write(indy(), a); break;

        case 0xA1: a = read(indx()); setZeroNegative(a); break;

        case 0xC1: compare(a, read(indx())); break;
        case 0xC5: compare(a, read(zp())); break;
        case 0xCD: compare(a, read(fetchWord())); break;
        case 0xD1: compare(a, read(indy())); break;
        case 0xD5: compare(a, read(zpx())); break;
        case 0xD9: compare(a, read(absy())); break;
        case 0xDD: compare(a, read(absx())); break;

        case 0xE1: adc(static_cast<std::uint8_t>(~read(indx()))); break;
        case 0xE5: adc(static_cast<std::uint8_t>(~read(zp()))); break;
        case 0xED: adc(static_cast<std::uint8_t>(~read(fetchWord()))); break;
        case 0xF1: adc(static_cast<std::uint8_t>(~read(indy()))); break;
        case 0xF5: adc(static_cast<std::uint8_t>(~read(zpx()))); break;
        case 0xF9: adc(static_cast<std::uint8_t>(~read(absy()))); break;
        case 0xFD: adc(static_cast<std::uint8_t>(~read(absx()))); break;

        case 0x24: {
            const auto value = read(zp());
            setFlag(FLAG_ZERO, (a & value) == 0);
            setFlag(FLAG_OVERFLOW, (value & 0x40) != 0);
            setFlag(FLAG_NEGATIVE, (value & 0x80) != 0);
            break;
        }
        case 0x2C: {
            const auto value = read(fetchWord());
            setFlag(FLAG_ZERO, (a & value) == 0);
            setFlag(FLAG_OVERFLOW, (value & 0x40) != 0);
            setFlag(FLAG_NEGATIVE, (value & 0x80) != 0);
            break;
        }

        case 0x26: case 0x2E: case 0x36: case 0x3E: {
            const std::uint16_t address = opcode == 0x26 ? zp() :
                opcode == 0x2E ? fetchWord() :
                opcode == 0x36 ? zpx() : absx();
            const auto oldValue = read(address);
            const auto result = static_cast<std::uint8_t>((oldValue << 1) | (getFlag(FLAG_CARRY) ? 1 : 0));
            setFlag(FLAG_CARRY, (oldValue & 0x80) != 0);
            write(address, result); setZeroNegative(result); break;
        }
        case 0x46: case 0x4E: case 0x56: case 0x5E: {
            const std::uint16_t address = opcode == 0x46 ? zp() :
                opcode == 0x4E ? fetchWord() :
                opcode == 0x56 ? zpx() : absx();
            const auto oldValue = read(address);
            const auto result = static_cast<std::uint8_t>((oldValue >> 1) | (getFlag(FLAG_CARRY) ? 0x80 : 0));
            setFlag(FLAG_CARRY, (oldValue & 0x01) != 0);
            write(address, result); setZeroNegative(result); break;
        }
        case 0x66: case 0x6E: case 0x76: case 0x7E: {
            const std::uint16_t address = opcode == 0x66 ? zp() :
                opcode == 0x6E ? fetchWord() :
                opcode == 0x76 ? zpx() : absx();
            const auto oldValue = read(address);
            const auto result = static_cast<std::uint8_t>((oldValue >> 1) | (getFlag(FLAG_CARRY) ? 0x80 : 0));
            setFlag(FLAG_CARRY, (oldValue & 0x01) != 0);
            write(address, result); setZeroNegative(result); break;
        }
        case 0x06: case 0x0E: case 0x16: case 0x1E: {
            const std::uint16_t address = opcode == 0x06 ? zp() :
                opcode == 0x0E ? fetchWord() : opcode == 0x16 ? zpx() : absx();
            const auto oldValue = read(address);
            const auto result = static_cast<std::uint8_t>(oldValue << 1);
            setFlag(FLAG_CARRY, (oldValue & 0x80) != 0);
            write(address, result); setZeroNegative(result); break;
        }
        case 0x2A: {
            const bool carryIn = getFlag(FLAG_CARRY);
            setFlag(FLAG_CARRY, (a & 0x80) != 0);
            a = static_cast<std::uint8_t>((a << 1) | (carryIn ? 1 : 0));
            setZeroNegative(a); break;
        }
        case 0x6A: {
            const bool carryIn = getFlag(FLAG_CARRY);
            setFlag(FLAG_CARRY, (a & 0x01) != 0);
            a = static_cast<std::uint8_t>((a >> 1) | (carryIn ? 0x80 : 0));
            setZeroNegative(a); break;
        }

        case 0xC6: case 0xCE: case 0xD6: case 0xDE: {
            const std::uint16_t address = opcode == 0xC6 ? zp() :
                opcode == 0xCE ? fetchWord() :
                opcode == 0xD6 ? zpx() : absx();
            const auto result = static_cast<std::uint8_t>(read(address) - 1);
            write(address, result); setZeroNegative(result); break;
        }
        case 0xEE: case 0xF6: case 0xFE: {
            const std::uint16_t address = opcode == 0xEE ? fetchWord() :
                opcode == 0xF6 ? zpx() : absx();
            const auto result = static_cast<std::uint8_t>(read(address) + 1);
            write(address, result); setZeroNegative(result); break;
        }

        case 0xE4: compare(x, read(zp())); break;
        case 0xEC: compare(x, read(fetchWord())); break;
        case 0xC4: compare(y, read(zp())); break;
        case 0xCC: compare(y, read(fetchWord())); break;
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

        // Accumulator shifts
        case 0x4A: {
            setFlag(FLAG_CARRY, (a & 0x01) != 0);
            a = static_cast<std::uint8_t>(a >> 1);
            setZeroNegative(a);
            break;
        }
        case 0x0A: {
            setFlag(FLAG_CARRY, (a & 0x80) != 0);
            a = static_cast<std::uint8_t>(a << 1);
            setZeroNegative(a);
            break;
        }

        // Increment/decrement memory
        case 0xE6: {
            const auto address = zp();
            const auto value = static_cast<std::uint8_t>(read(address) + 1);
            write(address, value);
            setZeroNegative(value);
            break;
        }

        // Zero-page arithmetic
        case 0x65: adc(read(zp())); break;

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
        case 0x6C: {
            const std::uint16_t pointer = fetchWord();
            const std::uint8_t low = read(pointer);
            // The original 6502 wraps the high-byte fetch within the same page.
            const std::uint16_t highAddress = static_cast<std::uint16_t>(
                (pointer & 0xFF00) | static_cast<std::uint8_t>(pointer + 1));
            const std::uint8_t high = read(highAddress);
            pc = static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8));
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
std::uint16_t CPU6502::debugTraceAddress(std::size_t index) const {
    return traceAddresses[(traceIndex + index) % traceAddresses.size()];
}
std::uint8_t CPU6502::debugTraceOpcode(std::size_t index) const {
    return traceOpcodes[(traceIndex + index) % traceOpcodes.size()];
}
