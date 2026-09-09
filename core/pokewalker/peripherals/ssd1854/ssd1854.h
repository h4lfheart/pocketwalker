#pragma once
#include <istream>
#include <ostream>

#include "core/memory/memory.h"
#include "core/soc/ssu/peripheral.h"

#define SSD1854_PIN_DC 0

#define SSD1854_CMD_COL_LOW_MIN 0x0
#define SSD1854_CMD_COL_LOW_MAX 0xF
#define SSD1854_CMD_COL_HIGH_MIN 0x10
#define SSD1854_CMD_COL_HIGH_MAX 0x17

#define SSD1854_CMD_SET_PAGE_OFFSET_MIN 0x40
#define SSD1854_CMD_SET_PAGE_OFFSET_MAX 0x43

#define SSD1854_CMD_SET_CONTRAST 0x81

#define SSD1854_CMD_POWER_SAVE_ON 0xA9

#define SSD1854_CMD_SET_PAGE_MIN 0xB0
#define SSD1854_CMD_SET_PAGE_MAX 0xBF

#define SSD1854_CMD_POWER_SAVE_OFF 0xE1
#define SSD1854_CMD_RESET 0xE2

#define SSD1854_COLUMN_SIZE 2
#define SSD1854_TOTAL_COLUMNS 128
#define SSD1854_TOTAL_PAGES 22

#define SSD1854_MEM_SIZE (SSD1854_TOTAL_COLUMNS * SSD1854_COLUMN_SIZE * SSD1854_TOTAL_PAGES)

enum class SSD1854State
{
    IDLE,
    SET_CONTRAST,
    SET_PAGE_OFFSET
};

struct SSD1854DrawInfo
{
    Memory<SSD1854_MEM_SIZE> vram = {};
    uint8_t page_offset = 0;
    uint8_t contrast = 20;
    bool power_save_mode = false;
};

class SSD1854 : public Peripheral
{
public:
    SSD1854();

    void Receive(uint8_t data) override;
    uint8_t Transmit() override;
    void SaveEmulatorState(std::ostream& stream) const;
    bool LoadEmulatorState(std::istream& stream);

    SSD1854DrawInfo draw_info = {};

private:
    void HandleCommand(uint8_t data);

    SSD1854State state = SSD1854State::IDLE;

    uint8_t column = 0;
    uint8_t offset = 0;
    uint8_t page = 0;

    bool is_data_mode = false;
};
