#pragma once
#include <array>
#include <cstdint>
#include <nlohmann/json.hpp>

#include "core/pokewalker/peripherals/bma150/step_sample_provider.h"

struct EmulationSettings
{
    struct Color
    {
        uint8_t r, g, b;
    };

    std::array<Color, 4> palette = {
        {
            {0xCC, 0xCC, 0xCC},
            {0x99, 0x99, 0x99},
            {0x66, 0x66, 0x66},
            {0x33, 0x33, 0x33}
        }
    };

    bool bypass_power_save = false;

    uint8_t step_period = STEP_SAMPLE_DEFAULT_PERIOD;
};

inline void to_json(nlohmann::json& j, const EmulationSettings::Color& c)
{
    j = nlohmann::json{{"r", c.r}, {"g", c.g}, {"b", c.b}};
}

inline void from_json(const nlohmann::json& j, EmulationSettings::Color& c)
{
    c.r = j.value("r", static_cast<uint8_t>(0));
    c.g = j.value("g", static_cast<uint8_t>(0));
    c.b = j.value("b", static_cast<uint8_t>(0));
}

inline void to_json(nlohmann::json& j, const EmulationSettings& s)
{
    j = nlohmann::json{
        {"palette", s.palette},
        {"bypass_power_save", s.bypass_power_save},
        {"step_period", s.step_period}
    };
}

inline void from_json(const nlohmann::json& j, EmulationSettings& s)
{
    if (j.contains("palette") && j["palette"].is_array() && j["palette"].size() == 4)
        s.palette = j["palette"].get<std::array<EmulationSettings::Color, 4>>();

    s.bypass_power_save = j.value("bypass_power_save", false);
    s.step_period = j.value("step_period", STEP_SAMPLE_DEFAULT_PERIOD);
}
