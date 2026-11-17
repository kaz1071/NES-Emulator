#pragma once
#include <array>
#include <cstdint>
#include <vector>

class NESAPU {
public:
    struct PulseState {
        std::uint16_t timer = 0, timerCounter = 0;
        std::uint8_t duty = 0, sequence = 0, length = 0;
        std::uint8_t envelopePeriod = 0, envelopeDivider = 0, envelopeDecay = 0;
        std::uint8_t sweepPeriod = 0, sweepDivider = 0;
        bool enabled = false, constantVolume = false, envelopeLoop = false;
        bool envelopeStart = false, sweepEnabled = false, sweepNegate = false;
        bool sweepReload = false;
        std::uint8_t sweepShift = 0;
    };
    struct TriangleState {
        std::uint16_t timer = 0, timerCounter = 0;
        std::uint8_t sequence = 0, length = 0, linearReload = 0, linearCounter = 0;
        bool enabled = false, control = false, reloadFlag = false;
    };
    struct NoiseState {
        std::uint16_t timerCounter = 0, shift = 1;
        std::uint8_t periodIndex = 0, length = 0;
        std::uint8_t envelopePeriod = 0, envelopeDivider = 0, envelopeDecay = 0;
        bool enabled = false, mode = false, constantVolume = false;
        bool envelopeLoop = false, envelopeStart = false;
    };
    struct State {
        PulseState pulse1{}, pulse2{};
        TriangleState triangle{};
        NoiseState noise{};
        std::uint32_t frameCycle = 0;
        std::uint32_t sampleAccumulator = 0;
        bool fiveStep = false, irqInhibit = false, frameIrq = false;
        bool evenCycle = false;
    };

    void reset();
    void write(std::uint16_t address, std::uint8_t value);
    std::uint8_t readStatus();
    void clock();
    State saveState() const { return state; }
    void loadState(const State& saved);
    std::vector<std::int16_t> takeSamples();

private:
    State state{};
    std::vector<std::int16_t> samples;

    void quarterFrame();
    void halfFrame();
    void clockEnvelope(PulseState& pulse);
    void clockEnvelope(NoiseState& noise);
    void clockSweep(PulseState& pulse, bool first);
    void clockPulseTimer(PulseState& pulse);
    void clockTriangleTimer();
    void clockNoiseTimer();
    double mixedOutput() const;
    std::uint8_t pulseOutput(const PulseState& pulse) const;
    static std::uint8_t lengthValue(std::uint8_t index);
};
