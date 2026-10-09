#include "pocketwalker.h"

#include <chrono>
#include <thread>

#include "core/soc/defines.h"

PocketWalker::PocketWalker(RomBuffer rom_buffer)
{
    this->soc = std::make_shared<H838606>(rom_buffer);


    this->bma150 = std::make_shared<BMA150>();
    this->soc->ssu->RegisterPeripheral(this->bma150, SSU_ADDR_PDR9, 0);
    this->soc->ssu->RegisterOutputPin(this->bma150, BMA150_PIN_INT, SSU_ADDR_PDRB, 1);

    this->step_provider = std::make_shared<StepSampleProvider>();
    this->bma150->SetSampleProvider(this->step_provider);

    this->m95512 = std::make_shared<M95512>();
    this->soc->ssu->RegisterPeripheral(this->m95512, SSU_ADDR_PDR1, 2);

    this->nt7508 = std::make_shared<NT7508>();
    this->soc->ssu->RegisterPeripheral(this->nt7508, SSU_ADDR_PDR1, 0);
    this->soc->ssu->RegisterInputPin(this->nt7508, SSU_ADDR_PDR1, 1, NT7508_PIN_DC);

    this->buzzer = std::make_shared<Buzzer>(this->soc->timer_w);

    this->activity_timer_bypass = std::make_shared<ActivityTimerBypass>(this->soc->memory);
}

void PocketWalker::Start()
{
    constexpr std::chrono::duration<long long, std::nano> CYCLE_DURATION(1'000'000'000LL / PHI_CLK);

    auto next = std::chrono::high_resolution_clock::now();
    bool prev_fast_mode = is_fast_mode;
    bool prev_paused = is_paused;

    this->is_running = true;
    while (this->is_running)
    {
        if (this->is_paused)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            prev_paused = true;
            continue;
        }

        if (prev_paused)
            next = std::chrono::high_resolution_clock::now();

        prev_paused = false;

        const uint8_t cycles = soc->Cycle();
        CyclePeripherals(cycles);
        CycleEnhancements(cycles);

        next += CYCLE_DURATION * cycles;

        if (!is_fast_mode)
        {
            if (prev_fast_mode)
                next = std::chrono::high_resolution_clock::now();
            std::this_thread::sleep_until(next);
        }

        prev_fast_mode = is_fast_mode;
    }
}

void PocketWalker::Stop()
{
    this->is_running = false;
}

void PocketWalker::SetWatts(uint16_t value)
{
    this->soc->memory->Write16(PW_ADDR_WATTS, value);
}

void PocketWalker::SetSessionSteps(uint32_t value)
{
    this->soc->memory->Write32(PW_ADDR_SESSION_STEPS, value);
    this->soc->memory->Write32(PW_ADDR_TOTAL_STEPS, value);
}

void PocketWalker::UseSyntheticSteps(bool value)
{
    this->step_provider->SetEnabled(value);
}

void PocketWalker::SetStepPeriod(uint8_t period)
{
    this->step_provider->SetPeriod(period);
}

void PocketWalker::UseFastMode(bool value)
{
    this->is_fast_mode = value;
}

void PocketWalker::SetBypassPowerSave(bool value)
{
    this->bypass_power_save = value;
}

void PocketWalker::SetPause(bool value)
{
    this->is_paused = value;
}

void PocketWalker::OnSamplePushed(const EventHandlerCallback<BuzzerInformation>& callback)
{
    this->buzzer->OnSamplePushed += callback;
}

void PocketWalker::OnTransmitIR(const EventHandlerCallback<uint8_t>& callback)
{
    this->soc->sci3->OnTransmitIR(callback);
}

void PocketWalker::ReceiveIR(const uint8_t data)
{
    this->soc->sci3->ReceiveIR(data);
}

NT7508DrawInfo PocketWalker::GetDrawInfo() const
{
    return this->nt7508->GetDrawInfo();
}

void PocketWalker::PressButton(ButtonType button) const
{
    const uint8_t current = soc->memory->Read8(SSU_ADDR_PDRB);
    soc->memory->Write8(SSU_ADDR_PDRB, current | static_cast<uint8_t>(button));
}

void PocketWalker::ReleaseButton(ButtonType button) const
{
    const uint8_t current = soc->memory->Read8(SSU_ADDR_PDRB);
    soc->memory->Write8(SSU_ADDR_PDRB, current & ~static_cast<uint8_t>(button));
}

EepromBuffer PocketWalker::GetEepromBuffer() const
{
    return m95512->eeprom;
}

void PocketWalker::SetEepromBuffer(const EepromBuffer& buffer) const
{
    m95512->eeprom = buffer;
}

void PocketWalker::CyclePeripherals(uint8_t cycles) const
{
    this->buzzer->Cycle(cycles);
}

void PocketWalker::CycleEnhancements(uint8_t cycles) const
{
    if (bypass_power_save)
        this->activity_timer_bypass->Cycle(cycles);
}
