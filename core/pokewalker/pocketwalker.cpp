#include "pocketwalker.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <thread>

#include "core/utils/logger.h"
#include "core/soc/defines.h"
#include "core/soc/memory/regions/io.h"
#include "core/soc/memory/regions/ram.h"

namespace
{
constexpr std::array<char, 8> STATE_MAGIC = {'P', 'W', 'S', 'T', 'A', 'T', '0', '5'};
constexpr uint32_t MAX_REASONABLE_STEPS = 9999999;
constexpr auto RTC_CATCH_UP_STEP_STABLE_DELAY = std::chrono::milliseconds(1200);
constexpr auto RTC_CATCH_UP_STEP_MAX_WAIT = std::chrono::seconds(8);
constexpr auto RTC_CATCH_UP_SETTLE_SAMPLE_INTERVAL = std::chrono::milliseconds(1);
constexpr auto RTC_CATCH_UP_SETTLE_STABLE_DELAY = std::chrono::milliseconds(3);
constexpr auto RTC_CATCH_UP_SETTLE_MAX_WAIT = std::chrono::milliseconds(25);
constexpr uint16_t RTC_CATCH_UP_TURBO_BATCH = 512;
constexpr uint16_t RTC_CATCH_UP_RAM_HASH_START = 0xF780;
constexpr uint16_t RTC_CATCH_UP_RAM_HASH_SIZE = 0x100;
constexpr auto PEER_COOLDOWN_NORMALIZE_DELAY = std::chrono::milliseconds(1500);
constexpr uint16_t EEPROM_MET_PEER_LIST_START = 0xDE24;
constexpr uint16_t EEPROM_MET_PEER_ENTRY_SIZE = 0x224;
constexpr uint16_t EEPROM_MET_PEER_COMPARE_OFFSET = 8;
constexpr uint16_t EEPROM_MET_PEER_COMPARE_SIZE = 0x28;
constexpr uint8_t EEPROM_MET_PEER_ENTRY_COUNT = 10;

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

void HashByte(uint64_t& hash, const uint8_t value)
{
    hash ^= value;
    hash *= 1099511628211ULL;
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
    bool prev_rtc_catch_up_active = false;
    const auto rtc_catch_up_step_wait_started = std::chrono::steady_clock::now();
    last_observed_total_days = soc->memory->Read16(PW_ADDR_TOTAL_DAYS);
    last_logged_total_steps = soc->memory->Read32(PW_ADDR_TOTAL_STEPS);
    last_logged_session_steps = soc->memory->Read32(PW_ADDR_SESSION_STEPS);
    last_logged_synthetic_steps = step_provider->is_enabled;
    last_logged_synthetic_sleep_probe = false;
    rtc_catch_up_was_active = soc->rtc->IsCatchUpActive();
    rtc_catch_up_settle_tracking = false;
    rtc_catch_up_settle_hash = 0;
    rtc_catch_up_settle_started_at = {};
    rtc_catch_up_settle_since = {};
    rtc_catch_up_next_settle_check = {};

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

        uint32_t elapsed_cycles = 0;
        const bool catch_up_turbo = soc->rtc->IsCatchUpActive();
        const uint16_t batch_size = catch_up_turbo ? RTC_CATCH_UP_TURBO_BATCH : 1;

        for (uint16_t i = 0; i < batch_size && this->is_running && !this->is_paused; i++)
        {
            const uint8_t cycles = soc->Cycle();
            elapsed_cycles += cycles;
            CyclePeripherals(cycles);
            CycleEnhancements(cycles);
            UpdateRtcCatchUpFirmwareSettle();
            UpdateDailyPeerCooldownNormalization();

            const uint32_t total_steps = soc->memory->Read32(PW_ADDR_TOTAL_STEPS);
            const uint32_t session_steps = soc->memory->Read32(PW_ADDR_SESSION_STEPS);
            const bool synthetic_steps = step_provider->is_enabled;
            if (total_steps != last_logged_total_steps ||
                session_steps != last_logged_session_steps ||
                synthetic_steps != last_logged_synthetic_steps)
            {
                soc->rtc->DebugMessage("step_debug synthetic=" + std::string(synthetic_steps ? "true" : "false") +
                                       " total_steps=" + std::to_string(total_steps) +
                                       " session_steps=" + std::to_string(session_steps) +
                                       " cpu_sleep=" + (soc->cpu->sleep ? std::string("true") : std::string("false")) +
                                       " ssu_clock_running=" + (soc->CKSTPR2.SSUCKSTP ? std::string("true") : std::string("false")) +
                                       " pdrb=" + std::to_string(soc->ssu->PDRB.VALUE));
                last_logged_total_steps = total_steps;
                last_logged_session_steps = session_steps;
                last_logged_synthetic_steps = synthetic_steps;
            }

            const bool synthetic_sleep_probe = synthetic_steps && soc->cpu->sleep;
            if (synthetic_sleep_probe && !last_logged_synthetic_sleep_probe)
            {
                soc->rtc->DebugMessage("synthetic_sleep_probe total_steps=" + std::to_string(total_steps) +
                                       " session_steps=" + std::to_string(session_steps) +
                                       " ienr1=" + std::to_string(soc->interrupts->IENR1.VALUE) +
                                       " irr1=" + std::to_string(soc->interrupts->IRR1.VALUE) +
                                       " pfcr=" + std::to_string(soc->ssu->PFCR.VALUE) +
                                       " pdrb=" + std::to_string(soc->ssu->PDRB.VALUE) +
                                       " bma_control1=" + std::to_string(*bma150->control1));
            }
            last_logged_synthetic_sleep_probe = synthetic_sleep_probe;

            if (catch_up_turbo && !soc->rtc->IsCatchUpActive())
                break;
        }

        if (rtc_catch_up_waiting_for_steps)
        {
            const auto now = std::chrono::steady_clock::now();
            const uint32_t total_steps = soc->memory->Read32(PW_ADDR_TOTAL_STEPS);
            const uint32_t session_steps = soc->memory->Read32(PW_ADDR_SESSION_STEPS);

            if (total_steps != rtc_catch_up_last_total_steps || session_steps != rtc_catch_up_last_session_steps)
            {
                rtc_catch_up_last_total_steps = total_steps;
                rtc_catch_up_last_session_steps = session_steps;
                rtc_catch_up_steps_stable_since = now;
            }

            if (now - rtc_catch_up_steps_stable_since >= RTC_CATCH_UP_STEP_STABLE_DELAY ||
                now - rtc_catch_up_step_wait_started >= RTC_CATCH_UP_STEP_MAX_WAIT)
            {
                rtc_catch_up_waiting_for_steps = false;
                soc->rtc->AllowCatchUpToRun(true);
            }
        }

        next += CYCLE_DURATION * elapsed_cycles;

        const bool is_rtc_catch_up_active = soc->rtc->IsCatchUpActive();
        if (!is_fast_mode && !is_rtc_catch_up_active)
        {
            if (prev_fast_mode || prev_rtc_catch_up_active)
                next = std::chrono::high_resolution_clock::now();
            std::this_thread::sleep_until(next);
        }
        else
        {
            next = std::chrono::high_resolution_clock::now();
        }

        prev_fast_mode = is_fast_mode;
        prev_rtc_catch_up_active = is_rtc_catch_up_active;
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
    const bool was_enabled = this->step_provider->is_enabled;
    this->step_provider->is_enabled = value;
    soc->rtc->DebugMessage("UseSyntheticSteps requested=" + std::string(value ? "true" : "false") +
                           " previous=" + (was_enabled ? std::string("true") : std::string("false")) +
                           " total_steps=" + std::to_string(soc->memory->Read32(PW_ADDR_TOTAL_STEPS)) +
                           " session_steps=" + std::to_string(soc->memory->Read32(PW_ADDR_SESSION_STEPS)) +
                           " cpu_sleep=" + (soc->cpu->sleep ? std::string("true") : std::string("false")) +
                           " ssu_clock_running=" + (soc->CKSTPR2.SSUCKSTP ? std::string("true") : std::string("false")) +
                           " ienr1=" + std::to_string(soc->interrupts->IENR1.VALUE) +
                           " irr1=" + std::to_string(soc->interrupts->IRR1.VALUE) +
                           " pfcr=" + std::to_string(soc->ssu->PFCR.VALUE) +
                           " bma_control1=" + std::to_string(*bma150->control1));
    (void)was_enabled;
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

void PocketWalker::LoadRtcState(std::istream& stream, const std::filesystem::path& base_directory) const
{
    soc->rtc->LoadState(stream, base_directory);
}

void PocketWalker::SaveRtcState(const std::string& path) const
{
    soc->rtc->SaveState(path);
}

void PocketWalker::SaveRtcState(std::ostream& stream, const std::filesystem::path& base_directory) const
{
    soc->rtc->SaveState(stream, base_directory);
}

bool PocketWalker::IsRtcCatchUpActive() const
{
    return soc->rtc->IsCatchUpActive();
}

size_t PocketWalker::RtcCatchUpMidnightsCompleted() const
{
    return soc->rtc->CatchUpMidnightsCompleted();
}

size_t PocketWalker::RtcCatchUpMidnightsTotal() const
{
    return soc->rtc->CatchUpMidnightsTotal();
}

void PocketWalker::ApplyRtcCatchUpOverflowDays() const
{
    const uint32_t overflow_days = soc->rtc->ConsumeCatchUpOverflowDays();
    if (overflow_days == 0)
        return;

    const uint16_t current_days = soc->memory->Read16(PW_ADDR_TOTAL_DAYS);
    const uint32_t patched_days = std::min<uint32_t>(current_days + overflow_days, 0xFFFF);
    soc->memory->Write16(PW_ADDR_TOTAL_DAYS, static_cast<uint16_t>(patched_days));
    soc->rtc->DebugMessage("overflow_total_days_patch old_days=" + std::to_string(current_days) +
                           " overflow_days=" + std::to_string(overflow_days) +
                           " new_days=" + std::to_string(patched_days));
    Log::Info("Patched total days for large RTC catch-up: {} + {} -> {}", current_days, overflow_days, patched_days);
}

void PocketWalker::ApplyPendingRtcSyncClock() const
{
    soc->rtc->ApplyPendingSyncClock();
}

void PocketWalker::ClearPendingRtcInterruptFlagsForCatchUp() const
{
    soc->rtc->ClearPendingInterruptFlagsForCatchUp();
}

void PocketWalker::PrepareRtcCatchUp()
{
    if (!soc->rtc->IsCatchUpActive())
    {
        soc->rtc->AllowCatchUpToRun(false);
        rtc_catch_up_waiting_for_steps = false;
        return;
    }

    soc->rtc->AllowCatchUpToRun(false);
    rtc_catch_up_waiting_for_steps = true;
    rtc_catch_up_last_total_steps = soc->memory->Read32(PW_ADDR_TOTAL_STEPS);
    rtc_catch_up_last_session_steps = soc->memory->Read32(PW_ADDR_SESSION_STEPS);
    rtc_catch_up_steps_stable_since = std::chrono::steady_clock::now();
}

bool PocketWalker::LoadEmulatorState(const std::string& path) const
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;

    return LoadEmulatorState(f);
}

bool PocketWalker::LoadEmulatorState(std::istream& stream) const
{
    std::array<char, STATE_MAGIC.size()> magic = {};
    stream.read(magic.data(), magic.size());
    if (!stream || magic != STATE_MAGIC)
        return false;

    if (!ReadCpuState(stream, soc->cpu))
        return false;

    stream.read(reinterpret_cast<char*>(m95512->eeprom.data()), m95512->eeprom.size());
    if (!stream)
        return false;

    if (!m95512->LoadEmulatorState(stream))
        return false;

    if (!bma150->LoadEmulatorState(stream))
        return false;

    if (!ReadMemoryRange(stream, soc->memory, RAM_START, RAM_SIZE))
        return false;

    if (!ReadMemoryRange(stream, soc->memory, IO_LOW_START, IO_LOW_SIZE))
        return false;

    if (!ReadMemoryRange(stream, soc->memory, IO_HIGH_START, IO_HIGH_SIZE))
        return false;

    if (!ReadValue(stream, soc->CKSTPR1.VALUE) || !ReadValue(stream, soc->CKSTPR2.VALUE))
        return false;

    if (!ReadValue(stream, soc->interrupts->IENR1.VALUE) || !ReadValue(stream, soc->interrupts->IENR2.VALUE) ||
        !ReadValue(stream, soc->interrupts->IRR1.VALUE) || !ReadValue(stream, soc->interrupts->IRR2.VALUE) ||
        !ReadValue(stream, soc->interrupts->RTCFLG.VALUE) || !ReadValue(stream, soc->interrupts->RTCCR2.VALUE) ||
        !ReadValue(stream, soc->interrupts->TIERW.VALUE) || !ReadValue(stream, soc->interrupts->TSRW.VALUE))
        return false;

    // RTC day/week flags in a save state are edge-triggered events, not durable
    // facts. The persisted RTC metadata below decides which missed midnights
    // should be replayed, so stale saved day/week flags must not run too.
    soc->interrupts->RTCFLG.DYIFG = false;
    soc->interrupts->RTCFLG.WKIFG = false;

    if (!ReadValue(stream, soc->ssu->PDRB.VALUE) || !ReadValue(stream, soc->ssu->PMRB.VALUE) ||
        !ReadValue(stream, soc->ssu->PFCR.VALUE) || !ReadValue(stream, soc->ssu->SSMR.VALUE) ||
        !ReadValue(stream, soc->ssu->SSER.VALUE) || !ReadValue(stream, soc->ssu->SSSR.VALUE) ||
        !ReadValue(stream, soc->ssu->SSRDR) || !ReadValue(stream, soc->ssu->SSTDR))
        return false;

    if (!ReadValue(stream, soc->sci3->SMR.VALUE) || !ReadValue(stream, soc->sci3->SSR.VALUE) ||
        !ReadValue(stream, soc->sci3->SCR.VALUE) || !ReadValue(stream, soc->sci3->IRCR.VALUE) ||
        !ReadValue(stream, soc->sci3->BRR) || !ReadValue(stream, soc->sci3->TDR) ||
        !ReadValue(stream, soc->sci3->RDR))
        return false;
    if (!soc->sci3->LoadEmulatorState(stream))
        return false;

    if (!ReadValue(stream, soc->timer_b1->TMB1.VALUE) || !ReadValue(stream, soc->timer_b1->TCB1) ||
        !ReadValue(stream, soc->timer_b1->TLB1))
        return false;
    if (!soc->timer_b1->LoadEmulatorState(stream))
        return false;

    if (!ReadValue(stream, soc->timer_w->TMRW.VALUE) || !ReadValue(stream, soc->timer_w->TCRW.VALUE))
        return false;
    if (!soc->timer_w->LoadEmulatorState(stream))
        return false;

    if (!ReadValue(stream, soc->rtc->RTCCR1.VALUE) || !ReadValue(stream, soc->rtc->RSECDR) ||
        !ReadValue(stream, soc->rtc->RMINDR) || !ReadValue(stream, soc->rtc->RHRDR) ||
        !ReadValue(stream, soc->rtc->RWKDR))
        return false;
    if (!soc->rtc->LoadEmulatorState(stream))
        return false;

    if (!ReadValue(stream, soc->adc->ADSR.VALUE) || !ReadValue(stream, soc->adc->AMR.VALUE))
        return false;

    if (!ReadDisplayState(stream, ssd1854))
        return false;
    if (!ssd1854->LoadEmulatorState(stream))
        return false;

    if (!soc->ssu->LoadEmulatorState(stream))
        return false;

    return true;
}

void PocketWalker::SaveEmulatorState(const std::string& path) const
{
    std::ofstream f(path, std::ios::binary);
    if (!f)
        return;

    SaveEmulatorState(f);
}

void PocketWalker::SaveEmulatorState(std::ostream& stream) const
{
    stream.write(STATE_MAGIC.data(), STATE_MAGIC.size());

    WriteCpuState(stream, soc->cpu);
    stream.write(reinterpret_cast<const char*>(m95512->eeprom.data()), m95512->eeprom.size());
    m95512->SaveEmulatorState(stream);
    bma150->SaveEmulatorState(stream);

    WriteMemoryRange(stream, soc->memory, RAM_START, RAM_SIZE);
    WriteMemoryRange(stream, soc->memory, IO_LOW_START, IO_LOW_SIZE);
    WriteMemoryRange(stream, soc->memory, IO_HIGH_START, IO_HIGH_SIZE);

    WriteValue(stream, soc->CKSTPR1.VALUE);
    WriteValue(stream, soc->CKSTPR2.VALUE);

    WriteValue(stream, soc->interrupts->IENR1.VALUE);
    WriteValue(stream, soc->interrupts->IENR2.VALUE);
    WriteValue(stream, soc->interrupts->IRR1.VALUE);
    WriteValue(stream, soc->interrupts->IRR2.VALUE);
    WriteValue(stream, soc->interrupts->RTCFLG.VALUE);
    WriteValue(stream, soc->interrupts->RTCCR2.VALUE);
    WriteValue(stream, soc->interrupts->TIERW.VALUE);
    WriteValue(stream, soc->interrupts->TSRW.VALUE);

    WriteValue(stream, soc->ssu->PDRB.VALUE);
    WriteValue(stream, soc->ssu->PMRB.VALUE);
    WriteValue(stream, soc->ssu->PFCR.VALUE);
    WriteValue(stream, soc->ssu->SSMR.VALUE);
    WriteValue(stream, soc->ssu->SSER.VALUE);
    WriteValue(stream, soc->ssu->SSSR.VALUE);
    WriteValue(stream, soc->ssu->SSRDR);
    WriteValue(stream, soc->ssu->SSTDR);

    WriteValue(stream, soc->sci3->SMR.VALUE);
    WriteValue(stream, soc->sci3->SSR.VALUE);
    WriteValue(stream, soc->sci3->SCR.VALUE);
    WriteValue(stream, soc->sci3->IRCR.VALUE);
    WriteValue(stream, soc->sci3->BRR);
    WriteValue(stream, soc->sci3->TDR);
    WriteValue(stream, soc->sci3->RDR);
    soc->sci3->SaveEmulatorState(stream);

    WriteValue(stream, soc->timer_b1->TMB1.VALUE);
    WriteValue(stream, soc->timer_b1->TCB1);
    WriteValue(stream, soc->timer_b1->TLB1);
    soc->timer_b1->SaveEmulatorState(stream);

    WriteValue(stream, soc->timer_w->TMRW.VALUE);
    WriteValue(stream, soc->timer_w->TCRW.VALUE);
    soc->timer_w->SaveEmulatorState(stream);

    WriteValue(stream, soc->rtc->RTCCR1.VALUE);
    WriteValue(stream, soc->rtc->RSECDR);
    WriteValue(stream, soc->rtc->RMINDR);
    WriteValue(stream, soc->rtc->RHRDR);
    WriteValue(stream, soc->rtc->RWKDR);
    soc->rtc->SaveEmulatorState(stream);

    WriteValue(stream, soc->adc->ADSR.VALUE);
    WriteValue(stream, soc->adc->AMR.VALUE);

    WriteDisplayState(stream, ssd1854);
    ssd1854->SaveEmulatorState(stream);
    soc->ssu->SaveEmulatorState(stream);
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

uint64_t PocketWalker::RtcCatchUpStateHash() const
{
    uint64_t hash = 14695981039346656037ULL;

    for (uint16_t offset = 0; offset < RTC_CATCH_UP_RAM_HASH_SIZE; offset++)
        HashByte(hash, soc->memory->Read8(RTC_CATCH_UP_RAM_HASH_START + offset));

    for (const uint8_t value : m95512->eeprom)
        HashByte(hash, value);

    return hash;
}

void PocketWalker::UpdateRtcCatchUpFirmwareSettle()
{
    if (!soc->rtc->IsCatchUpWaitingForFirmwareSettle())
    {
        rtc_catch_up_settle_tracking = false;
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (rtc_catch_up_next_settle_check != std::chrono::steady_clock::time_point{} &&
        now < rtc_catch_up_next_settle_check)
    {
        return;
    }

    rtc_catch_up_next_settle_check = now + RTC_CATCH_UP_SETTLE_SAMPLE_INTERVAL;
    const uint64_t current_hash = RtcCatchUpStateHash();

    if (!rtc_catch_up_settle_tracking || current_hash != rtc_catch_up_settle_hash)
    {
        if (!rtc_catch_up_settle_tracking)
            rtc_catch_up_settle_started_at = now;

        rtc_catch_up_settle_tracking = true;
        rtc_catch_up_settle_hash = current_hash;
        rtc_catch_up_settle_since = now;
        return;
    }

    if (now - rtc_catch_up_settle_since >= RTC_CATCH_UP_SETTLE_STABLE_DELAY ||
        now - rtc_catch_up_settle_started_at >= RTC_CATCH_UP_SETTLE_MAX_WAIT)
    {
        rtc_catch_up_settle_tracking = false;
        soc->rtc->MarkCatchUpFirmwareSettled();
    }
}

void PocketWalker::UpdateDailyPeerCooldownNormalization()
{
    const bool rtc_catch_up_active = soc->rtc->IsCatchUpActive();
    if (rtc_catch_up_was_active && !rtc_catch_up_active)
    {
        NormalizeClearedPeerCooldownSlots();
    }
    rtc_catch_up_was_active = rtc_catch_up_active;

    uint16_t total_days = soc->memory->Read16(PW_ADDR_TOTAL_DAYS);
    if (total_days != last_observed_total_days)
    {
        soc->rtc->DebugMessage("total_days_changed old_days=" + std::to_string(last_observed_total_days) +
                               " new_days=" + std::to_string(total_days) +
                               " catch_up_active=" + (rtc_catch_up_active ? std::string("true") : std::string("false")));
        last_observed_total_days = total_days;
        soc->rtc->MarkCurrentDayProcessed();
        soc->rtc->ClearPendingInterruptFlagsForCatchUp();
        peer_cooldown_normalize_pending = true;
        peer_cooldown_normalize_at = std::chrono::steady_clock::now() + PEER_COOLDOWN_NORMALIZE_DELAY;
    }

    if (peer_cooldown_normalize_pending && std::chrono::steady_clock::now() >= peer_cooldown_normalize_at)
    {
        peer_cooldown_normalize_pending = false;
        NormalizeClearedPeerCooldownSlots();
    }
}

void PocketWalker::NormalizeClearedPeerCooldownSlots() const
{
    uint8_t normalized_slots = 0;
    for (uint8_t slot = 0; slot < EEPROM_MET_PEER_ENTRY_COUNT; slot++)
    {
        const uint16_t compare_start =
            EEPROM_MET_PEER_LIST_START +
            static_cast<uint16_t>(slot) * EEPROM_MET_PEER_ENTRY_SIZE +
            EEPROM_MET_PEER_COMPARE_OFFSET;

        bool erased = true;
        for (uint16_t i = 0; i < EEPROM_MET_PEER_COMPARE_SIZE; i++)
        {
            if (m95512->eeprom[compare_start + i] != 0xFF)
            {
                erased = false;
                break;
            }
        }

        if (!erased)
            continue;

        std::fill_n(m95512->eeprom.begin() + compare_start, EEPROM_MET_PEER_COMPARE_SIZE, 0);
        normalized_slots++;
    }

    if (normalized_slots > 0)
        Log::Info("Normalized {} erased peer cooldown slot fingerprints", normalized_slots);
}
