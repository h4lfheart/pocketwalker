#include "ssd1854.h"

#include <algorithm>

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
    std::lock_guard lock(draw_info_mutex);

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

SSD1854DrawInfo SSD1854::GetDrawInfo() const
{
    std::lock_guard lock(draw_info_mutex);
    return draw_info;
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
