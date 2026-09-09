#include "emulator_context.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include "desktop/src/qt/settings/app_settings.h"

namespace
{
constexpr std::streamoff STATE_EEPROM_OFFSET = 44;
constexpr std::streamoff STATE_RAM_TOTAL_STEPS_OFFSET = 0x100BB;
constexpr std::streamoff STATE_RAM_TOTAL_DAYS_OFFSET = 0x100C7;
constexpr std::streamoff STATE_RAM_SESSION_STEPS_OFFSET = 0x100D7;
constexpr size_t EEPROM_TOTAL_STEPS_OFFSET = 0x156;
constexpr size_t EEPROM_TOTAL_DAYS_OFFSET = 0x162;
constexpr size_t EEPROM_HISTORY_STEPS_OFFSET = 0xCEF0;
constexpr size_t EEPROM_HISTORY_DAYS = 7;

uint32_t ReadU32BEFromFile(const std::string& path, const std::streamoff offset)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return 0;

    file.seekg(offset);
    uint8_t b0 = 0;
    uint8_t b1 = 0;
    uint8_t b2 = 0;
    uint8_t b3 = 0;
    file.read(reinterpret_cast<char*>(&b0), sizeof(b0));
    file.read(reinterpret_cast<char*>(&b1), sizeof(b1));
    file.read(reinterpret_cast<char*>(&b2), sizeof(b2));
    file.read(reinterpret_cast<char*>(&b3), sizeof(b3));
    if (!file)
        return 0;

    return (static_cast<uint32_t>(b0) << 24) |
           (static_cast<uint32_t>(b1) << 16) |
           (static_cast<uint32_t>(b2) << 8) |
           static_cast<uint32_t>(b3);
}

uint16_t ReadDaysFromEepromFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return 0;

    file.seekg(EEPROM_TOTAL_DAYS_OFFSET);
    uint8_t hi = 0;
    uint8_t lo = 0;
    file.read(reinterpret_cast<char*>(&hi), sizeof(hi));
    file.read(reinterpret_cast<char*>(&lo), sizeof(lo));
    if (!file)
        return 0;

    return static_cast<uint16_t>((hi << 8) | lo);
}

std::string ReadHistoryFromEepromFile(const std::string& path)
{
    std::ostringstream stream;
    for (size_t i = 0; i < EEPROM_HISTORY_DAYS; i++)
    {
        if (i > 0)
            stream << ",";

        stream << "-" << (i + 1) << "="
               << ReadU32BEFromFile(path, static_cast<std::streamoff>(EEPROM_HISTORY_STEPS_OFFSET + i * sizeof(uint32_t)));
    }

    return stream.str();
}

uint16_t ReadDaysFromStateRam(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return 0;

    file.seekg(STATE_RAM_TOTAL_DAYS_OFFSET);
    uint8_t hi = 0;
    uint8_t lo = 0;
    file.read(reinterpret_cast<char*>(&hi), sizeof(hi));
    file.read(reinterpret_cast<char*>(&lo), sizeof(lo));
    if (!file)
        return 0;

    return static_cast<uint16_t>((hi << 8) | lo);
}

bool StateEepromMatchesSave(const std::string& save_path, const std::string& state_path)
{
    if (!std::filesystem::exists(state_path))
        return false;

    if (!std::filesystem::exists(save_path))
        return true;

    std::ifstream save(save_path, std::ios::binary);
    std::ifstream state(state_path, std::ios::binary);
    if (!save || !state)
        return false;

    state.seekg(STATE_EEPROM_OFFSET);

    std::array<char, 4096> save_block = {};
    std::array<char, 4096> state_block = {};
    size_t compared = 0;
    while (compared < EepromBuffer{}.size())
    {
        const size_t wanted = std::min(save_block.size(), EepromBuffer{}.size() - compared);
        save.read(save_block.data(), static_cast<std::streamsize>(wanted));
        state.read(state_block.data(), static_cast<std::streamsize>(wanted));
        if (!save || !state)
            return false;

        if (!std::equal(save_block.begin(), save_block.begin() + static_cast<std::ptrdiff_t>(wanted), state_block.begin()))
            return false;

        compared += wanted;
    }

    return true;
}

bool StateRamDaysMatchSave(const std::string& save_path, const std::string& state_path)
{
    if (!std::filesystem::exists(state_path) || !std::filesystem::exists(save_path))
        return false;

    return ReadDaysFromEepromFile(save_path) == ReadDaysFromStateRam(state_path);
}

std::string FormatDebugTime()
{
    const std::time_t now = std::time(nullptr);
    std::tm local = {};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif

    std::ostringstream stream;
    stream << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

std::string FormatLocalDate(const std::time_t time)
{
    if (time <= 0)
        return {};

    std::tm local = {};
#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif

    std::ostringstream stream;
    stream << local.tm_mday << "/" << (local.tm_mon + 1) << "/" << (local.tm_year + 1900);
    return stream.str();
}

std::string ReadRtcLastActiveDate(const std::string& save_path)
{
    if (save_path.empty())
        return {};

    std::ifstream file(save_path + ".rtc", std::ios::binary);
    if (!file)
        return {};

    char magic[8] = {};
    int64_t saved_virtual_time = 0;
    int64_t saved_host_time = 0;
    int64_t saved_processed_midnight = 0;
    file.read(magic, sizeof(magic));
    file.read(reinterpret_cast<char*>(&saved_virtual_time), sizeof(saved_virtual_time));
    file.read(reinterpret_cast<char*>(&saved_host_time), sizeof(saved_host_time));
    file.read(reinterpret_cast<char*>(&saved_processed_midnight), sizeof(saved_processed_midnight));

    if (!file || std::string(magic, sizeof(magic)) != "PWRTC002")
        return {};

    return FormatLocalDate(static_cast<std::time_t>(saved_host_time));
}

void AppendRtcDebug(const std::string& save_path, const std::string& message)
{
    if (save_path.empty())
        return;

    const std::filesystem::path save_directory = std::filesystem::path(save_path).parent_path();
    if (!std::filesystem::exists(save_directory / "pocketwalker_enable_debug_log.txt"))
        return;

    const std::filesystem::path log_path = save_directory / "pocketwalker_rtc_debug.log";
    std::ofstream log(log_path, std::ios::app);
    if (!log)
        return;

    log << FormatDebugTime() << " | context | " << message << '\n';
}
}

EmulatorContext::EmulatorContext(const std::string& rom_path, const std::string& save_path,
                                 const ApplicationArguments& args, QObject* parent)
    : QObject(parent), rom_path(rom_path)
{
    this->save_path = save_path;
    rtc_last_active_date = ReadRtcLastActiveDate(this->save_path);

    RomBuffer rom_buffer = {};
    std::ifstream rom_file(rom_path, std::ios::binary);
    rom_file.read(reinterpret_cast<char*>(rom_buffer.data()), 0xC000);

    emu.emplace(rom_buffer);
    loadSave();
    if (!this->save_path.empty())
    {
        const std::string state_path = this->save_path + ".state";
        const bool eeprom_matches = StateEepromMatchesSave(this->save_path, state_path);
        const bool ram_days_match = StateRamDaysMatchSave(this->save_path, state_path);
        AppendRtcDebug(this->save_path, "----- EmulatorContext launch -----");
        AppendRtcDebug(this->save_path, "save_path=" + this->save_path);
        AppendRtcDebug(this->save_path, "state_path=" + state_path);
        AppendRtcDebug(this->save_path, "state_exists=" + std::string(std::filesystem::exists(state_path) ? "true" : "false") +
                                      " save_exists=" + std::string(std::filesystem::exists(this->save_path) ? "true" : "false") +
                                      " eeprom_matches=" + std::string(eeprom_matches ? "true" : "false") +
                                      " ram_days_match=" + std::string(ram_days_match ? "true" : "false") +
                                      " save_days=" + std::to_string(ReadDaysFromEepromFile(this->save_path)) +
                                      " state_days=" + std::to_string(ReadDaysFromStateRam(state_path)) +
                                      " save_total_steps=" + std::to_string(ReadU32BEFromFile(this->save_path, EEPROM_TOTAL_STEPS_OFFSET)) +
                                      " state_total_steps=" + std::to_string(ReadU32BEFromFile(state_path, STATE_RAM_TOTAL_STEPS_OFFSET)) +
                                      " state_session_steps=" + std::to_string(ReadU32BEFromFile(state_path, STATE_RAM_SESSION_STEPS_OFFSET)) +
                                      " save_history=[" + ReadHistoryFromEepromFile(this->save_path) + "]");

        if (eeprom_matches && ram_days_match)
        {
            AppendRtcDebug(this->save_path, "state validation passed; loading state and rtc metadata");
            emu->LoadEmulatorState(state_path);
            emu->LoadRtcState(this->save_path + ".rtc");
            emu->ApplyRtcCatchUpOverflowDays();
            emu->PrepareRtcCatchUp();
        }
        else
        {
            AppendRtcDebug(this->save_path, "state validation failed; save-state/rtc catch-up skipped");
        }
    }

    audio = std::make_unique<QtAudioSystem>();
    emu->OnSamplePushed([this](BuzzerInformation info)
    {
        audio->PushSample(info);
    });

    const auto& ir = AppSettings::instance.ir;
    const bool server_mode = args.server_mode.value_or(ir.mode == IRSettings::Mode::Server);
    const QString host = QString::fromStdString(args.host.value_or(ir.host));
    const quint16 port = args.port.value_or(ir.port);

    network_thread = std::make_unique<QThread>();
    network = std::make_unique<QtNetworkSystem>(*emu, server_mode, host, port, 5);
    network->moveToThread(network_thread.get());
    connect(network_thread.get(), &QThread::started, network.get(), &QtNetworkSystem::start);
    network_thread->start();

    emulator_thread = std::make_unique<std::thread>([this] { emu->Start(); });
}

EmulatorContext::EmulatorContext(const std::string& path, const ApplicationArguments& args, QObject* parent)
    : EmulatorContext(path, path.substr(0, path.find_last_of('.')) + ".sav", args, parent)
{

}

EmulatorContext::~EmulatorContext()
{
    const bool catch_up_interrupted = emu && emu->IsRtcCatchUpActive();

    emu->Stop();
    if (emulator_thread && emulator_thread->joinable())
        emulator_thread->join();

    audio.reset();

    if (network && network_thread && network_thread->isRunning())
    {
        QThread* main_thread = QThread::currentThread();
        QMetaObject::invokeMethod(
            network.get(),
            [n = network.get(), main_thread]() { n->moveToThread(main_thread); },
            Qt::BlockingQueuedConnection);
    }

    if (network_thread)
    {
        network_thread->quit();
        network_thread->wait();
    }

    if (catch_up_interrupted)
    {
        AppendRtcDebug(save_path, "writeSave skipped because startup RTC catch-up was interrupted");
        return;
    }

    writeSave();
}

void EmulatorContext::loadSave()
{
    if (!std::filesystem::exists(save_path))
        return;

    EepromBuffer buf = {};
    std::ifstream f(save_path, std::ios::binary);
    f.read(reinterpret_cast<char*>(buf.data()), buf.size());
    emu->SetEepromBuffer(buf);
}

void EmulatorContext::writeSave()
{
    if (save_path.empty())
        return;

    AppendRtcDebug(save_path, "writeSave begin eeprom_days_before_write=" +
                              std::to_string(emu->GetEepromBuffer()[EEPROM_TOTAL_DAYS_OFFSET] << 8 |
                                             emu->GetEepromBuffer()[EEPROM_TOTAL_DAYS_OFFSET + 1]) +
                              " eeprom_total_steps_before_write=" +
                              std::to_string((static_cast<uint32_t>(emu->GetEepromBuffer()[EEPROM_TOTAL_STEPS_OFFSET]) << 24) |
                                             (static_cast<uint32_t>(emu->GetEepromBuffer()[EEPROM_TOTAL_STEPS_OFFSET + 1]) << 16) |
                                             (static_cast<uint32_t>(emu->GetEepromBuffer()[EEPROM_TOTAL_STEPS_OFFSET + 2]) << 8) |
                                             static_cast<uint32_t>(emu->GetEepromBuffer()[EEPROM_TOTAL_STEPS_OFFSET + 3])));
    const EepromBuffer buf = emu->GetEepromBuffer();
    std::ofstream f(save_path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    emu->SaveEmulatorState(save_path + ".state");
    emu->SaveRtcState(save_path + ".rtc");
    AppendRtcDebug(save_path, "writeSave end save_days=" + std::to_string(ReadDaysFromEepromFile(save_path)) +
                              " state_days=" + std::to_string(ReadDaysFromStateRam(save_path + ".state")) +
                              " save_total_steps=" + std::to_string(ReadU32BEFromFile(save_path, EEPROM_TOTAL_STEPS_OFFSET)) +
                              " state_total_steps=" + std::to_string(ReadU32BEFromFile(save_path + ".state", STATE_RAM_TOTAL_STEPS_OFFSET)) +
                              " state_session_steps=" + std::to_string(ReadU32BEFromFile(save_path + ".state", STATE_RAM_SESSION_STEPS_OFFSET)) +
                              " save_history=[" + ReadHistoryFromEepromFile(save_path) + "]");
}
