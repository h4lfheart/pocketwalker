#include "ssu.h"

#include <array>
#include <print>

#include "core/soc/defines.h"

static std::array<uint16_t, 8> clock_rates = {
    256,
    128,
    64,
    32,
    16,
    8,
    4,
    2
};

SSU::SSU(const std::shared_ptr<MemoryInterface>& memory, const std::shared_ptr<Interrupts>& interrupts)
{
    this->mem = memory;
    this->interrupts = interrupts;
}

void SSU::RegisterIOHandlers(const std::shared_ptr<IO>& io)
{
    io->RegisterReadHandler(SSU_ADDR_SSTDR, [this]
    {
        return SSTDR;
    });

    io->RegisterWriteHandler(SSU_ADDR_SSTDR, [this](const uint8_t value)
    {
        SSTDR = value;
        SSSR.TDRE = false;
        SSSR.TEND = false;
    });

    io->RegisterReadHandler(SSU_ADDR_SSRDR, [this]
    {
        SSSR.RDRF = false;
        return SSRDR;
    });

    io->RegisterWriteHandler(SSU_ADDR_SSRDR, [this](const uint8_t value)
    {
        // read only
    });

    io->RegisterReadHandler(SSU_ADDR_SSSR, [this]()
    {
        return SSSR.VALUE;
    });

    io->RegisterWriteHandler(SSU_ADDR_SSSR, [this](const uint8_t value)
    {
        SSSR.VALUE &= value;

        if (!SSER.TE)
            SSSR.TDRE = true;
    });

    IO_HANDLER_READ_UNION(SSU_ADDR_SSMR, SSMR);
    IO_HANDLER_WRITE_UNION(SSU_ADDR_SSMR, SSMR);

    IO_HANDLER_READ_UNION(SSU_ADDR_SSER, SSER);
    IO_HANDLER_WRITE_UNION(SSU_ADDR_SSER, SSER);

    io->RegisterReadHandler(SSU_ADDR_PDRB, [this]
    {
        return PDRB.VALUE;
    });

    io->RegisterWriteHandler(SSU_ADDR_PDRB, [this](const uint8_t value)
    {
        PDRB.VALUE = value;

        if (PFCR.IRQ0S == 0b00 && PDRB.PB0)
            interrupts->IRR1.IRRI0 = true;

        if (PFCR.IRQ1S == 0b00 && PDRB.PB1)
            interrupts->IRR1.IRRI1 = true;
    });

    IO_HANDLER_READ_UNION(SSU_ADDR_PMRB, PMRB);
    IO_HANDLER_WRITE_UNION(SSU_ADDR_PMRB, PMRB);

    IO_HANDLER_READ_UNION(SSU_ADDR_PFCR, PFCR);
    IO_HANDLER_WRITE_UNION(SSU_ADDR_PFCR, PFCR);
}

void SSU::RegisterPeripheral(
    const std::shared_ptr<Peripheral>& peripheral,
    uint16_t cs_port_addr,
    uint8_t cs_pin_index)
{
    peripherals.push_back({peripheral, cs_port_addr, cs_pin_index});
}

void SSU::RegisterInputPin(
    const std::shared_ptr<Peripheral>& peripheral,
    uint16_t port_addr,
    uint8_t pin_index,
    uint8_t peripheral_pin)
{
    input_pins.push_back({peripheral, port_addr, pin_index, peripheral_pin});
}

void SSU::RegisterOutputPin(
    const std::shared_ptr<Peripheral>& peripheral,
    uint8_t peripheral_pin,
    uint16_t port_addr,
    uint8_t pin_index)
{
    peripheral->OnOutputPin += [this, peripheral_pin, port_addr, pin_index](PinEvent event)
    {
        if (event.pin != peripheral_pin)
            return;

        uint8_t port = mem->Read8(port_addr);
        if (event.value)
            port |= (1 << pin_index);
        else
            port &= ~(1 << pin_index);
        mem->Write8(port_addr, port);
    };
}

void SSU::Cycle(uint8_t cycles)
{
    for (const auto& entry : peripherals)
        entry.peripheral->Cycle(cycles);

    ssu_cycles += cycles;
    if (ssu_cycles < clock_rates[SSMR.CKS])
        return;

    ssu_cycles -= clock_rates[SSMR.CKS];

    const auto peripheral = ActivePeripheral();
    if (peripheral == nullptr)
        return;

    if (SSER.TE)
    {
        if (!SSSR.TDRE)
        {
            peripheral->Receive(SSTDR);
            SSSR.TDRE = true;
            SSSR.TEND = false;
        }
        else
        {
            SSSR.TEND = true;
        }
    }

    if (SSER.RE)
    {
        if (!SSSR.RDRF)
        {
            SSRDR = peripheral->Transmit();
            SSSR.RDRF = true;
        }
    }
}

std::shared_ptr<Peripheral> SSU::ActivePeripheral()
{
    for (auto& [peripheral, port_addr, pin_index] : peripherals)
    {
        const uint8_t port = mem->Read8(port_addr);
        if ((port & (1 << pin_index)) == 0) // active low
        {
            for (auto& pin : input_pins)
            {
                if (pin.peripheral != peripheral)
                    continue;
                const uint8_t pin_port = mem->Read8(pin.port_addr);
                const bool value = (pin_port >> pin.pin_index) & 1;
                peripheral->OnInputPin({pin.peripheral_pin, value});
            }

            return peripheral;
        }
        else
        {
            peripheral->Reset();
        }
    }

    return nullptr;
}

void SSU::SaveEmulatorState(std::ostream& stream) const
{
    stream.write(reinterpret_cast<const char*>(&ssu_cycles), sizeof(ssu_cycles));
}

bool SSU::LoadEmulatorState(std::istream& stream)
{
    stream.read(reinterpret_cast<char*>(&ssu_cycles), sizeof(ssu_cycles));
    return static_cast<bool>(stream);
}
