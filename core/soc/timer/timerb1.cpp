#include "timerb1.h"

#include "core/soc/defines.h"

static std::array<uint32_t, 8> clock_rates = {
    8192,
    2048,
    256,
    64,
    16,
    4,
    PHI_CLK / PHI_W_CLK * 1024,
    PHI_CLK / PHI_W_CLK * 256
};

TimerB1::TimerB1(const std::shared_ptr<Interrupts>& interrupts)
{
    this->interrupts = interrupts;
}

void TimerB1::RegisterIOHandlers(const std::shared_ptr<IO>& io)
{
    IO_HANDLER_READ_UNION(TIMER_B1_ADDR_TMB1, TMB1);
    IO_HANDLER_WRITE_UNION(TIMER_B1_ADDR_TMB1, TMB1);

    IO_HANDLER_READ_VALUE(TIMER_B1_ADDR_TCB1_TLB1, TCB1);
    IO_HANDLER_WRITE_VALUE(TIMER_B1_ADDR_TCB1_TLB1, TLB1);
}

void TimerB1::Cycle(uint8_t cycles)
{
    if (!TMB1.CNT)
        return;

    timer_b1_cycles += cycles;
    if (timer_b1_cycles >= clock_rates[TMB1.CKS])
    {
        timer_b1_cycles -= clock_rates[TMB1.CKS];

        if (TCB1 == 0xFF)
        {
            TCB1 = TLB1;
            interrupts->IRR2.IRRTB1 = true;
        }
        else
        {
            TCB1++;
        }
    }
}

void TimerB1::SaveEmulatorState(std::ostream& stream) const
{
    stream.write(reinterpret_cast<const char*>(&timer_b1_cycles), sizeof(timer_b1_cycles));
}

bool TimerB1::LoadEmulatorState(std::istream& stream)
{
    stream.read(reinterpret_cast<char*>(&timer_b1_cycles), sizeof(timer_b1_cycles));
    return static_cast<bool>(stream);
}
