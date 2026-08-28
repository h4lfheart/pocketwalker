#include "rtc.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>

#include "core/soc/defines.h"

using Clock = std::chrono::steady_clock;

static uint8_t BCD(const uint8_t number)
{
    const uint8_t tens = std::floor(number / 10);
    const uint8_t ones = number % 10;

    return tens * 16 + ones;
}

static uint8_t FromBCD(const uint8_t value)
{
    return ((value >> 4) * 10) + (value & 0x0F);
}

static std::tm LocalTime(const time_t value)
{
    std::tm result = {};
#ifdef _WIN32
    localtime_s(&result, &value);
#else
    localtime_r(&value, &result);
#endif
    return result;
}

RTC::RTC(const std::shared_ptr<Interrupts>& interrupts)
{
    this->interrupts = interrupts;
    this->virtual_time = std::time(nullptr);
}

void RTC::RegisterIOHandlers(const std::shared_ptr<IO>& io)
{
    IO_HANDLER_READ_UNION(RTC_ADDR_RTCCR1, RTCCR1);
    io->RegisterWriteHandler(RTC_ADDR_RTCCR1, [this](const uint8_t value) { WriteRTCCR1(value); });

    IO_HANDLER_READ_VALUE(RTC_ADDR_RSECDR, RSECDR);
    io->RegisterWriteHandler(RTC_ADDR_RSECDR, [this](const uint8_t value) { WriteSeconds(value); });

    IO_HANDLER_READ_VALUE(RTC_ADDR_RMINDR, RMINDR);
    io->RegisterWriteHandler(RTC_ADDR_RMINDR, [this](const uint8_t value) { WriteMinutes(value); });

    IO_HANDLER_READ_VALUE(RTC_ADDR_RHRDR, RHRDR);
    io->RegisterWriteHandler(RTC_ADDR_RHRDR, [this](const uint8_t value) { WriteHours(value); });

    IO_HANDLER_READ_VALUE(RTC_ADDR_RWKDR, RWKDR);
    io->RegisterWriteHandler(RTC_ADDR_RWKDR, [this](const uint8_t value) { WriteWeekday(value); });
}

void RTC::LoadState(const std::string& path)
{
    const auto now_wall = Clock::now();
    ignore_rtc_writes_until = now_wall + std::chrono::seconds(3);
    wall_clock_initialized = false;

    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        virtual_time = std::time(nullptr);
        SetRegistersFromVirtualTime();
        last_time = LocalTime(virtual_time);
        return;
    }

    char magic[8] = {};
    int64_t saved_virtual_time = 0;
    int64_t saved_host_time = 0;
    f.read(magic, sizeof(magic));
    f.read(reinterpret_cast<char*>(&saved_virtual_time), sizeof(saved_virtual_time));
    f.read(reinterpret_cast<char*>(&saved_host_time), sizeof(saved_host_time));

    if (!f || std::string(magic, sizeof(magic)) != "PWRTC001")
    {
        virtual_time = std::time(nullptr);
    }
    else
    {
        const time_t now = std::time(nullptr);
        int64_t elapsed = static_cast<int64_t>(now) - saved_host_time;
        if (elapsed < 0)
            elapsed = 0;
        virtual_time = static_cast<time_t>(saved_virtual_time + elapsed);
    }

    SetRegistersFromVirtualTime();
    last_time = LocalTime(virtual_time);
}

void RTC::SaveState(const std::string& path)
{
    std::ofstream f(path, std::ios::binary);
    if (!f)
        return;

    const char magic[8] = {'P', 'W', 'R', 'T', 'C', '0', '0', '1'};
    const int64_t saved_virtual_time = static_cast<int64_t>(virtual_time);
    const int64_t saved_host_time = static_cast<int64_t>(std::time(nullptr));

    f.write(magic, sizeof(magic));
    f.write(reinterpret_cast<const char*>(&saved_virtual_time), sizeof(saved_virtual_time));
    f.write(reinterpret_cast<const char*>(&saved_host_time), sizeof(saved_host_time));
}

void RTC::Cycle(uint8_t cycles)
{
    (void)cycles;

    if (!RTCCR1.RUN)
    {
        wall_clock_initialized = false;
        return;
    }

    const auto now = Clock::now();
    if (!wall_clock_initialized)
    {
        last_wall_tick = now;
        wall_clock_initialized = true;
        return;
    }

    const auto tick = std::chrono::milliseconds(250);
    while (now - last_wall_tick >= tick)
    {
        last_wall_tick += tick;
        TickQuarter();
    }
}

void RTC::TickQuarter()
{
    quarters++;

    if (quarters % 4 == 0)
        virtual_time++;

    const std::tm current_time = LocalTime(virtual_time);
    SetRegistersFromVirtualTime();

    if (!initialized)
    {
        interrupts->RTCFLG.SEIFG025 = true;
        interrupts->RTCFLG.SEIFG05 = true;
        interrupts->RTCFLG.SEIFG1 = true;
        interrupts->RTCFLG.MNIFG = true;
        interrupts->RTCFLG.HRIFG = true;
        interrupts->RTCFLG.DYIFG = true;
        interrupts->RTCFLG.WKIFG = true;

        last_time = current_time;
        initialized = true;
    }

    interrupts->RTCFLG.SEIFG025 = true;

    if (quarters % 2 == 0)
        interrupts->RTCFLG.SEIFG05 = true;

    if (quarters % 4 == 0)
    {
        quarters = 0;

        if (current_time.tm_sec != last_time.tm_sec)
            interrupts->RTCFLG.SEIFG1 = true;

        if (current_time.tm_min != last_time.tm_min)
            interrupts->RTCFLG.MNIFG = true;

        if (current_time.tm_hour != last_time.tm_hour)
            interrupts->RTCFLG.HRIFG = true;

        if (current_time.tm_mday != last_time.tm_mday) [[unlikely]]
            interrupts->RTCFLG.DYIFG = true;

        if (current_time.tm_wday != last_time.tm_wday) [[unlikely]]
            interrupts->RTCFLG.WKIFG = true;

        last_time = current_time;
    }
}

void RTC::SetRegistersFromVirtualTime()
{
    const std::tm current_time = LocalTime(virtual_time);
    RSECDR = BCD(current_time.tm_sec);
    RMINDR = BCD(current_time.tm_min);
    RHRDR = BCD(RTCCR1.HR24 ? current_time.tm_hour : current_time.tm_hour % 12);
    RWKDR = BCD(current_time.tm_wday);
}

void RTC::SyncVirtualTimeFromRegisters()
{
    std::tm current_time = LocalTime(virtual_time);
    current_time.tm_sec = std::min<uint8_t>(FromBCD(RSECDR), 59);
    current_time.tm_min = std::min<uint8_t>(FromBCD(RMINDR), 59);

    uint8_t hour = FromBCD(RHRDR);
    if (!RTCCR1.HR24)
    {
        hour %= 12;
        if (RTCCR1.PM)
            hour += 12;
    }
    current_time.tm_hour = std::min<uint8_t>(hour, 23);
    current_time.tm_isdst = -1;

    virtual_time = std::mktime(&current_time);
    last_time = LocalTime(virtual_time);
}

bool RTC::CanAcceptRtcWrites() const
{
    return Clock::now() >= ignore_rtc_writes_until;
}

void RTC::WriteRTCCR1(const uint8_t value)
{
    RTCCR1.VALUE = value;
    if (CanAcceptRtcWrites())
        SyncVirtualTimeFromRegisters();
}

void RTC::WriteSeconds(const uint8_t value)
{
    RSECDR = value;
    if (CanAcceptRtcWrites())
        SyncVirtualTimeFromRegisters();
}

void RTC::WriteMinutes(const uint8_t value)
{
    RMINDR = value;
    if (CanAcceptRtcWrites())
        SyncVirtualTimeFromRegisters();
}

void RTC::WriteHours(const uint8_t value)
{
    RHRDR = value;
    if (CanAcceptRtcWrites())
        SyncVirtualTimeFromRegisters();
}

void RTC::WriteWeekday(const uint8_t value)
{
    RWKDR = value;
    if (CanAcceptRtcWrites())
        SyncVirtualTimeFromRegisters();
}
