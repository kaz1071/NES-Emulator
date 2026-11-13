#pragma once
#include "../bus/bus.hpp"
#include <cstdint>
#include <array>
#include <cstddef>
#include <string>

class CPU6502 {
public:
    void connectBus(NESBus* bus);
    void reset();
    bool step();

    std::uint8_t a = 0, x = 0, y = 0;
    std::uint8_t sp = 0xFD;
    std::uint16_t pc = 0;
    std::uint8_t status = 0x24;

    bool halted() const;
    const std::string& error() const;
    std::uint16_t debugTraceAddress(std::size_t index) const;
    std::uint8_t debugTraceOpcode(std::size_t index) const;

private:
    static constexpr std::uint8_t FLAG_CARRY = 0x01;
    static constexpr std::uint8_t FLAG_ZERO = 0x02;
    static constexpr std::uint8_t FLAG_INTERRUPT = 0x04;
    static constexpr std::uint8_t FLAG_DECIMAL = 0x08;
    static constexpr std::uint8_t FLAG_BREAK = 0x10;
    static constexpr std::uint8_t FLAG_UNUSED = 0x20;
    static constexpr std::uint8_t FLAG_OVERFLOW = 0x40;
    static constexpr std::uint8_t FLAG_NEGATIVE = 0x80;

    NESBus* bus = nullptr;
    bool isHalted = false;
    std::string lastError;
    std::array<std::uint16_t, 16> traceAddresses{};
    std::array<std::uint8_t, 16> traceOpcodes{};
    std::size_t traceIndex = 0;

    std::uint8_t read(std::uint16_t address);
    void write(std::uint16_t address, std::uint8_t value);
    void setFlag(std::uint8_t flag, bool value);
    bool getFlag(std::uint8_t flag) const;
    void setZeroNegative(std::uint8_t value);
    void push(std::uint8_t value);
    std::uint8_t pop();
    void adc(std::uint8_t value);
    void compare(std::uint8_t reg, std::uint8_t value);
};
