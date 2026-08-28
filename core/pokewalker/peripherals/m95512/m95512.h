#pragma once
#include <array>
#include <istream>
#include <ostream>

#include "core/soc/ssu/peripheral.h"

#define EEPROM_SIZE 0x10000

#define M95512_CMD_WRITE_MEMORY 0x2
#define M95512_CMD_READ_MEMORY 0x3
#define M95512_CMD_READ_STATUS 0x5
#define M95512_CMD_WRITE_ENABLE 0x6

#define M95512_STATUS_WRITE_UNLOCK 0b00000010

using EepromBuffer = std::array<uint8_t, EEPROM_SIZE>;

enum class M95512State
{
    IDLE,
    ADDR_HIGH,
    ADDR_LOW,
    MEMORY,
    STATUS
};

class M95512 : public Peripheral
{

public:
    void Receive(uint8_t data) override;
    uint8_t Transmit() override;
    void Reset() override;
    void SaveEmulatorState(std::ostream& stream) const;
    bool LoadEmulatorState(std::istream& stream);

    EepromBuffer eeprom = {};

private:

    M95512State state = M95512State::IDLE;
    bool is_reading = false;
    uint8_t status = 0;
    uint8_t high_addr = 0;
    uint8_t low_addr = 0;
    uint16_t offset = 0;
};

