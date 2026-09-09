#include "sci3.h"

#include "core/soc/defines.h"
#include "core/utils/logger.h"

static std::array<uint32_t, 4> clock_rates = {
    PHI_CLK,
    PHI_CLK / PHI_W_CLK * 1,
    PHI_CLK / PHI_W_CLK * 16,
    PHI_CLK / PHI_W_CLK * 64

};

void SCI3::RegisterIOHandlers(const std::shared_ptr<IO>& io)
{
    IO_HANDLER_READ_UNION(SCI3_ADDR_SMR, SMR);
    IO_HANDLER_WRITE_UNION(SCI3_ADDR_SMR, SMR);

    IO_HANDLER_READ_UNION(SCI3_ADDR_SCR, SCR);
    IO_HANDLER_WRITE_UNION(SCI3_ADDR_SCR, SCR);

    IO_HANDLER_READ_UNION(SCI3_ADDR_IRCR, IRCR);
    IO_HANDLER_WRITE_UNION(SCI3_ADDR_IRCR, IRCR);

    IO_HANDLER_READ_VALUE(SCI3_ADDR_BRR, BRR);
    IO_HANDLER_WRITE_VALUE(SCI3_ADDR_BRR, BRR);

    io->RegisterReadHandler(SCI3_ADDR_SSR, [this]()
    {
        return SSR.VALUE;
    });

    io->RegisterWriteHandler(SCI3_ADDR_SSR, [this](const uint8_t value)
    {
        if (!SCR.TE)
            SSR.TDRE = true;
    });

    io->RegisterReadHandler(SCI3_ADDR_TDR, [this]
    {
        return TDR;
    });

    io->RegisterWriteHandler(SCI3_ADDR_TDR, [this](const uint8_t value)
    {
        TDR = value;
        SSR.TDRE = false;
        SSR.TEND = false;
    });

    io->RegisterReadHandler(SCI3_ADDR_RDR, [this]
    {
        SSR.RDRF = false;
        return RDR;
    });

    io->RegisterWriteHandler(SCI3_ADDR_RDR, [this](const uint8_t value)
    {
        // read only
    });
}

void SCI3::Cycle(uint8_t cycles)
{
    const uint16_t clock_rate = 32 * std::max(static_cast<uint8_t>(1), BRR);

    sci3_cycles += cycles;
    if (sci3_cycles >= clock_rate)
    {
        sci3_cycles -= clock_rate;

        if (SCR.TE)
        {
            if (!SSR.TDRE)
            {
                if (IRCR.IRE)
                {
                    on_transmit_ir(TDR);
                    SSR.TDRE = true;
                }
                else
                {
                    Log::Fatal("Unimplemented SCI3 TXD Transmit");
                }
            }
            else
            {
                SSR.TEND = true;
            }
        }

        if (SCR.RE)
        {
            if (!SSR.RDRF)
            {
                std::lock_guard lock(receive_mutex);
                if (!receive_queue.empty())
                {
                    if (IRCR.IRE)
                    {
                        RDR = receive_queue.front();
                        receive_queue.pop();
                        SSR.RDRF = true;
                    }
                    else
                    {
                        Log::Fatal("Unimplemented SCI3 RXD Receive");
                    }
                }
            }
        }
    }
}

void SCI3::OnTransmitIR(const EventHandlerCallback<uint8_t>& callback)
{
    on_transmit_ir += callback;
}

void SCI3::ReceiveIR(uint8_t data)
{
    std::lock_guard lock(receive_mutex);
    receive_queue.push(data);
}

void SCI3::SaveEmulatorState(std::ostream& stream)
{
    std::lock_guard lock(receive_mutex);
    stream.write(reinterpret_cast<const char*>(&sci3_cycles), sizeof(sci3_cycles));

    uint32_t queue_size = static_cast<uint32_t>(receive_queue.size());
    stream.write(reinterpret_cast<const char*>(&queue_size), sizeof(queue_size));

    auto queue_copy = receive_queue;
    while (!queue_copy.empty())
    {
        const uint8_t value = queue_copy.front();
        queue_copy.pop();
        stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }
}

bool SCI3::LoadEmulatorState(std::istream& stream)
{
    std::lock_guard lock(receive_mutex);
    stream.read(reinterpret_cast<char*>(&sci3_cycles), sizeof(sci3_cycles));
    if (!stream)
        return false;

    uint32_t queue_size = 0;
    stream.read(reinterpret_cast<char*>(&queue_size), sizeof(queue_size));
    if (!stream)
        return false;

    receive_queue = {};
    for (uint32_t i = 0; i < queue_size; i++)
    {
        uint8_t value = 0;
        stream.read(reinterpret_cast<char*>(&value), sizeof(value));
        if (!stream)
            return false;
        receive_queue.push(value);
    }

    return true;
}
