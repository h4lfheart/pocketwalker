#pragma once
#include <array>
#include <memory>

#include "core/memory/memory.h"
#include "core/soc/ssu/peripheral.h"
#include "sample_provider.h"

#define BMA150_PIN_INT 4

#define BMA150_ADDR_CHIP_ID 0x00
#define BMA150_ADDR_ACC_X_LSB 0x02
#define BMA150_ADDR_ACC_X_MSB 0x03
#define BMA150_ADDR_ACC_Y_LSB 0x04
#define BMA150_ADDR_ACC_Y_MSB 0x05
#define BMA150_ADDR_ACC_Z_LSB 0x06
#define BMA150_ADDR_ACC_Z_MSB 0x07
#define BMA150_ADDR_CONTROL_1 0x0A
#define BMA150_ADDR_RANGE_BW_REG 0x14

#define BMA150_CHIP_ID 0x2

#define BMA150_CONTROL_SLEEP 1

#define BMA150_ACC_NEW_DATA 1

static constexpr std::array<uint16_t, 8> BMA150_CLOCK_RATES = {
    50, 100, 190, 375, 750, 1500, 3000, 3000
};

enum class BMA150State
{
    IDLE,
    MEMORY
};

class BMA150 : public Peripheral
{
public:
    explicit BMA150();

    void Receive(uint8_t data) override;
    uint8_t Transmit() override;
    void Reset() override;
    void Cycle(uint32_t cycles) override;

    void SetSampleProvider(const std::shared_ptr<SampleProvider>& provider)
    {
        sample_provider = provider;
    }

    h8300h_ptr<uint8_t> control1 = nullptr;

private:
    void SetAxisRegisters(int16_t x, int16_t y, int16_t z);

    Memory<0x80> mem = {};

    BMA150State state = BMA150State::IDLE;
    bool is_reading = false;
    uint8_t register_index = 0;
    uint32_t cycle_count = 0;
    uint8_t offset = 0;

    std::shared_ptr<SampleProvider> sample_provider = nullptr;
};
