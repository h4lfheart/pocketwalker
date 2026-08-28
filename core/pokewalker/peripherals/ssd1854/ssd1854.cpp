#include "ssd1854.h"

#include <algorithm>
#include <print>

#include "core/utils/logger.h"

SSD1854::SSD1854()
{
    OnInputPin += [this](PinEvent event)
    {
        if (event.pin == SSD1854_PIN_DC)
            is_data_mode = event.value;
    };
}

void SSD1854::Receive(uint8_t data)
{
    if (is_data_mode)
    {
        if (column >= SSD1854_TOTAL_COLUMNS)
            return;

        const uint16_t address = (page * SSD1854_TOTAL_COLUMNS * SSD1854_COLUMN_SIZE) + (column * SSD1854_COLUMN_SIZE) +offset;

        draw_info.vram.Write8(address, data);

        if (offset == 1)
            column++;

        offset++;
        offset %= 2;
    }
    else
    {
        switch (state)
        {
        case SSD1854State::IDLE:
            HandleCommand(data);
            break;
        case SSD1854State::SET_CONTRAST:
            draw_info.contrast = data;
            state = SSD1854State::IDLE;
            break;
        case SSD1854State::SET_PAGE_OFFSET:
            draw_info.page_offset = std::clamp(data / 8, 0, 14);
            state = SSD1854State::IDLE;
            break;
        }
    }
}

uint8_t SSD1854::Transmit()
{
    return 0xFF;
}

void SSD1854::HandleCommand(uint8_t data)
{
    if (data >= SSD1854_CMD_COL_LOW_MIN && data <= SSD1854_CMD_COL_LOW_MAX)
    {
        column = (column & 0xF0) | (data & 0x0F);
        offset = 0;
        state = SSD1854State::IDLE;
    }
    else if (data >= SSD1854_CMD_COL_HIGH_MIN && data <= SSD1854_CMD_COL_HIGH_MAX)
    {
        column = (column & 0x0F) | ((data & 0b111) << 4);
        offset = 0;
        state = SSD1854State::IDLE;
    }
    else if (data == SSD1854_CMD_SET_CONTRAST)
    {
        state = SSD1854State::SET_CONTRAST;
    }
    else if (data >= SSD1854_CMD_SET_PAGE_OFFSET_MIN && data <= SSD1854_CMD_SET_PAGE_OFFSET_MAX)
    {
        state = SSD1854State::SET_PAGE_OFFSET;
    }
    else if (data >= SSD1854_CMD_SET_PAGE_MIN && data <= SSD1854_CMD_SET_PAGE_MAX)
    {
        page = data & 0xF;
        state = SSD1854State::IDLE;
    }
    else if (data == SSD1854_CMD_POWER_SAVE_ON)
    {
        draw_info.power_save_mode = true;
        state = SSD1854State::IDLE;
    }
    else if (data == SSD1854_CMD_POWER_SAVE_OFF)
    {
        draw_info.power_save_mode = false;
        state = SSD1854State::IDLE;
    }
    else if (data == SSD1854_CMD_RESET)
    {
        column = 0;
        offset = 0;
        page = 0;
        draw_info.contrast = 20;
        draw_info.page_offset = 0;
        draw_info.power_save_mode = false;
        state = SSD1854State::IDLE;
    }
    else
    {
        Log::Warn("Invalid SSD1854 Command: 0x{:02X}", data);
        state = SSD1854State::IDLE;
    }
}

void SSD1854::SaveEmulatorState(std::ostream& stream) const
{
    const uint8_t state_value = static_cast<uint8_t>(state);
    stream.write(reinterpret_cast<const char*>(&state_value), sizeof(state_value));
    stream.write(reinterpret_cast<const char*>(&column), sizeof(column));
    stream.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
    stream.write(reinterpret_cast<const char*>(&page), sizeof(page));
    stream.write(reinterpret_cast<const char*>(&is_data_mode), sizeof(is_data_mode));
}

bool SSD1854::LoadEmulatorState(std::istream& stream)
{
    uint8_t state_value = 0;
    stream.read(reinterpret_cast<char*>(&state_value), sizeof(state_value));
    stream.read(reinterpret_cast<char*>(&column), sizeof(column));
    stream.read(reinterpret_cast<char*>(&offset), sizeof(offset));
    stream.read(reinterpret_cast<char*>(&page), sizeof(page));
    stream.read(reinterpret_cast<char*>(&is_data_mode), sizeof(is_data_mode));
    if (!stream)
        return false;

    if (state_value > static_cast<uint8_t>(SSD1854State::SET_PAGE_OFFSET))
        state_value = static_cast<uint8_t>(SSD1854State::IDLE);

    state = static_cast<SSD1854State>(state_value);
    column = std::min<uint8_t>(column, SSD1854_TOTAL_COLUMNS);
    offset %= SSD1854_COLUMN_SIZE;
    page %= SSD1854_TOTAL_PAGES;
    return true;
}
