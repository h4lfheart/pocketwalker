#include "m95512.h"

#include <print>

void M95512::Receive(uint8_t data)
{
    switch (state)
    {
    case M95512State::IDLE:

        if (data == M95512_CMD_WRITE_MEMORY)
        {
            is_reading = false;
            state = M95512State::ADDR_HIGH;
        }
        else if (data == M95512_CMD_READ_MEMORY)
        {
            is_reading = true;
            state = M95512State::ADDR_HIGH;
        }
        else if (data == M95512_CMD_READ_STATUS)
        {
            state = M95512State::STATUS;
        }
        else if (data == M95512_CMD_WRITE_ENABLE)
        {
            status |= M95512_STATUS_WRITE_UNLOCK;
            state = M95512State::IDLE;
        }
        return;
    case M95512State::ADDR_HIGH:
        high_addr = data;
        state = M95512State::ADDR_LOW;
        return;
    case M95512State::ADDR_LOW:
        low_addr = data;
        state = M95512State::MEMORY;
        return;
    case M95512State::MEMORY:
        if (!is_reading)
        {
            const uint32_t address = ((high_addr << 8 | low_addr) + offset) & 0xFFFF;
            eeprom[address] = data;
            offset++;
            offset %= 128;
        }
        return;
    }
}

uint8_t M95512::Transmit()
{
    switch (state)
    {
    case M95512State::MEMORY:
        {
            if (is_reading)
            {
                uint32_t address = ((high_addr << 8 | low_addr) + offset) & 0xFFFF;
                const uint8_t value = eeprom[address];
                offset++;

                return value;
            }

            return 0xFF;
        }
    case M95512State::STATUS:
        state = M95512State::IDLE;
        return status;
    default:
        return 0xFF;
    }
}

void M95512::Reset()
{
    state = M95512State::IDLE;
    offset = 0;
}

void M95512::SaveEmulatorState(std::ostream& stream) const
{
    const uint8_t state_value = static_cast<uint8_t>(state);
    stream.write(reinterpret_cast<const char*>(&state_value), sizeof(state_value));
    stream.write(reinterpret_cast<const char*>(&is_reading), sizeof(is_reading));
    stream.write(reinterpret_cast<const char*>(&status), sizeof(status));
    stream.write(reinterpret_cast<const char*>(&high_addr), sizeof(high_addr));
    stream.write(reinterpret_cast<const char*>(&low_addr), sizeof(low_addr));
    stream.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
}

bool M95512::LoadEmulatorState(std::istream& stream)
{
    uint8_t state_value = 0;
    stream.read(reinterpret_cast<char*>(&state_value), sizeof(state_value));
    stream.read(reinterpret_cast<char*>(&is_reading), sizeof(is_reading));
    stream.read(reinterpret_cast<char*>(&status), sizeof(status));
    stream.read(reinterpret_cast<char*>(&high_addr), sizeof(high_addr));
    stream.read(reinterpret_cast<char*>(&low_addr), sizeof(low_addr));
    stream.read(reinterpret_cast<char*>(&offset), sizeof(offset));
    if (!stream)
        return false;

    if (state_value > static_cast<uint8_t>(M95512State::STATUS))
        state_value = static_cast<uint8_t>(M95512State::IDLE);

    state = static_cast<M95512State>(state_value);
    offset %= 128;
    return true;
}
