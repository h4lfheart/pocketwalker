#pragma once
#include <mutex>

#include "core/memory/memory.h"
#include "core/soc/ssu/peripheral.h"

#define NT7508_PIN_DC 0

#define NT7508_CMD_COL_LOW_MIN 0x00
#define NT7508_CMD_COL_LOW_MAX 0x0F
#define NT7508_CMD_COL_HIGH_MIN 0x10
#define NT7508_CMD_COL_HIGH_MAX 0x17
#define NT7508_CMD_SET_INITIAL_DISPLAY_LINE_MIN 0x40
#define NT7508_CMD_SET_INITIAL_DISPLAY_LINE_MAX 0x43
#define NT7508_CMD_SET_INITIAL_COM0_MIN 0x44
#define NT7508_CMD_SET_N_LINE_INVERSION_MAX 0x4F
#define NT7508_CMD_SET_CONTRAST 0x81
#define NT7508_CMD_SET_GRAY_PULSE_WIDTH_MIN 0x88
#define NT7508_CMD_SET_GRAY_PULSE_WIDTH_MAX 0x8F
#define NT7508_CMD_POWER_SAVE_ON 0xA9
#define NT7508_CMD_SET_PAGE_MIN 0xB0
#define NT7508_CMD_SET_PAGE_MAX 0xBF
#define NT7508_CMD_POWER_SAVE_OFF 0xE1
#define NT7508_CMD_SET_TEMPERATURE_COEFFICIENT 0xF1
#define NT7508_CMD_SET_FRAME_FREQUENCY 0xF6
#define NT7508_CMD_SET_OSCILLATOR_SOURCE 0xF7

#define NT7508_COLUMN_SIZE 2
#define NT7508_TOTAL_COLUMNS 128
#define NT7508_TOTAL_PAGES 16
#define NT7508_GRAPHICS_HEIGHT (NT7508_TOTAL_PAGES * 8)
#define NT7508_INITIAL_DISPLAY_LINE_MAX 127

#define NT7508_MEM_SIZE (NT7508_TOTAL_COLUMNS * NT7508_COLUMN_SIZE * NT7508_TOTAL_PAGES)

enum class NT7508State
{
    IDLE,
    SET_CONTRAST,
    SET_INITIAL_DISPLAY_LINE,
    IGNORE_VALUE
};

struct NT7508DrawInfo
{
    Memory<NT7508_MEM_SIZE> vram = {};
    uint8_t initial_display_line = 0;
    uint8_t contrast = 20;
    bool power_save = false;
};

class NT7508 : public Peripheral
{
public:
    NT7508();

    void Receive(uint8_t data) override;
    uint8_t Transmit() override;

    NT7508DrawInfo GetDrawInfo() const;

private:
    void HandleCommand(uint8_t data);

    NT7508State state = NT7508State::IDLE;

    uint8_t column = 0;
    uint8_t offset = 0;
    uint8_t page = 0;

    bool is_data_mode = false;

    NT7508DrawInfo draw_info = {};
    mutable std::mutex draw_info_mutex;
};
