#include "apu.hpp"
#include <algorithm>
#include <cmath>

namespace {
constexpr std::array<std::uint8_t, 32> LENGTH_TABLE{{
    10,254,20,2,40,4,80,6,160,8,60,10,14,12,26,14,
    12,16,24,18,48,20,96,22,192,24,48,26,96,28,192,30
}};
constexpr std::array<std::uint16_t, 16> NOISE_PERIODS{{
    4,8,16,32,64,96,128,160,202,254,380,508,762,1016,2034,4068
}};
constexpr std::array<std::uint8_t, 4> DUTY_BITS{{0x02,0x06,0x1E,0xF9}};
constexpr std::array<std::uint8_t, 32> TRIANGLE_SEQUENCE{{
    15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
}};
constexpr std::array<std::uint16_t, 16> DMC_PERIODS{{
    428,380,340,320,286,254,226,214,190,160,142,128,106,85,72,54
}};
constexpr std::uint32_t CPU_FREQUENCY = 1789773;
constexpr std::uint32_t SAMPLE_RATE = 44100;
}

std::uint8_t NESAPU::lengthValue(std::uint8_t index) {
    return LENGTH_TABLE[index & 0x1F];
}

void NESAPU::reset() {
    state = {};
    state.noise.shift = 1;
    state.dmc.bitsRemaining = 8;
    state.dmc.sampleBufferEmpty = true;
    state.dmc.silence = true;
    samples.clear();
}

void NESAPU::loadState(const State& saved) {
    state = saved;
    state.noise.shift &= 0x7FFF;
    if (state.noise.shift == 0) state.noise.shift = 1;
    state.dmc.rateIndex &= 0x0F;
    state.dmc.outputLevel &= 0x7F;
    if (state.dmc.bitsRemaining == 0 || state.dmc.bitsRemaining > 8) state.dmc.bitsRemaining = 8;
    samples.clear();
}

void NESAPU::write(std::uint16_t address, std::uint8_t value) {
    switch (address) {
    case 0x4000:
    case 0x4004: {
        auto& p = address == 0x4000 ? state.pulse1 : state.pulse2;
        p.duty = value >> 6;
        p.envelopeLoop = (value & 0x20) != 0;
        p.constantVolume = (value & 0x10) != 0;
        p.envelopePeriod = value & 0x0F;
        break;
    }
    case 0x4001:
    case 0x4005: {
        auto& p = address == 0x4001 ? state.pulse1 : state.pulse2;
        p.sweepEnabled = (value & 0x80) != 0;
        p.sweepPeriod = (value >> 4) & 7;
        p.sweepNegate = (value & 8) != 0;
        p.sweepShift = value & 7;
        p.sweepReload = true;
        break;
    }
    case 0x4002: state.pulse1.timer = (state.pulse1.timer & 0x0700) | value; break;
    case 0x4003:
        state.pulse1.timer = (state.pulse1.timer & 0x00FF) | ((value & 7) << 8);
        if (state.pulse1.enabled) state.pulse1.length = lengthValue(value >> 3);
        state.pulse1.sequence = 0;
        state.pulse1.envelopeStart = true;
        break;
    case 0x4006: state.pulse2.timer = (state.pulse2.timer & 0x0700) | value; break;
    case 0x4007:
        state.pulse2.timer = (state.pulse2.timer & 0x00FF) | ((value & 7) << 8);
        if (state.pulse2.enabled) state.pulse2.length = lengthValue(value >> 3);
        state.pulse2.sequence = 0;
        state.pulse2.envelopeStart = true;
        break;
    case 0x4008:
        state.triangle.control = (value & 0x80) != 0;
        state.triangle.linearReload = value & 0x7F;
        break;
    case 0x400A: state.triangle.timer = (state.triangle.timer & 0x0700) | value; break;
    case 0x400B:
        state.triangle.timer = (state.triangle.timer & 0x00FF) | ((value & 7) << 8);
        if (state.triangle.enabled) state.triangle.length = lengthValue(value >> 3);
        state.triangle.reloadFlag = true;
        break;
    case 0x400C:
        state.noise.envelopeLoop = (value & 0x20) != 0;
        state.noise.constantVolume = (value & 0x10) != 0;
        state.noise.envelopePeriod = value & 0x0F;
        break;
    case 0x400E:
        state.noise.mode = (value & 0x80) != 0;
        state.noise.periodIndex = value & 0x0F;
        break;
    case 0x400F:
        if (state.noise.enabled) state.noise.length = lengthValue(value >> 3);
        state.noise.envelopeStart = true;
        break;
    case 0x4010:
        state.dmc.irqEnabled = (value & 0x80) != 0;
        state.dmc.loop = (value & 0x40) != 0;
        state.dmc.rateIndex = value & 0x0F;
        if (!state.dmc.irqEnabled) state.dmc.irq = false;
        break;
    case 0x4011: state.dmc.outputLevel = value & 0x7F; break;
    case 0x4012: state.dmc.sampleAddress = static_cast<std::uint16_t>(0xC000 | (value << 6)); break;
    case 0x4013: state.dmc.sampleLength = static_cast<std::uint16_t>((value << 4) | 1); break;
    case 0x4015:
        state.dmc.irq = false;
        state.pulse1.enabled = (value & 0x01) != 0;
        state.pulse2.enabled = (value & 0x02) != 0;
        state.triangle.enabled = (value & 0x04) != 0;
        state.noise.enabled = (value & 0x08) != 0;
        if (!state.pulse1.enabled) state.pulse1.length = 0;
        if (!state.pulse2.enabled) state.pulse2.length = 0;
        if (!state.triangle.enabled) state.triangle.length = 0;
        if (!state.noise.enabled) state.noise.length = 0;
        if ((value & 0x10) == 0) state.dmc.bytesRemaining = 0;
        else if (state.dmc.bytesRemaining == 0) {
            state.dmc.currentAddress = state.dmc.sampleAddress;
            state.dmc.bytesRemaining = state.dmc.sampleLength;
        }
        state.frameIrq = false;
        break;
    case 0x4017:
        state.fiveStep = (value & 0x80) != 0;
        state.irqInhibit = (value & 0x40) != 0;
        if (state.irqInhibit) state.frameIrq = false;
        state.frameCycle = 0;
        if (state.fiveStep) { quarterFrame(); halfFrame(); }
        break;
    default: break;
    }
}

std::uint8_t NESAPU::readStatus() {
    std::uint8_t result = 0;
    if (state.pulse1.length) result |= 0x01;
    if (state.pulse2.length) result |= 0x02;
    if (state.triangle.length) result |= 0x04;
    if (state.noise.length) result |= 0x08;
    if (state.dmc.bytesRemaining > 0) result |= 0x10;
    if (state.frameIrq) result |= 0x40;
    if (state.dmc.irq) result |= 0x80;
    state.frameIrq = false;
    return result;
}

void NESAPU::clockEnvelope(PulseState& p) {
    if (p.envelopeStart) {
        p.envelopeStart = false;
        p.envelopeDecay = 15;
        p.envelopeDivider = p.envelopePeriod;
    } else if (p.envelopeDivider == 0) {
        p.envelopeDivider = p.envelopePeriod;
        if (p.envelopeDecay > 0) --p.envelopeDecay;
        else if (p.envelopeLoop) p.envelopeDecay = 15;
    } else --p.envelopeDivider;
}

void NESAPU::clockEnvelope(NoiseState& n) {
    if (n.envelopeStart) {
        n.envelopeStart = false;
        n.envelopeDecay = 15;
        n.envelopeDivider = n.envelopePeriod;
    } else if (n.envelopeDivider == 0) {
        n.envelopeDivider = n.envelopePeriod;
        if (n.envelopeDecay > 0) --n.envelopeDecay;
        else if (n.envelopeLoop) n.envelopeDecay = 15;
    } else --n.envelopeDivider;
}

void NESAPU::quarterFrame() {
    clockEnvelope(state.pulse1);
    clockEnvelope(state.pulse2);
    clockEnvelope(state.noise);
    auto& t = state.triangle;
    if (t.reloadFlag) t.linearCounter = t.linearReload;
    else if (t.linearCounter > 0) --t.linearCounter;
    if (!t.control) t.reloadFlag = false;
}

void NESAPU::clockSweep(PulseState& p, bool first) {
    const std::uint16_t change = p.timer >> p.sweepShift;
    const int target = static_cast<int>(p.timer) +
        (p.sweepNegate ? -static_cast<int>(change) - (first ? 1 : 0)
                       : static_cast<int>(change));
    if (p.sweepDivider == 0 && p.sweepEnabled && p.sweepShift != 0 &&
        p.timer >= 8 && target >= 0 && target <= 0x7FF)
        p.timer = static_cast<std::uint16_t>(target);
    if (p.sweepDivider == 0 || p.sweepReload) {
        p.sweepDivider = p.sweepPeriod;
        p.sweepReload = false;
    } else --p.sweepDivider;
}

void NESAPU::halfFrame() {
    auto lengthClock = [](auto& channel, bool halt) {
        if (!halt && channel.length > 0) --channel.length;
    };
    lengthClock(state.pulse1, state.pulse1.envelopeLoop);
    lengthClock(state.pulse2, state.pulse2.envelopeLoop);
    lengthClock(state.triangle, state.triangle.control);
    lengthClock(state.noise, state.noise.envelopeLoop);
    clockSweep(state.pulse1, true);
    clockSweep(state.pulse2, false);
}

void NESAPU::clockPulseTimer(PulseState& p) {
    if (p.timerCounter == 0) {
        p.timerCounter = p.timer;
        p.sequence = (p.sequence + 1) & 7;
    } else --p.timerCounter;
}

void NESAPU::clockTriangleTimer() {
    auto& t = state.triangle;
    if (t.timerCounter == 0) {
        t.timerCounter = t.timer;
        if (t.enabled && t.length > 0 && t.linearCounter > 0 && t.timer > 1)
            t.sequence = (t.sequence + 1) & 31;
    } else --t.timerCounter;
}

void NESAPU::clockNoiseTimer() {
    auto& n = state.noise;
    if (n.timerCounter == 0) {
        n.timerCounter = NOISE_PERIODS[n.periodIndex] / 2 - 1;
        const unsigned tap = n.mode ? 6 : 1;
        const std::uint16_t feedback = (n.shift ^ (n.shift >> tap)) & 1;
        n.shift = static_cast<std::uint16_t>((n.shift >> 1) | (feedback << 14));
    } else --n.timerCounter;
}

std::uint8_t NESAPU::pulseOutput(const PulseState& p) const {
    if (!p.enabled || p.length == 0 || p.timer < 8 || p.timer > 0x7FF)
        return 0;
    if ((DUTY_BITS[p.duty] & (1u << p.sequence)) == 0) return 0;
    return p.constantVolume ? p.envelopePeriod : p.envelopeDecay;
}

double NESAPU::mixedOutput() const {
    const int pulseSum = pulseOutput(state.pulse1) + pulseOutput(state.pulse2);
    const double pulse = pulseSum == 0 ? 0.0 :
        95.88 / (8128.0 / pulseSum + 100.0);
    const auto& t = state.triangle;
    const int triangle = (t.enabled && t.length && t.linearCounter)
        ? TRIANGLE_SEQUENCE[t.sequence] : 0;
    const auto& n = state.noise;
    const int noise = (n.enabled && n.length && (n.shift & 1) == 0)
        ? (n.constantVolume ? n.envelopePeriod : n.envelopeDecay) : 0;
    const double tndInput = triangle / 8227.0 + noise / 12241.0;
    const double tnd = tndInput == 0.0 ? 0.0 : 159.79 / (1.0 / tndInput + 100.0);
    const double dmc = (state.dmc.outputLevel / 127.0) * 0.08;
    return std::clamp(pulse + tnd + dmc, 0.0, 1.0);
}

void NESAPU::clockDmc() {
    auto& d = state.dmc;
    if (d.timerCounter == 0) {
        d.timerCounter = static_cast<std::uint16_t>(DMC_PERIODS[d.rateIndex] - 1);
        if (!d.silence) {
            if (d.shiftRegister & 1) {
                if (d.outputLevel <= 125) d.outputLevel += 2;
            } else if (d.outputLevel >= 2) d.outputLevel -= 2;
        }
        d.shiftRegister >>= 1;
        if (--d.bitsRemaining == 0) {
            d.bitsRemaining = 8;
            if (d.sampleBufferEmpty) d.silence = true;
            else {
                d.silence = false;
                d.shiftRegister = d.sampleBuffer;
                d.sampleBufferEmpty = true;
            }
        }
    } else {
        --d.timerCounter;
    }

    if (d.sampleBufferEmpty && d.bytesRemaining > 0 && !d.dmaPending && !d.dmaInProgress) {
        d.dmaAddress = d.currentAddress;
        d.dmaPending = true;
    }
}

void NESAPU::clock() {
    state.evenCycle = !state.evenCycle;
    if (state.evenCycle) {
        clockPulseTimer(state.pulse1);
        clockPulseTimer(state.pulse2);
        clockNoiseTimer();
    }
    clockTriangleTimer();
    clockDmc();

    ++state.frameCycle;
    if (state.frameCycle == 3729 || state.frameCycle == 11186) quarterFrame();
    if (state.frameCycle == 7457) { quarterFrame(); halfFrame(); }
    if (!state.fiveStep && state.frameCycle >= 14915) {
        quarterFrame(); halfFrame();
        if (!state.irqInhibit) state.frameIrq = true;
        state.frameCycle = 0;
    } else if (state.fiveStep && state.frameCycle >= 18641) {
        quarterFrame(); halfFrame();
        state.frameCycle = 0;
    }

    state.sampleAccumulator += SAMPLE_RATE;
    if (state.sampleAccumulator >= CPU_FREQUENCY) {
        state.sampleAccumulator -= CPU_FREQUENCY;
        const double sample = mixedOutput() * 24000.0;
        samples.push_back(static_cast<std::int16_t>(sample));
    }
}

std::vector<std::int16_t> NESAPU::takeSamples() {
    std::vector<std::int16_t> result;
    result.swap(samples);
    return result;
}


bool NESAPU::consumeDmcDmaRequest(std::uint16_t& address) {
    auto& d = state.dmc;
    if (!d.dmaPending) return false;
    address = d.dmaAddress;
    d.dmaPending = false;
    d.dmaInProgress = true;
    return true;
}

void NESAPU::completeDmcDma(std::uint8_t value) {
    auto& d = state.dmc;
    d.dmaInProgress = false;
    if (!d.sampleBufferEmpty || d.bytesRemaining == 0) return;
    d.sampleBuffer = value;
    d.sampleBufferEmpty = false;
    d.currentAddress = d.currentAddress == 0xFFFF
        ? 0x8000 : static_cast<std::uint16_t>(d.currentAddress + 1);
    --d.bytesRemaining;
    if (d.bytesRemaining == 0) {
        if (d.loop) {
            d.currentAddress = d.sampleAddress;
            d.bytesRemaining = d.sampleLength;
        } else if (d.irqEnabled) {
            d.irq = true;
        }
    }
}
