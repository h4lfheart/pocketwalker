#pragma once
#include <array>
#include <cstdint>

#include "sample_provider.h"

#define STEP_SAMPLE_LUT_SIZE 64
#define STEP_SAMPLE_DEFAULT_PERIOD 8

#define STEP_SAMPLE_MIN_PERIOD 6
#define STEP_SAMPLE_MAX_PERIOD 13

static constexpr std::array<int8_t, STEP_SAMPLE_LUT_SIZE> STEP_SINE_LUT = {
    0, 12, 25, 37, 49, 60, 71, 81,
    90, 98, 106, 112, 117, 122, 125, 126,
    127, 126, 125, 122, 117, 112, 106, 98,
    90, 81, 71, 60, 49, 37, 25, 12,
    0, -12, -25, -37, -49, -60, -71, -81,
    -90, -98, -106, -112, -117, -122, -125, -126,
    -127, -126, -125, -122, -117, -112, -106, -98,
    -90, -81, -71, -60, -49, -37, -25, -12
};

class StepSampleProvider : public SampleProvider
{
public:
    AccelSample GetSample() override
    {
        const auto index = static_cast<uint16_t>((phase >> 8) & (STEP_SAMPLE_LUT_SIZE - 1));
        phase += phase_step;

        if (!IsEnabled())
            return {0, 0, 0};

        const auto x = static_cast<int16_t>((STEP_SINE_LUT[index] * amplitude) >> 7);
        return {x, static_cast<int16_t>(x / 2), 0};
    }

    void SetPeriod(uint8_t value)
    {
        period = value < STEP_SAMPLE_MIN_PERIOD ? STEP_SAMPLE_MIN_PERIOD : (value > STEP_SAMPLE_MAX_PERIOD ? STEP_SAMPLE_MAX_PERIOD : value);
        phase_step = (STEP_SAMPLE_LUT_SIZE << 8) / period;
    }

private:
    int16_t amplitude = 104;
    uint8_t period = STEP_SAMPLE_DEFAULT_PERIOD;

    uint16_t phase = 0;
    uint16_t phase_step = (STEP_SAMPLE_LUT_SIZE << 8) / STEP_SAMPLE_DEFAULT_PERIOD;
};