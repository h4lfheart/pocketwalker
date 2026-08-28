#pragma once
#include <istream>
#include <ostream>

#include "core/memory/interface.h"
#include "core/soc/interrupts/interrupts.h"
#include "core/utils/h8300h_ptr.h"

#define TIMER_W_ADDR_TMRW 0xF0F0
#define TIMER_W_ADDR_TCRW 0xF0F1
#define TIMER_W_ADDR_TCNT 0xF0F6

#define TIMER_W_ADDR_GRA 0xF0F8
#define TIMER_W_ADDR_GRB 0xF0FA
#define TIMER_W_ADDR_GRC 0xF0FC
#define TIMER_W_ADDR_GRD 0xF0FE

union TMRW_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t PWMB : 1;
        uint8_t PWMC : 1;
        uint8_t PWMD : 1;
        uint8_t : 1;
        uint8_t BUFEA : 1;
        uint8_t BUFEB : 1;
        uint8_t : 1;
        uint8_t CTS : 1;
    };
};


union TCRW_t
{
    uint8_t VALUE;

    struct
    {
        uint8_t TOA : 1;
        uint8_t TOB : 1;
        uint8_t TOC : 1;
        uint8_t TOD : 1;
        uint8_t CKS : 3;
        uint8_t CCLR : 1;
    };
};

class TimerW
{

public:
    explicit TimerW(const std::shared_ptr<MemoryInterface>& memory, const std::shared_ptr<Interrupts>& interrupts);
    void RegisterIOHandlers(const std::shared_ptr<IO>& io);

    void Cycle(uint8_t cycles);
    void SaveEmulatorState(std::ostream& stream) const;
    bool LoadEmulatorState(std::istream& stream);

    TMRW_t TMRW = {};
    TCRW_t TCRW = {};

    h8300h_ptr<uint16_t> TCNT = nullptr;

    h8300h_ptr<uint16_t> GRA = nullptr;
    h8300h_ptr<uint16_t> GRB = nullptr;
    h8300h_ptr<uint16_t> GRC = nullptr;
    h8300h_ptr<uint16_t> GRD = nullptr;

private:
    std::shared_ptr<Interrupts> interrupts;

    uint32_t timer_w_cycles = 0;
};
