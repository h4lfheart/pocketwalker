#include "bma150.h"
#include "core/soc/defines.h"

BMA150::BMA150()
{
    control1 = mem.Ptr8(BMA150_ADDR_CONTROL_1);
    *control1 = BMA150_CONTROL_SLEEP;

    mem.Write8(BMA150_ADDR_CHIP_ID, BMA150_CHIP_ID);
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

    if (register_index == BMA150_ADDR_ACC_X_LSB && offset == 0 && sample_provider && sample_provider->IsEnabled())
    {
        auto [x, y, z] = sample_provider->GetSample();
        SetAxisRegisters(x, y, z);
    }

    return mem.Read8(register_index + offset++);
}

void BMA150::SetAxisRegisters(int16_t x, int16_t y, int16_t z)
{
    const auto write = [this](uint8_t lsb_address, uint8_t msb_address, int16_t value)
    {
        mem.Write8(msb_address, static_cast<uint8_t>(value >> 2));
        mem.Write8(lsb_address, static_cast<uint8_t>(((value & 0x3) << 6) | BMA150_ACC_NEW_DATA));
    };

    write(BMA150_ADDR_ACC_X_LSB, BMA150_ADDR_ACC_X_MSB, x);
    write(BMA150_ADDR_ACC_Y_LSB, BMA150_ADDR_ACC_Y_MSB, y);
    write(BMA150_ADDR_ACC_Z_LSB, BMA150_ADDR_ACC_Z_MSB, z);
}

void BMA150::Reset()
{
    state = BMA150State::IDLE;
    is_reading = false;
    register_index = 0;
    offset = 0;
}

void BMA150::Cycle(uint32_t cycles)
{
    if (*control1 & BMA150_CONTROL_SLEEP)
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