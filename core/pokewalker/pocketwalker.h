#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

#include "core/soc/h838606.h"
#include "core/utils/event_handler.h"
#include "enhancements/activity_timer_bypass.h"
#include "peripherals/bma150/bma150.h"
#include "peripherals/bma150/step_sample_provider.h"
#include "peripherals/buzzer/buzzer.h"
#include "peripherals/m95512/m95512.h"
#include "peripherals/nt7508/nt7508.h"

#define PW_ADDR_WATTS 0xF78E
#define PW_ADDR_SESSION_STEPS 0xF79C
#define PW_ADDR_TOTAL_STEPS 0xF780
#define PW_ADDR_ACTIVITY_TIMER 0xF7AF

enum class ButtonType
{
    CENTER = 1 << 0,
    LEFT = 1 << 2,
    RIGHT = 1 << 4
};

class PocketWalker
{
public:
    explicit PocketWalker(RomBuffer rom_buffer);

    void Start();
    void Stop();

    void UseSyntheticSteps(bool value);
    void SetStepPeriod(uint8_t period);
    void UseFastMode(bool value);
    void SetBypassPowerSave(bool value);
    void SetWatts(uint16_t value);
    void SetSessionSteps(uint32_t value);
    void SetPause(bool value);

    void OnSamplePushed(const EventHandlerCallback<BuzzerInformation>& callback);

    void OnTransmitIR(const EventHandlerCallback<uint8_t>& callback);
    void ReceiveIR(uint8_t data);

    NT7508DrawInfo GetDrawInfo() const;

    void PressButton(ButtonType button) const;
    void ReleaseButton(ButtonType button) const;

    EepromBuffer GetEepromBuffer() const;
    void SetEepromBuffer(const EepromBuffer& buffer) const;

private:
    void CyclePeripherals(uint8_t cycles) const;
    void CycleEnhancements(uint8_t cycles) const;

    std::shared_ptr<H838606> soc = nullptr;

    // peripherals
    std::shared_ptr<BMA150> bma150 = nullptr;
    std::shared_ptr<M95512> m95512 = nullptr;
    std::shared_ptr<NT7508> nt7508 = nullptr;
    std::shared_ptr<Buzzer> buzzer = nullptr;

    // enhancements
    std::shared_ptr<ActivityTimerBypass> activity_timer_bypass = nullptr;

    std::shared_ptr<StepSampleProvider> step_provider = nullptr;

    std::atomic<bool> is_running = false;
    std::atomic<bool> is_paused = false;
    std::atomic<bool> is_fast_mode = false;
    std::atomic<bool> bypass_power_save = false;
};
