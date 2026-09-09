#include "timerw.h"

#include "core/soc/defines.h"

static std::array<uint32_t, 8> clock_rates = {
    1,
    2,
    4,
    8,
    PHI_CLK / PHI_W_CLK,
    PHI_CLK / PHI_W_CLK * 4,
    PHI_CLK / PHI_W_CLK * 16,
    0 // external event clock, ignore
};

TimerW::TimerW(const std::shared_ptr<MemoryInterface>& memory, const std::shared_ptr<Interrupts>& interrupts)
{
    this->interrupts = interrupts;

    TCNT = memory->Ptr16(TIMER_W_ADDR_TCNT);
    GRA = memory->Ptr16(TIMER_W_ADDR_GRA);
    GRB = memory->Ptr16(TIMER_W_ADDR_GRB);
    GRC = memory->Ptr16(TIMER_W_ADDR_GRC);
    GRD = memory->Ptr16(TIMER_W_ADDR_GRD);
}

void TimerW::RegisterIOHandlers(const std::shared_ptr<IO>& io)
{
    IO_HANDLER_READ_UNION(TIMER_W_ADDR_TMRW, TMRW);
    IO_HANDLER_WRITE_UNION(TIMER_W_ADDR_TMRW, TMRW);

    IO_HANDLER_READ_UNION(TIMER_W_ADDR_TCRW, TCRW);
    IO_HANDLER_WRITE_UNION(TIMER_W_ADDR_TCRW, TCRW);
}

void TimerW::Cycle(uint8_t cycles)
{
    if (!TMRW.CTS)
        return;

    timer_w_cycles += cycles;
    if (timer_w_cycles >= clock_rates[TCRW.CKS])
    {
        timer_w_cycles -= clock_rates[TCRW.CKS];

        if (*TCNT == 0xFFFF)
        {
            *TCNT = 0;
            interrupts->TSRW.OVF = true;
        }
        else
        {
            (*TCNT)++;
        }

        if (*TCNT >= *GRA)
        {
            if (TCRW.CCLR)
                *TCNT = 0;

            interrupts->TSRW.IMFA = true;
        }
    }
}

void TimerW::SaveEmulatorState(std::ostream& stream) const
{
    stream.write(reinterpret_cast<const char*>(&timer_w_cycles), sizeof(timer_w_cycles));
}

bool TimerW::LoadEmulatorState(std::istream& stream)
{
    stream.read(reinterpret_cast<char*>(&timer_w_cycles), sizeof(timer_w_cycles));
    return static_cast<bool>(stream);
}
