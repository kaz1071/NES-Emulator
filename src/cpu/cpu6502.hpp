#pragma once

#include "../cartridge/cartridge.hpp"

#include <array>
#include <cstdint>
#include <string>

class CPU6502 {
public:
    // The NES CPU has 2 KiB of internal RAM, mirrored through $1FFF.
    void connectCartridge(const Cartridge* cartridge);
    void reset();

    // Executes one instruction. Returns false if the CPU is halted or an
    // unsupported opcode is encountered.
    bool step();

    std::uint8_t a = 0;       // Accumulator
    std::uint8_t x = 0;       // X index register
    std::uint8_t y = 0;       // Y index register
    std::uint8_t sp = 0xFD;   // Stack pointer
    std::uint16_t pc = 0;     // Program counter
    std::uint8_t status = 0x24;

    bool halted() const;
    const std::string& error() const;

private:
    static constexpr std::uint8_t FLAG_ZERO = 0x02;
    static constexpr std::uint8_t FLAG_NEGATIVE = 0x80;

    std::array<std::uint8_t, 2048> ram{};
    const Cartridge* cartridge = nullptr;
    bool isHalted = false;
    std::string lastError;

    std::uint8_t read(std::uint16_t address) const;
    void write(std::uint16_t address, std::uint8_t value);
    void setZeroNegative(std::uint8_t value);
};
