#include "rtc.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "core/soc/defines.h"

using Clock = std::chrono::steady_clock;

namespace
{
constexpr auto CATCH_UP_MIN_MIDNIGHT_HOLD = std::chrono::milliseconds(1);
constexpr auto CATCH_UP_POST_DAY_SETTLE = std::chrono::milliseconds(2);
constexpr auto CATCH_UP_MAX_MIDNIGHT_HOLD = std::chrono::milliseconds(80);

template <typename T>
void WriteRtcValue(std::ostream& stream, const T& value)
{
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool ReadRtcValue(std::istream& stream, T& value)
{
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(stream);
}
}

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

static std::string FormatLocalTime(const time_t value)
{
    std::ostringstream stream;
    const std::tm local_time = LocalTime(value);
    stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

static std::string DescribeTime(const time_t value)
{
    std::ostringstream stream;
    stream << FormatLocalTime(value) << " (" << static_cast<int64_t>(value) << ")";
    return stream.str();
}

static bool TryParseLocalTime(const std::string& text, time_t& value)
{
    std::tm parsed = {};
    std::istringstream stream(text);
    stream >> std::get_time(&parsed, "%Y-%m-%d %H:%M:%S");

    if (stream.fail())
        return false;

    parsed.tm_isdst = -1;
    value = std::mktime(&parsed);
    return value != static_cast<time_t>(-1);
}

static bool TryReadClockFile(const std::filesystem::path& path, time_t& value)
{
    std::ifstream clock_file(path);
    if (clock_file)
    {
        std::string text;
        std::getline(clock_file, text);
        return TryParseLocalTime(text, value);
    }

    return false;
}

struct HostClockInfo
{
    time_t value = 0;
    time_t custom_time = 0;
    time_t anchor_host_time = 0;
    int64_t elapsed_from_anchor = 0;
    bool anchor_paused = false;
    std::string source = "PC system clock";
};

static std::string ClockAnchorValue(const std::string& line)
{
    const auto separator = line.find('=');
    if (separator == std::string::npos)
        return line;

    return line.substr(separator + 1);
}

static bool IsPausedClockAnchor(const std::string& line)
{
    const std::string value = ClockAnchorValue(line);
    return value == "paused" || value == "true" || value == "1";
}

static bool TryReadClockAnchor(const std::filesystem::path& path, HostClockInfo& info)
{
    std::ifstream clock_file(path);
    if (!clock_file)
        return false;

    std::string custom_line;
    std::string host_line;
    std::string mode_line;
    std::getline(clock_file, custom_line);
    std::getline(clock_file, host_line);
    std::getline(clock_file, mode_line);

    time_t custom_time = 0;
    if (!TryParseLocalTime(ClockAnchorValue(custom_line), custom_time))
        return false;

    info.custom_time = custom_time;
    info.source = "custom anchor: " + path.string();

    if (IsPausedClockAnchor(mode_line))
    {
        info.value = custom_time;
        info.anchor_paused = true;
        return true;
    }

    time_t host_time = 0;
    if (!TryParseLocalTime(ClockAnchorValue(host_line), host_time))
    {
        info.value = custom_time;
        return true;
    }

    int64_t elapsed = static_cast<int64_t>(std::time(nullptr)) - static_cast<int64_t>(host_time);
    if (elapsed < 0)
        elapsed = 0;

    info.anchor_host_time = host_time;
    info.elapsed_from_anchor = elapsed;
    info.value = static_cast<time_t>(static_cast<int64_t>(custom_time) + elapsed);
    return true;
}

static HostClockInfo CurrentHostClock(const std::filesystem::path& clock_directory = std::filesystem::current_path())
{
    (void)clock_directory;
    static bool test_elapsed_clock_initialized = false;
    static std::string test_elapsed_clock_text;
    static time_t test_elapsed_clock_base = 0;
    static Clock::time_point test_elapsed_clock_started = {};

    HostClockInfo info = {};
    if (const char* env_time = std::getenv("POCKETWALKER_CLOCK"))
    {
        time_t parsed = 0;
        if (TryParseLocalTime(env_time, parsed))
        {
            if (std::getenv("POCKETWALKER_CLOCK_ELAPSES"))
            {
                const std::string current_text = env_time;
                if (!test_elapsed_clock_initialized || test_elapsed_clock_text != current_text)
                {
                    test_elapsed_clock_initialized = true;
                    test_elapsed_clock_text = current_text;
                    test_elapsed_clock_base = parsed;
                    test_elapsed_clock_started = Clock::now();
                }

                const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    Clock::now() - test_elapsed_clock_started).count();
                info.value = static_cast<time_t>(static_cast<int64_t>(test_elapsed_clock_base) + elapsed);
                info.source = "POCKETWALKER_CLOCK_ELAPSES";
                return info;
            }

            info.value = parsed;
            info.source = "POCKETWALKER_CLOCK";
            return info;
        }
    }

    info.value = std::time(nullptr);
    info.source = "PC system clock";

    return info;
}

static time_t CurrentHostTime(const std::filesystem::path& clock_directory = std::filesystem::current_path())
{
    return CurrentHostClock(clock_directory).value;
}

static bool TryReadSyncClockFromPath(const std::filesystem::path& path, time_t& value)
{
    if (!TryReadClockFile(path, value))
        return false;

    std::error_code error;
    std::filesystem::remove(path, error);
    return true;
}

static bool TryReadSyncClock(const std::filesystem::path& clock_directory, time_t& value)
{
    if (const char* env_time = std::getenv("POCKETWALKER_SYNC_CLOCK"))
    {
        if (TryParseLocalTime(env_time, value))
            return true;
    }

    const auto base_path = clock_directory.empty() ? std::filesystem::current_path() : clock_directory;
    const auto sync_path = base_path / "pocketwalker_sync_clock.txt";
    if (TryReadSyncClockFromPath(sync_path, value))
        return true;

    const auto current_sync_path = std::filesystem::current_path() / "pocketwalker_sync_clock.txt";
    if (current_sync_path != sync_path && TryReadSyncClockFromPath(current_sync_path, value))
        return true;

    return false;
}

static time_t NextLocalMidnightAfter(const time_t value)
{
    std::tm time = LocalTime(value);
    time.tm_sec = 0;
    time.tm_min = 0;
    time.tm_hour = 0;
    time.tm_mday += 1;
    time.tm_isdst = -1;
    return std::mktime(&time);
}

static time_t LocalMidnightAtOrBefore(const time_t value)
{
    std::tm time = LocalTime(value);
    time.tm_sec = 0;
    time.tm_min = 0;
    time.tm_hour = 0;
    time.tm_isdst = -1;
    return std::mktime(&time);
}

static bool IsCloseToLocalMidnight(const time_t value)
{
    const std::tm time = LocalTime(value);
    return time.tm_hour == 0 && time.tm_min < 5;
}

RTC::RTC(const std::shared_ptr<Interrupts>& interrupts)
{
    this->interrupts = interrupts;
    this->virtual_time = CurrentHostTime();
    this->last_processed_midnight = LocalMidnightAtOrBefore(this->virtual_time);
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

void RTC::DebugLog(const std::string& message) const
{
    if (debug_log_path.empty())
        return;

    const std::filesystem::path debug_directory = debug_log_path.parent_path();
    if (!std::filesystem::exists(debug_directory / "pocketwalker_enable_debug_log.txt"))
        return;

    std::ofstream log(debug_log_path, std::ios::app);
    if (!log)
        return;

    const std::time_t now = std::time(nullptr);
    log << FormatLocalTime(now) << " | " << message << '\n';
}

void RTC::DebugMessage(const std::string& message) const
{
    DebugLog(message);
}

void RTC::LoadState(const std::string& path)
{
    const auto rtc_path = std::filesystem::path(path);
    const auto rtc_directory = rtc_path.parent_path();
    clock_directory = rtc_directory;
    debug_log_path = rtc_directory / "pocketwalker_rtc_debug.log";
    DebugLog("----- RTC LoadState begin -----");
    DebugLog("rtc_path=" + rtc_path.string());
    DebugLog("state_virtual_before_rtc=" + DescribeTime(virtual_time));
    DebugLog("state_last_processed_before_rtc=" + DescribeTime(last_processed_midnight));

    const auto now_wall = Clock::now();
    ignore_rtc_writes_until = now_wall + std::chrono::seconds(3);
    wall_clock_initialized = false;
    catch_up_allowed_to_run = false;
    catch_up_midnights.clear();
    catch_up_midnight_index = 0;
    catch_up_target_time = 0;
    catch_up_target_host_time = 0;
    catch_up_current_midnight = 0;
    catch_up_overflow_days = 0;
    catch_up_waiting_for_firmware_settle = false;
    suppress_day_week_flags_once = false;
    has_pending_sync_time = false;
    pending_sync_time = 0;
    catch_up_hold_until = {};
    catch_up_force_next_after = {};
    next_sync_clock_check = {};

    // A saved state can contain an RTC day/week interrupt that was already
    // accounted for by the persisted processed-midnight metadata. Clear those
    // stale flags before catch-up decides which midnights still need replaying.
    interrupts->RTCFLG.DYIFG = false;
    interrupts->RTCFLG.WKIFG = false;

    time_t sync_time = 0;
    if (TryReadSyncClock(rtc_directory, sync_time))
    {
        has_pending_sync_time = true;
        pending_sync_time = sync_time;
        ApplyPendingSyncClock();
        DebugLog("RTC LoadState end virtual=" + DescribeTime(virtual_time) +
                 " catch_up_active=false sync_clock_applied=true");
        return;
    }

    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        last_processed_midnight = LocalMidnightAtOrBefore(virtual_time);
        last_time = LocalTime(virtual_time);
        DebugLog("rtc_metadata=missing; catch-up skipped, last_processed_midnight=" + DescribeTime(last_processed_midnight));
        return;
    }

    char magic[8] = {};
    int64_t saved_virtual_time = 0;
    int64_t saved_host_time = 0;
    int64_t saved_processed_midnight = 0;
    f.read(magic, sizeof(magic));
    f.read(reinterpret_cast<char*>(&saved_virtual_time), sizeof(saved_virtual_time));
    f.read(reinterpret_cast<char*>(&saved_host_time), sizeof(saved_host_time));
    f.read(reinterpret_cast<char*>(&saved_processed_midnight), sizeof(saved_processed_midnight));

    if (!f || std::string(magic, sizeof(magic)) != "PWRTC002")
    {
        last_processed_midnight = LocalMidnightAtOrBefore(virtual_time);
        DebugLog("rtc_metadata=invalid_or_old magic=" + std::string(magic, sizeof(magic)) +
                 "; catch-up skipped, last_processed_midnight=" + DescribeTime(last_processed_midnight));
    }
    else
    {
        const HostClockInfo clock = CurrentHostClock(rtc_directory);
        const time_t now = clock.value;
        int64_t elapsed = static_cast<int64_t>(now) - saved_host_time;
        if (elapsed < 0)
            elapsed = 0;

        std::ostringstream source_details;
        source_details << "clock_source=" << clock.source
                       << " now=" << DescribeTime(now)
                       << " saved_virtual=" << DescribeTime(static_cast<time_t>(saved_virtual_time))
                       << " saved_host=" << DescribeTime(static_cast<time_t>(saved_host_time))
                       << " saved_processed_midnight=" << DescribeTime(static_cast<time_t>(saved_processed_midnight))
                       << " elapsed_seconds=" << elapsed;
        if (clock.custom_time != 0)
        {
            source_details << " anchor_custom=" << DescribeTime(clock.custom_time)
                           << " anchor_host=" << DescribeTime(clock.anchor_host_time)
                           << " anchor_elapsed=" << clock.elapsed_from_anchor
                           << " anchor_paused=" << (clock.anchor_paused ? "true" : "false");
        }
        DebugLog(source_details.str());

        const int64_t current_virtual_time = static_cast<int64_t>(virtual_time);
        if (std::llabs(current_virtual_time - saved_virtual_time) > 5)
        {
            last_processed_midnight = LocalMidnightAtOrBefore(virtual_time);
            DebugLog("state_metadata_mismatch=true current_virtual=" + DescribeTime(virtual_time) +
                     "; catch-up skipped, last_processed_midnight=" + DescribeTime(last_processed_midnight));
        }
        else
        {
            last_processed_midnight = static_cast<time_t>(saved_processed_midnight);
            if (last_processed_midnight == 0)
                last_processed_midnight = LocalMidnightAtOrBefore(virtual_time);

            const time_t target_time = static_cast<time_t>(current_virtual_time + elapsed);
            DebugLog("state_metadata_mismatch=false target_time=" + DescribeTime(target_time));
            if (elapsed > 0)
                StartCatchUp(last_processed_midnight, target_time, now);
            else
                DebugLog("elapsed_seconds=0; no catch-up needed");
        }
    }

    SetRegistersFromVirtualTime();
    if (!initialized)
        last_time = LocalTime(virtual_time);
    DebugLog("RTC LoadState end virtual=" + DescribeTime(virtual_time) +
             " catch_up_active=" + (IsCatchUpActive() ? std::string("true") : std::string("false")));
}

void RTC::ApplyPendingSyncClock()
{
    if (!has_pending_sync_time)
        return;

    DebugLog("ApplyPendingSyncClock requested: " + DescribeTime(pending_sync_time));
    virtual_time = pending_sync_time;
    pending_sync_time = 0;
    has_pending_sync_time = false;
    catch_up_allowed_to_run = false;
    catch_up_midnights.clear();
    catch_up_midnight_index = 0;
    catch_up_target_time = 0;
    catch_up_target_host_time = 0;
    catch_up_current_midnight = 0;
    catch_up_overflow_days = 0;
    catch_up_waiting_for_firmware_settle = false;
    suppress_day_week_flags_once = false;
    catch_up_hold_until = {};
    catch_up_force_next_after = {};
    wall_clock_initialized = false;
    quarters = 0;
    ignore_rtc_writes_until = Clock::now() + std::chrono::seconds(3);
    interrupts->RTCFLG.VALUE = 0;

    SetRegistersFromVirtualTime();
    last_time = LocalTime(virtual_time);
    last_processed_midnight = LocalMidnightAtOrBefore(virtual_time);
    initialized = true;
    interrupts->RTCFLG.SEIFG025 = true;
    interrupts->RTCFLG.SEIFG05 = true;
    interrupts->RTCFLG.SEIFG1 = true;
    interrupts->RTCFLG.MNIFG = true;
    interrupts->RTCFLG.HRIFG = true;
    DebugLog("ApplyPendingSyncClock complete virtual=" + DescribeTime(virtual_time) +
             " last_processed_midnight=" + DescribeTime(last_processed_midnight));
}

bool RTC::IsCatchUpActive() const
{
    return catch_up_target_time != 0;
}

bool RTC::IsCatchUpWaitingForFirmwareSettle() const
{
    return IsCatchUpActive() && catch_up_waiting_for_firmware_settle;
}

size_t RTC::CatchUpMidnightsCompleted() const
{
    return catch_up_midnight_index;
}

size_t RTC::CatchUpMidnightsTotal() const
{
    return catch_up_midnights.size();
}

uint32_t RTC::ConsumeCatchUpOverflowDays()
{
    const uint32_t result = catch_up_overflow_days;
    catch_up_overflow_days = 0;
    if (result != 0)
        DebugLog("overflow_days_consumed=" + std::to_string(result));
    return result;
}

void RTC::MarkCurrentDayProcessed()
{
    const time_t processed_midnight = LocalMidnightAtOrBefore(virtual_time);
    time_t catch_up_processed_midnight = catch_up_current_midnight;
    if (IsCatchUpActive() &&
        catch_up_processed_midnight == 0 &&
        catch_up_midnight_index < catch_up_midnights.size() &&
        processed_midnight <= last_processed_midnight)
    {
        catch_up_processed_midnight = catch_up_midnights[catch_up_midnight_index];
        DebugLog("firmware_day_processed_from_loaded_state queued_midnight=" +
                 DescribeTime(catch_up_processed_midnight) +
                 " virtual=" + DescribeTime(virtual_time));
    }

    const bool processed_catch_up_midnight =
        IsCatchUpActive() &&
        catch_up_processed_midnight != 0 &&
        catch_up_processed_midnight > last_processed_midnight;

    if (processed_catch_up_midnight)
    {
        last_processed_midnight = catch_up_processed_midnight;
        DebugLog("firmware_day_processed virtual=" + DescribeTime(virtual_time) +
                 " last_processed_midnight=" + DescribeTime(last_processed_midnight));
    }
    else if (processed_midnight > last_processed_midnight)
    {
        last_processed_midnight = processed_midnight;
        DebugLog("firmware_day_processed virtual=" + DescribeTime(virtual_time) +
                 " last_processed_midnight=" + DescribeTime(last_processed_midnight));
    }

    while (catch_up_midnight_index < catch_up_midnights.size() &&
           catch_up_midnights[catch_up_midnight_index] <= last_processed_midnight)
    {
        catch_up_midnight_index++;
    }

    if (processed_catch_up_midnight)
    {
        if (catch_up_current_midnight == 0)
            catch_up_current_midnight = catch_up_processed_midnight;

        catch_up_waiting_for_firmware_settle = true;
        catch_up_hold_until = Clock::now() + CATCH_UP_POST_DAY_SETTLE;
        DebugLog("catch_up_midnight_confirmed midnight=" + DescribeTime(catch_up_processed_midnight) +
                 " completed=" + std::to_string(catch_up_midnight_index) +
                 "/" + std::to_string(catch_up_midnights.size()));
    }
}

void RTC::MarkCatchUpFirmwareSettled()
{
    if (!IsCatchUpActive() || !catch_up_waiting_for_firmware_settle)
        return;

    catch_up_waiting_for_firmware_settle = false;
    DebugLog("catch_up_firmware_settled midnight=" + DescribeTime(catch_up_current_midnight) +
             " completed=" + std::to_string(catch_up_midnight_index) +
             "/" + std::to_string(catch_up_midnights.size()));
}

void RTC::ClearPendingInterruptFlagsForCatchUp()
{
    if (IsCatchUpActive())
    {
        DebugLog("clearing synthetic RTC flags after firmware handled catch-up day; RTCFLG=" +
                 std::to_string(interrupts->RTCFLG.VALUE));
        interrupts->RTCFLG.VALUE = 0;
        return;
    }

    if (interrupts->RTCFLG.DYIFG || interrupts->RTCFLG.WKIFG)
    {
        DebugLog("clearing handled natural day/week RTC flags; RTCFLG=" +
                 std::to_string(interrupts->RTCFLG.VALUE));
        interrupts->RTCFLG.DYIFG = false;
        interrupts->RTCFLG.WKIFG = false;
    }
}

void RTC::AllowCatchUpToRun(bool value)
{
    if (catch_up_allowed_to_run == value)
        return;

    catch_up_allowed_to_run = value;
    DebugLog(std::string("catch_up_allowed_to_run=") + (value ? "true" : "false"));
}

void RTC::StartCatchUp(const time_t processed_midnight, const time_t target_time, const time_t target_host_time)
{
    const time_t saved_time = virtual_time;
    DebugLog("StartCatchUp saved_time=" + DescribeTime(saved_time) +
             " processed_midnight=" + DescribeTime(processed_midnight) +
             " target_time=" + DescribeTime(target_time));

    catch_up_midnights.clear();
    catch_up_midnight_index = 0;
    catch_up_target_time = 0;
    catch_up_target_host_time = target_host_time;
    catch_up_current_midnight = 0;
    catch_up_overflow_days = 0;
    catch_up_waiting_for_firmware_settle = false;
    catch_up_hold_until = {};
    catch_up_force_next_after = {};

    if (processed_midnight > 0)
        last_processed_midnight = processed_midnight;
    else if (last_processed_midnight == 0)
        last_processed_midnight = LocalMidnightAtOrBefore(saved_time);

    for (time_t midnight = NextLocalMidnightAfter(last_processed_midnight);
         midnight > last_processed_midnight && midnight <= target_time;
         midnight = NextLocalMidnightAfter(midnight))
    {
        catch_up_midnights.push_back(midnight);
    }

    if (catch_up_midnights.empty())
    {
        virtual_time = target_time;
        last_time = LocalTime(saved_time);
        initialized = true;
        SetRegistersFromVirtualTime();
        interrupts->RTCFLG.VALUE = 0;
        const std::tm target_local_time = LocalTime(virtual_time);
        if (!IsCloseToLocalMidnight(virtual_time) && target_local_time.tm_hour != 0)
        {
            RequestClockDisplayRefresh();
        }
        else
        {
            RequestClockDisplayRefreshNearMidnight();
            DebugLog("StartCatchUp no missed midnights; partial display refresh near midnight/hour");
        }
        DebugLog("StartCatchUp no missed midnights; direct virtual_time=" + DescribeTime(virtual_time));
        catch_up_target_host_time = 0;
        return;
    }

    virtual_time = catch_up_midnights.front() - 1;
    catch_up_target_time = target_time;
    catch_up_target_host_time = target_host_time;
    last_time = LocalTime(virtual_time);
    initialized = true;
    DebugLog("StartCatchUp queued_midnights=" + std::to_string(catch_up_midnights.size()) +
             " first_midnight=" + DescribeTime(catch_up_midnights.front()) +
             " start_virtual=" + DescribeTime(virtual_time));
}

void RTC::CycleCatchUp()
{
    const auto now = Clock::now();

    if (catch_up_current_midnight != 0)
    {
        if (now < catch_up_hold_until)
            return;

        if (last_processed_midnight < catch_up_current_midnight && now < catch_up_force_next_after)
            return;

        if (catch_up_waiting_for_firmware_settle && now < catch_up_force_next_after)
            return;

        if (catch_up_waiting_for_firmware_settle)
        {
            DebugLog("catch_up_firmware_settle_timeout midnight=" + DescribeTime(catch_up_current_midnight));
            catch_up_waiting_for_firmware_settle = false;
        }

        if (last_processed_midnight < catch_up_current_midnight)
        {
            DebugLog("catch_up_midnight_retry midnight=" + DescribeTime(catch_up_current_midnight) +
                     " last_processed_midnight=" + DescribeTime(last_processed_midnight));
        }

        catch_up_current_midnight = 0;
        catch_up_waiting_for_firmware_settle = false;
        catch_up_hold_until = {};
        catch_up_force_next_after = {};
    }

    if (catch_up_current_midnight == 0)
    {
        while (catch_up_midnight_index < catch_up_midnights.size() &&
               catch_up_midnights[catch_up_midnight_index] <= last_processed_midnight)
        {
            catch_up_midnight_index++;
        }

        if (catch_up_midnight_index < catch_up_midnights.size())
        {
            catch_up_current_midnight = catch_up_midnights[catch_up_midnight_index];
            virtual_time = catch_up_current_midnight - 1;
            last_time = LocalTime(virtual_time);
            quarters = 3;
            TickQuarter();
            DebugLog("catch_up_midnight_pulse index=" + std::to_string(catch_up_midnight_index + 1) +
                     "/" + std::to_string(catch_up_midnights.size()) +
                     " midnight=" + DescribeTime(catch_up_current_midnight) +
                     " virtual_after_tick=" + DescribeTime(virtual_time));
            catch_up_hold_until = now + CATCH_UP_MIN_MIDNIGHT_HOLD;
            catch_up_force_next_after = now + CATCH_UP_MAX_MIDNIGHT_HOLD;
            return;
        }

        const time_t completed_target_time = catch_up_target_time;
        const time_t completed_target_host_time = catch_up_target_host_time;

        virtual_time = completed_target_time;
        SetRegistersFromVirtualTime();
        last_time = LocalTime(virtual_time);
        interrupts->RTCFLG.VALUE = 0;

        if (completed_target_host_time > 0)
        {
            const HostClockInfo clock = CurrentHostClock(clock_directory);
            int64_t extra_elapsed = static_cast<int64_t>(clock.value) - static_cast<int64_t>(completed_target_host_time);
            if (extra_elapsed < 0)
                extra_elapsed = 0;

            const time_t follow_up_target_time =
                static_cast<time_t>(static_cast<int64_t>(completed_target_time) + extra_elapsed);
            DebugLog("catch_up_post_load_elapsed host_now=" + DescribeTime(clock.value) +
                     " target_host=" + DescribeTime(completed_target_host_time) +
                     " extra_seconds=" + std::to_string(extra_elapsed) +
                     " follow_up_target=" + DescribeTime(follow_up_target_time));

            const time_t next_midnight = NextLocalMidnightAfter(last_processed_midnight);
            if (extra_elapsed > 0 && next_midnight > last_processed_midnight && next_midnight <= follow_up_target_time)
            {
                DebugLog("catch_up_post_load_queue_extra_midnights next_midnight=" + DescribeTime(next_midnight));
                StartCatchUp(last_processed_midnight, follow_up_target_time, clock.value);
                return;
            }

            if (extra_elapsed > 0)
            {
                virtual_time = follow_up_target_time;
                SetRegistersFromVirtualTime();
                last_time = LocalTime(virtual_time);
            }
        }

        const std::tm completed_local_time = LocalTime(virtual_time);
        if (!IsCloseToLocalMidnight(virtual_time) && completed_local_time.tm_hour != 0)
        {
            RequestClockDisplayRefresh();
        }
        else
        {
            RequestClockDisplayRefreshNearMidnight();
            DebugLog("catch_up_complete partial display refresh near midnight/hour");
        }
        DebugLog("catch_up_complete target_virtual=" + DescribeTime(virtual_time));
        suppress_day_week_flags_once = true;
        catch_up_target_time = 0;
        catch_up_target_host_time = 0;
        catch_up_midnights.clear();
        catch_up_midnight_index = 0;
        catch_up_current_midnight = 0;
        catch_up_waiting_for_firmware_settle = false;
        catch_up_hold_until = {};
        catch_up_force_next_after = {};
        wall_clock_initialized = false;
        return;
    }
}

void RTC::SaveState(const std::string& path)
{
    const auto rtc_path = std::filesystem::path(path);
    const auto rtc_directory = rtc_path.parent_path();
    if (debug_log_path.empty())
        debug_log_path = rtc_directory / "pocketwalker_rtc_debug.log";

    std::ofstream f(path, std::ios::binary);
    if (!f)
    {
        DebugLog("SaveState failed path=" + rtc_path.string());
        return;
    }

    const char magic[8] = {'P', 'W', 'R', 'T', 'C', '0', '0', '2'};
    const int64_t saved_virtual_time = static_cast<int64_t>(IsCatchUpActive() ? catch_up_target_time : virtual_time);
    const HostClockInfo clock = CurrentHostClock(rtc_directory);
    const int64_t saved_host_time = static_cast<int64_t>(clock.value);
    int64_t saved_processed_midnight = static_cast<int64_t>(last_processed_midnight);
    if (saved_processed_midnight == 0)
        saved_processed_midnight = static_cast<int64_t>(LocalMidnightAtOrBefore(virtual_time));

    f.write(magic, sizeof(magic));
    f.write(reinterpret_cast<const char*>(&saved_virtual_time), sizeof(saved_virtual_time));
    f.write(reinterpret_cast<const char*>(&saved_host_time), sizeof(saved_host_time));
    f.write(reinterpret_cast<const char*>(&saved_processed_midnight), sizeof(saved_processed_midnight));
    DebugLog("SaveState path=" + rtc_path.string() +
             " virtual=" + DescribeTime(static_cast<time_t>(saved_virtual_time)) +
             " host=" + DescribeTime(static_cast<time_t>(saved_host_time)) +
             " processed_midnight=" + DescribeTime(static_cast<time_t>(saved_processed_midnight)) +
             " catch_up_active=" + (IsCatchUpActive() ? std::string("true") : std::string("false")) +
             " clock_source=" + clock.source +
             " anchor_paused=" + (clock.anchor_paused ? std::string("true") : std::string("false")));
}

bool RTC::LoadEmulatorState(std::istream& stream)
{
    int64_t saved_virtual_time = 0;
    int64_t saved_last_time = 0;

    if (!ReadRtcValue(stream, saved_virtual_time) ||
        !ReadRtcValue(stream, saved_last_time) ||
        !ReadRtcValue(stream, quarters) ||
        !ReadRtcValue(stream, initialized))
    {
        return false;
    }

    virtual_time = static_cast<time_t>(saved_virtual_time);
    last_time = LocalTime(static_cast<time_t>(saved_last_time));
    wall_clock_initialized = false;
    catch_up_allowed_to_run = false;
    catch_up_midnights.clear();
    catch_up_midnight_index = 0;
    catch_up_target_time = 0;
    catch_up_target_host_time = 0;
    catch_up_current_midnight = 0;
    catch_up_overflow_days = 0;
    catch_up_waiting_for_firmware_settle = false;
    suppress_day_week_flags_once = false;
    has_pending_sync_time = false;
    pending_sync_time = 0;
    catch_up_hold_until = {};
    catch_up_force_next_after = {};
    ignore_rtc_writes_until = Clock::now() + std::chrono::seconds(3);
    SetRegistersFromVirtualTime();
    return true;
}

void RTC::SaveEmulatorState(std::ostream& stream) const
{
    std::tm saved_last_tm = last_time;
    const int64_t saved_virtual_time = static_cast<int64_t>(IsCatchUpActive() ? catch_up_target_time : virtual_time);
    const int64_t saved_last_time = IsCatchUpActive()
        ? saved_virtual_time
        : static_cast<int64_t>(std::mktime(&saved_last_tm));
    const uint8_t saved_quarters = IsCatchUpActive() ? 0 : quarters;

    WriteRtcValue(stream, saved_virtual_time);
    WriteRtcValue(stream, saved_last_time);
    WriteRtcValue(stream, saved_quarters);
    WriteRtcValue(stream, initialized);
}

void RTC::Cycle(uint8_t cycles)
{
    (void)cycles;

    const auto now = Clock::now();

    if (IsCatchUpActive())
    {
        if (catch_up_allowed_to_run)
            CycleCatchUp();
        return;
    }

    if (!RTCCR1.RUN)
    {
        wall_clock_initialized = false;
        return;
    }

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

        const bool suppress_day_week = suppress_day_week_flags_once;
        suppress_day_week_flags_once = false;

        if (current_time.tm_mday != last_time.tm_mday) [[unlikely]]
        {
            if (!suppress_day_week)
                interrupts->RTCFLG.DYIFG = true;
            else
                DebugLog("suppressed day flag after catch-up virtual=" + DescribeTime(virtual_time));
        }

        if (current_time.tm_wday != last_time.tm_wday) [[unlikely]]
        {
            if (!suppress_day_week)
                interrupts->RTCFLG.WKIFG = true;
            else
                DebugLog("suppressed week flag after catch-up virtual=" + DescribeTime(virtual_time));
        }

        last_time = current_time;
    }
}

void RTC::SetRegistersFromVirtualTime()
{
    const std::tm current_time = LocalTime(virtual_time);
    RSECDR = BCD(current_time.tm_sec);
    RMINDR = BCD(current_time.tm_min);
    if (RTCCR1.HR24)
    {
        RHRDR = BCD(current_time.tm_hour);
    }
    else
    {
        RTCCR1.PM = current_time.tm_hour >= 12;
        RHRDR = BCD(current_time.tm_hour % 12);
    }
    RWKDR = BCD(current_time.tm_wday);
}

void RTC::RequestClockDisplayRefresh()
{
    interrupts->RTCFLG.SEIFG025 = true;
    interrupts->RTCFLG.SEIFG05 = true;
    interrupts->RTCFLG.SEIFG1 = true;
    interrupts->RTCFLG.MNIFG = true;
    interrupts->RTCFLG.HRIFG = true;
}

void RTC::RequestClockDisplayRefreshNearMidnight()
{
    interrupts->RTCFLG.SEIFG025 = true;
    interrupts->RTCFLG.SEIFG05 = true;
    interrupts->RTCFLG.SEIFG1 = true;
    interrupts->RTCFLG.MNIFG = true;
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
    const bool accepted = CanAcceptRtcWrites();
    if (accepted)
        SyncVirtualTimeFromRegisters();
}

void RTC::WriteSeconds(const uint8_t value)
{
    RSECDR = value;
    const bool accepted = CanAcceptRtcWrites();
    if (accepted)
        SyncVirtualTimeFromRegisters();
}

void RTC::WriteMinutes(const uint8_t value)
{
    RMINDR = value;
    const bool accepted = CanAcceptRtcWrites();
    if (accepted)
        SyncVirtualTimeFromRegisters();
}

void RTC::WriteHours(const uint8_t value)
{
    RHRDR = value;
    const bool accepted = CanAcceptRtcWrites();
    if (accepted)
        SyncVirtualTimeFromRegisters();
}

void RTC::WriteWeekday(const uint8_t value)
{
    RWKDR = value;
    const bool accepted = CanAcceptRtcWrites();
    if (accepted)
        SyncVirtualTimeFromRegisters();
}
