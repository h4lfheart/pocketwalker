#pragma once
#include <atomic>
#include <cstdint>

struct AccelSample
{
    int16_t x, y, z;
};

class SampleProvider
{
public:
    virtual ~SampleProvider() = default;
    virtual AccelSample GetSample() = 0;

    void SetEnabled(bool value)
    {
        is_enabled = value;
    }

    bool IsEnabled() const
    {
        return is_enabled;
    }

protected:
    std::atomic<bool> is_enabled = false;
};