#include "pocketwalker.h"

#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <thread>

#include "core/soc/defines.h"
#include "core/soc/memory/regions/io.h"
#include "core/soc/memory/regions/ram.h"

namespace
{
constexpr std::array<char, 8> STATE_MAGIC = {'P', 'W', 'S', 'T', 'A', 'T', '0', '4'};
constexpr uint32_t MAX_REASONABLE_STEPS = 9999999;

template <typename T>
void WriteValue(std::ostream& stream, const T& value)
{
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool ReadValue(std::istream& stream, T& value)
{
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(stream);
}

void WriteMemoryRange(std::ostream& stream, const std::shared_ptr<MemoryInterface>& memory, const uint16_t start, const size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        auto ptr = memory->Ptr8(start + static_cast<uint16_t>(i));
        const uint8_t value = ptr.ptr ? *ptr : 0xFF;
        WriteValue(stream, value);
    }
}

bool ReadMemoryRange(std::istream& stream, const std::shared_ptr<MemoryInterface>& memory, const uint16_t start, const size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        uint8_t value = 0;
        if (!ReadValue(stream, value))
            return false;

        auto ptr = memory->Ptr8(start + static_cast<uint16_t>(i));
        if (ptr.ptr)
            ptr = value;
    }

    return true;
}

void WriteCpuState(std::ostream& stream, const std::shared_ptr<CPU>& cpu)
{
    WriteValue(stream, cpu->reg.PC);
    WriteValue(stream, cpu->reg.flags.CCR);
    for (uint8_t i = 0; i < 8; i++)
        WriteValue(stream, *cpu->reg.Reg32(i));
    WriteValue(stream, cpu->sleep);
}

bool ReadCpuState(std::istream& stream, const std::shared_ptr<CPU>& cpu)
{
    if (!ReadValue(stream, cpu->reg.PC))
        return false;

    if (!ReadValue(stream, cpu->reg.flags.CCR))
        return false;

    for (uint8_t i = 0; i < 8; i++)
    {
        uint32_t value = 0;
        if (!ReadValue(stream, value))
            return false;
        *cpu->reg.Reg32(i) = value;
    }

    return ReadValue(stream, cpu->sleep);
}

void WriteDisplayState(std::ostream& stream, const std::shared_ptr<SSD1854>& display)
{
    for (uint16_t i = 0; i < SSD1854_MEM_SIZE; i++)
        WriteValue(stream, display->draw_info.vram.Read8(i));

    WriteValue(stream, display->draw_info.page_offset);
    WriteValue(stream, display->draw_info.contrast);
    WriteValue(stream, display->draw_info.power_save_mode);
}

bool ReadDisplayState(std::istream& stream, const std::shared_ptr<SSD1854>& display)
{
    for (uint16_t i = 0; i < SSD1854_MEM_SIZE; i++)
    {
        uint8_t value = 0;
        if (!ReadValue(stream, value))
            return false;
        display->draw_info.vram.Write8(i, value);
    }

    return ReadValue(stream, display->draw_info.page_offset) &&
           ReadValue(stream, display->draw_info.contrast) &&
           ReadValue(stream, display->draw_info.power_save_mode);
}
}

PocketWalker::PocketWalker(RomBuffer rom_buffer)
{
    this->soc = std::make_shared<H838606>(rom_buffer);


    this->bma150 = std::make_shared<BMA150>();
    this->soc->ssu->RegisterPeripheral(this->bma150, SSU_ADDR_PDR9, 0);
    this->soc->ssu->RegisterOutputPin(this->bma150, BMA150_PIN_INT, SSU_ADDR_PDRB, 1);

    this->step_provider = std::make_shared<StepSampleProvider>(this->soc->memory);
    this->bma150->SetSampleProvider(this->step_provider);

    this->m95512 = std::make_shared<M95512>();
    this->soc->ssu->RegisterPeripheral(this->m95512, SSU_ADDR_PDR1, 2);

    this->ssd1854 = std::make_shared<SSD1854>();
    this->soc->ssu->RegisterPeripheral(this->ssd1854, SSU_ADDR_PDR1, 0);
    this->soc->ssu->RegisterInputPin(this->ssd1854, SSU_ADDR_PDR1, 1, SSD1854_PIN_DC);

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
    this->step_provider->is_enabled = value;
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

SSD1854DrawInfo* PocketWalker::GetDrawInfo()
{
    return &this->ssd1854->draw_info;
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

uint32_t PocketWalker::GetVolatileStepCount() const
{
    const uint32_t session_steps = this->soc->memory->Read32(PW_ADDR_SESSION_STEPS);
    const uint32_t total_steps = this->soc->memory->Read32(PW_ADDR_TOTAL_STEPS);

    if (session_steps > 0 && session_steps <= MAX_REASONABLE_STEPS)
        return session_steps;

    if (total_steps > 0 && total_steps <= MAX_REASONABLE_STEPS)
        return total_steps;

    return 0;
}

uint16_t PocketWalker::GetVolatileWatts() const
{
    return this->soc->memory->Read16(PW_ADDR_WATTS);
}

void PocketWalker::RestoreVolatileCounters(uint32_t steps, uint16_t watts) const
{
    if (steps > 0 && steps <= MAX_REASONABLE_STEPS)
    {
        this->soc->memory->Write32(PW_ADDR_SESSION_STEPS, steps);
        this->soc->memory->Write32(PW_ADDR_TOTAL_STEPS, steps);
    }

    if (watts > 0)
        this->soc->memory->Write16(PW_ADDR_WATTS, watts);
}

void PocketWalker::LoadRtcState(const std::string& path) const
{
    soc->rtc->LoadState(path);
}

void PocketWalker::SaveRtcState(const std::string& path) const
{
    soc->rtc->SaveState(path);
}

bool PocketWalker::LoadEmulatorState(const std::string& path) const
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;

    std::array<char, STATE_MAGIC.size()> magic = {};
    f.read(magic.data(), magic.size());
    if (!f || magic != STATE_MAGIC)
        return false;

    if (!ReadCpuState(f, soc->cpu))
        return false;

    f.read(reinterpret_cast<char*>(m95512->eeprom.data()), m95512->eeprom.size());
    if (!f)
        return false;

    if (!m95512->LoadEmulatorState(f))
        return false;

    if (!bma150->LoadEmulatorState(f))
        return false;

    if (!ReadMemoryRange(f, soc->memory, RAM_START, RAM_SIZE))
        return false;

    if (!ReadMemoryRange(f, soc->memory, IO_LOW_START, IO_LOW_SIZE))
        return false;

    if (!ReadMemoryRange(f, soc->memory, IO_HIGH_START, IO_HIGH_SIZE))
        return false;

    if (!ReadValue(f, soc->CKSTPR1.VALUE) || !ReadValue(f, soc->CKSTPR2.VALUE))
        return false;

    if (!ReadValue(f, soc->interrupts->IENR1.VALUE) || !ReadValue(f, soc->interrupts->IENR2.VALUE) ||
        !ReadValue(f, soc->interrupts->IRR1.VALUE) || !ReadValue(f, soc->interrupts->IRR2.VALUE) ||
        !ReadValue(f, soc->interrupts->RTCFLG.VALUE) || !ReadValue(f, soc->interrupts->RTCCR2.VALUE) ||
        !ReadValue(f, soc->interrupts->TIERW.VALUE) || !ReadValue(f, soc->interrupts->TSRW.VALUE))
        return false;

    if (!ReadValue(f, soc->ssu->PDRB.VALUE) || !ReadValue(f, soc->ssu->PMRB.VALUE) ||
        !ReadValue(f, soc->ssu->PFCR.VALUE) || !ReadValue(f, soc->ssu->SSMR.VALUE) ||
        !ReadValue(f, soc->ssu->SSER.VALUE) || !ReadValue(f, soc->ssu->SSSR.VALUE) ||
        !ReadValue(f, soc->ssu->SSRDR) || !ReadValue(f, soc->ssu->SSTDR))
        return false;

    if (!ReadValue(f, soc->sci3->SMR.VALUE) || !ReadValue(f, soc->sci3->SSR.VALUE) ||
        !ReadValue(f, soc->sci3->SCR.VALUE) || !ReadValue(f, soc->sci3->IRCR.VALUE) ||
        !ReadValue(f, soc->sci3->BRR) || !ReadValue(f, soc->sci3->TDR) ||
        !ReadValue(f, soc->sci3->RDR))
        return false;
    if (!soc->sci3->LoadEmulatorState(f))
        return false;

    if (!ReadValue(f, soc->timer_b1->TMB1.VALUE) || !ReadValue(f, soc->timer_b1->TCB1) ||
        !ReadValue(f, soc->timer_b1->TLB1))
        return false;
    if (!soc->timer_b1->LoadEmulatorState(f))
        return false;

    if (!ReadValue(f, soc->timer_w->TMRW.VALUE) || !ReadValue(f, soc->timer_w->TCRW.VALUE))
        return false;
    if (!soc->timer_w->LoadEmulatorState(f))
        return false;

    if (!ReadValue(f, soc->rtc->RTCCR1.VALUE) || !ReadValue(f, soc->rtc->RSECDR) ||
        !ReadValue(f, soc->rtc->RMINDR) || !ReadValue(f, soc->rtc->RHRDR) ||
        !ReadValue(f, soc->rtc->RWKDR))
        return false;

    if (!ReadValue(f, soc->adc->ADSR.VALUE) || !ReadValue(f, soc->adc->AMR.VALUE))
        return false;

    if (!ReadDisplayState(f, ssd1854))
        return false;
    if (!ssd1854->LoadEmulatorState(f))
        return false;

    if (!soc->ssu->LoadEmulatorState(f))
        return false;

    const uint32_t total_steps = soc->memory->Read32(PW_ADDR_TOTAL_STEPS);
    const uint32_t session_steps = soc->memory->Read32(PW_ADDR_SESSION_STEPS);
    if (session_steps == 0 && total_steps > 0 && total_steps <= MAX_REASONABLE_STEPS)
        soc->memory->Write32(PW_ADDR_SESSION_STEPS, total_steps);

    return true;
}

void PocketWalker::SaveEmulatorState(const std::string& path) const
{
    std::ofstream f(path, std::ios::binary);
    if (!f)
        return;

    f.write(STATE_MAGIC.data(), STATE_MAGIC.size());

    WriteCpuState(f, soc->cpu);
    f.write(reinterpret_cast<const char*>(m95512->eeprom.data()), m95512->eeprom.size());
    m95512->SaveEmulatorState(f);
    bma150->SaveEmulatorState(f);

    WriteMemoryRange(f, soc->memory, RAM_START, RAM_SIZE);
    WriteMemoryRange(f, soc->memory, IO_LOW_START, IO_LOW_SIZE);
    WriteMemoryRange(f, soc->memory, IO_HIGH_START, IO_HIGH_SIZE);

    WriteValue(f, soc->CKSTPR1.VALUE);
    WriteValue(f, soc->CKSTPR2.VALUE);

    WriteValue(f, soc->interrupts->IENR1.VALUE);
    WriteValue(f, soc->interrupts->IENR2.VALUE);
    WriteValue(f, soc->interrupts->IRR1.VALUE);
    WriteValue(f, soc->interrupts->IRR2.VALUE);
    WriteValue(f, soc->interrupts->RTCFLG.VALUE);
    WriteValue(f, soc->interrupts->RTCCR2.VALUE);
    WriteValue(f, soc->interrupts->TIERW.VALUE);
    WriteValue(f, soc->interrupts->TSRW.VALUE);

    WriteValue(f, soc->ssu->PDRB.VALUE);
    WriteValue(f, soc->ssu->PMRB.VALUE);
    WriteValue(f, soc->ssu->PFCR.VALUE);
    WriteValue(f, soc->ssu->SSMR.VALUE);
    WriteValue(f, soc->ssu->SSER.VALUE);
    WriteValue(f, soc->ssu->SSSR.VALUE);
    WriteValue(f, soc->ssu->SSRDR);
    WriteValue(f, soc->ssu->SSTDR);

    WriteValue(f, soc->sci3->SMR.VALUE);
    WriteValue(f, soc->sci3->SSR.VALUE);
    WriteValue(f, soc->sci3->SCR.VALUE);
    WriteValue(f, soc->sci3->IRCR.VALUE);
    WriteValue(f, soc->sci3->BRR);
    WriteValue(f, soc->sci3->TDR);
    WriteValue(f, soc->sci3->RDR);
    soc->sci3->SaveEmulatorState(f);

    WriteValue(f, soc->timer_b1->TMB1.VALUE);
    WriteValue(f, soc->timer_b1->TCB1);
    WriteValue(f, soc->timer_b1->TLB1);
    soc->timer_b1->SaveEmulatorState(f);

    WriteValue(f, soc->timer_w->TMRW.VALUE);
    WriteValue(f, soc->timer_w->TCRW.VALUE);
    soc->timer_w->SaveEmulatorState(f);

    WriteValue(f, soc->rtc->RTCCR1.VALUE);
    WriteValue(f, soc->rtc->RSECDR);
    WriteValue(f, soc->rtc->RMINDR);
    WriteValue(f, soc->rtc->RHRDR);
    WriteValue(f, soc->rtc->RWKDR);

    WriteValue(f, soc->adc->ADSR.VALUE);
    WriteValue(f, soc->adc->AMR.VALUE);

    WriteDisplayState(f, ssd1854);
    ssd1854->SaveEmulatorState(f);
    soc->ssu->SaveEmulatorState(f);
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
