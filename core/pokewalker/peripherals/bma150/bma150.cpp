#include "bma150.h"
#include "core/soc/defines.h"

BMA150::BMA150()
{
    control1 = mem.Ptr8(BMA150_ADDR_CONTROL_1);
    *control1 = 1;
}

void BMA150::Receive(uint8_t data)
{
    switch (state)
    {
    case BMA150State::IDLE:
        is_reading = data & 0b10000000;
        register_index = data & 0b01111111;
        offset = 0;
        state = BMA150State::MEMORY;
        break;
    case BMA150State::MEMORY:
        if (!is_reading)
            mem.Write8(register_index, data);
        break;
    }
}

uint8_t BMA150::Transmit()
{
    if (!is_reading)
        return 0xFF;

    if (register_index == BMA150_ADDR_ACC_X_LSB && offset == 0 && sample_provider && sample_provider->is_enabled)
    {
        auto [x, y, z] = sample_provider->GetSample();
        mem.Write8(BMA150_ADDR_ACC_X_MSB, static_cast<uint8_t>(x));
        mem.Write8(BMA150_ADDR_ACC_Y_MSB, static_cast<uint8_t>(y));
        mem.Write8(BMA150_ADDR_ACC_Z_MSB, static_cast<uint8_t>(z));
    }

    return mem.Read8(register_index + offset++);
}

void BMA150::Reset()
{
    state = BMA150State::IDLE;
    offset = 0;
}

void BMA150::Cycle(uint32_t cycles)
{
    if (*control1 & 1) // sleep
        return;

    const uint16_t clock_rate = BMA150_CLOCK_RATES[
        mem.Read8(BMA150_ADDR_RANGE_BW_REG) & 0b111];

    cycle_count += cycles;
    if (cycle_count < PHI_CLK / clock_rate)
        return;

    cycle_count -= PHI_CLK / clock_rate;

    OnOutputPin({BMA150_PIN_INT, true});
    OnOutputPin({BMA150_PIN_INT, false});
}

void BMA150::SaveEmulatorState(std::ostream& stream)
{
    for (uint16_t i = 0; i < 0x80; i++)
    {
        const uint8_t value = mem.Read8(i);
        stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    const uint8_t state_value = static_cast<uint8_t>(state);
    stream.write(reinterpret_cast<const char*>(&state_value), sizeof(state_value));
    stream.write(reinterpret_cast<const char*>(&is_reading), sizeof(is_reading));
    stream.write(reinterpret_cast<const char*>(&register_index), sizeof(register_index));
    stream.write(reinterpret_cast<const char*>(&cycle_count), sizeof(cycle_count));
    stream.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
}

bool BMA150::LoadEmulatorState(std::istream& stream)
{
    for (uint16_t i = 0; i < 0x80; i++)
    {
        uint8_t value = 0;
        stream.read(reinterpret_cast<char*>(&value), sizeof(value));
        if (!stream)
            return false;
        mem.Write8(i, value);
    }

    uint8_t state_value = 0;
    stream.read(reinterpret_cast<char*>(&state_value), sizeof(state_value));
    stream.read(reinterpret_cast<char*>(&is_reading), sizeof(is_reading));
    stream.read(reinterpret_cast<char*>(&register_index), sizeof(register_index));
    stream.read(reinterpret_cast<char*>(&cycle_count), sizeof(cycle_count));
    stream.read(reinterpret_cast<char*>(&offset), sizeof(offset));
    if (!stream)
        return false;

    if (state_value > static_cast<uint8_t>(BMA150State::MEMORY))
        state_value = static_cast<uint8_t>(BMA150State::IDLE);

    state = static_cast<BMA150State>(state_value);
    register_index &= 0x7F;
    return true;
}
