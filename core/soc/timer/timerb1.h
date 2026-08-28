#pragma once
#include <istream>
#include <memory>
#include <ostream>

#include "core/memory/interface.h"
#include "core/soc/interrupts/interrupts.h"
#include "core/soc/memory/regions/io.h"

#define TIMER_B1_ADDR_TMB1 0xF0D0
#define TIMER_B1_ADDR_TCB1_TLB1 0xF0D1

union TMB1_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t CKS : 3;
        uint8_t : 3;
        uint8_t CNT : 1;
        uint8_t RLD : 1;
    };
};

class TimerB1
{
public:
    explicit TimerB1(const std::shared_ptr<Interrupts>& interrupts);
    void RegisterIOHandlers(const std::shared_ptr<IO>& io);

    void Cycle(uint8_t cycles);
    void SaveEmulatorState(std::ostream& stream) const;
    bool LoadEmulatorState(std::istream& stream);

    TMB1_t TMB1 = {};
    uint8_t TCB1 = 0;
    uint8_t TLB1 = 0;

private:
    std::shared_ptr<Interrupts> interrupts;

    uint32_t timer_b1_cycles = 0;
};
