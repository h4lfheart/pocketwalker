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
#include <vector>
#include "desktop/src/qt/settings/app_settings.h"

namespace
{
using ByteBuffer = std::vector<uint8_t>;

constexpr std::array<char, 8> PWSAV_MAGIC = {'P', 'W', 'S', 'A', 'V', '0', '0', '1'};
constexpr std::array<char, 4> PWSAV_CHUNK_EEPROM = {'E', 'E', 'P', '1'};
constexpr std::array<char, 4> PWSAV_CHUNK_RTC = {'R', 'T', 'C', '1'};
constexpr std::array<char, 4> PWSAV_CHUNK_STATE = {'S', 'T', 'A', '1'};
constexpr uint64_t PWSAV_MAX_CHUNK_SIZE = 64ULL * 1024ULL * 1024ULL;
constexpr std::streamoff STATE_EEPROM_OFFSET = 44;
constexpr std::streamoff STATE_RAM_TOTAL_STEPS_OFFSET = 0x100BB;
constexpr std::streamoff STATE_RAM_TOTAL_DAYS_OFFSET = 0x100C7;
constexpr std::streamoff STATE_RAM_SESSION_STEPS_OFFSET = 0x100D7;
constexpr size_t EEPROM_TOTAL_STEPS_OFFSET = 0x156;
constexpr size_t EEPROM_TOTAL_DAYS_OFFSET = 0x162;
constexpr size_t EEPROM_HISTORY_STEPS_OFFSET = 0xCEF0;
constexpr size_t EEPROM_HISTORY_DAYS = 7;

std::string FormatLocalDate(std::time_t time);

template <typename T>
void WritePwsavValue(std::ostream& stream, const T& value)
{
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool ReadPwsavValue(std::istream& stream, T& value)
{
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(stream);
}

std::filesystem::path PwsavPathForSavePath(const std::string& save_path)
{
    std::filesystem::path path(save_path);
    if (path.extension() != ".pwsav")
        path.replace_extension(".pwsav");
    return path;
}

uint32_t ReadU32BEFromBytes(const ByteBuffer& bytes, const size_t offset)
{
    if (offset + sizeof(uint32_t) > bytes.size())
        return 0;

    return (static_cast<uint32_t>(bytes[offset]) << 24) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
           static_cast<uint32_t>(bytes[offset + 3]);
}

uint16_t ReadU16BEFromBytes(const ByteBuffer& bytes, const size_t offset)
{
    if (offset + sizeof(uint16_t) > bytes.size())
        return 0;

    return static_cast<uint16_t>((bytes[offset] << 8) | bytes[offset + 1]);
}

uint16_t ReadDaysFromEepromBuffer(const EepromBuffer& buffer)
{
    return static_cast<uint16_t>((buffer[EEPROM_TOTAL_DAYS_OFFSET] << 8) |
                                 buffer[EEPROM_TOTAL_DAYS_OFFSET + 1]);
}

uint16_t ReadDaysFromStateBytes(const ByteBuffer& state)
{
    return ReadU16BEFromBytes(state, static_cast<size_t>(STATE_RAM_TOTAL_DAYS_OFFSET));
}

uint32_t ReadU32BEFromEepromBuffer(const EepromBuffer& buffer, const size_t offset)
{
    if (offset + sizeof(uint32_t) > buffer.size())
        return 0;

    return (static_cast<uint32_t>(buffer[offset]) << 24) |
           (static_cast<uint32_t>(buffer[offset + 1]) << 16) |
           (static_cast<uint32_t>(buffer[offset + 2]) << 8) |
           static_cast<uint32_t>(buffer[offset + 3]);
}

std::string ReadHistoryFromEepromBuffer(const EepromBuffer& buffer)
{
    std::ostringstream stream;
    for (size_t i = 0; i < EEPROM_HISTORY_DAYS; i++)
    {
        if (i > 0)
            stream << ",";

        stream << "-" << (i + 1) << "="
               << ReadU32BEFromEepromBuffer(buffer, EEPROM_HISTORY_STEPS_OFFSET + i * sizeof(uint32_t));
    }

    return stream.str();
}

struct PwsavData
{
    bool has_eeprom = false;
    bool has_rtc = false;
    bool has_state = false;
    EepromBuffer eeprom = {};
    ByteBuffer rtc = {};
    ByteBuffer state = {};
};

bool ReadPwsavFile(const std::filesystem::path& path, PwsavData& data)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;

    std::array<char, PWSAV_MAGIC.size()> magic = {};
    file.read(magic.data(), magic.size());
    if (!file || magic != PWSAV_MAGIC)
        return false;

    while (file.peek() != std::char_traits<char>::eof())
    {
        std::array<char, 4> chunk_id = {};
        uint64_t chunk_size = 0;
        file.read(chunk_id.data(), chunk_id.size());
        if (!ReadPwsavValue(file, chunk_size))
            return false;

        if (chunk_size > PWSAV_MAX_CHUNK_SIZE)
            return false;

        ByteBuffer chunk(static_cast<size_t>(chunk_size));
        if (!chunk.empty())
            file.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
        if (!file)
            return false;

        if (chunk_id == PWSAV_CHUNK_EEPROM)
        {
            if (chunk.size() != data.eeprom.size())
                return false;
            std::copy(chunk.begin(), chunk.end(), data.eeprom.begin());
            data.has_eeprom = true;
        }
        else if (chunk_id == PWSAV_CHUNK_RTC)
        {
            data.rtc = std::move(chunk);
            data.has_rtc = true;
        }
        else if (chunk_id == PWSAV_CHUNK_STATE)
        {
            data.state = std::move(chunk);
            data.has_state = true;
        }
    }

    return data.has_eeprom;
}

void WritePwsavChunk(std::ostream& stream, const std::array<char, 4>& chunk_id, const uint8_t* data, const size_t size)
{
    stream.write(chunk_id.data(), chunk_id.size());
    WritePwsavValue(stream, static_cast<uint64_t>(size));
    if (size > 0)
        stream.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
}

void WritePwsavChunk(std::ostream& stream, const std::array<char, 4>& chunk_id, const std::string& data)
{
    WritePwsavChunk(stream, chunk_id, reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

bool StateEepromMatchesSave(const EepromBuffer& save, const ByteBuffer& state)
{
    if (state.size() < static_cast<size_t>(STATE_EEPROM_OFFSET) + save.size())
        return false;

    return std::equal(save.begin(), save.end(), state.begin() + static_cast<std::ptrdiff_t>(STATE_EEPROM_OFFSET));
}

bool StateRamDaysMatchSave(const EepromBuffer& save, const ByteBuffer& state)
{
    return ReadDaysFromEepromBuffer(save) == ReadDaysFromStateBytes(state);
}

std::string ReadRtcLastActiveDateFromBytes(const ByteBuffer& rtc)
{
    if (rtc.size() < 32)
        return {};

    const std::string magic(reinterpret_cast<const char*>(rtc.data()), 8);
    if (magic != "PWRTC002")
        return {};

    int64_t saved_host_time = 0;
    std::copy_n(rtc.data() + 16, sizeof(saved_host_time), reinterpret_cast<uint8_t*>(&saved_host_time));
    return FormatLocalDate(static_cast<std::time_t>(saved_host_time));
}

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

    PwsavData pwsav = {};
    if (ReadPwsavFile(PwsavPathForSavePath(save_path), pwsav) && pwsav.has_rtc)
        return ReadRtcLastActiveDateFromBytes(pwsav.rtc);

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

bool LoadPwsavRuntimeState(PocketWalker& emu, const std::string& save_path)
{
    PwsavData pwsav = {};
    const std::filesystem::path pwsav_path = PwsavPathForSavePath(save_path);
    if (!ReadPwsavFile(pwsav_path, pwsav) || !pwsav.has_state || !pwsav.has_rtc)
        return false;

    const bool eeprom_matches = StateEepromMatchesSave(pwsav.eeprom, pwsav.state);
    const bool ram_days_match = StateRamDaysMatchSave(pwsav.eeprom, pwsav.state);
    AppendRtcDebug(save_path, "pwsav_path=" + pwsav_path.string());
    AppendRtcDebug(save_path, "pwsav_exists=true eeprom_matches=" + std::string(eeprom_matches ? "true" : "false") +
                              " ram_days_match=" + std::string(ram_days_match ? "true" : "false") +
                              " save_days=" + std::to_string(ReadDaysFromEepromBuffer(pwsav.eeprom)) +
                              " state_days=" + std::to_string(ReadDaysFromStateBytes(pwsav.state)) +
                              " save_total_steps=" + std::to_string(ReadU32BEFromEepromBuffer(pwsav.eeprom, EEPROM_TOTAL_STEPS_OFFSET)) +
                              " state_total_steps=" + std::to_string(ReadU32BEFromBytes(pwsav.state, static_cast<size_t>(STATE_RAM_TOTAL_STEPS_OFFSET))) +
                              " state_session_steps=" + std::to_string(ReadU32BEFromBytes(pwsav.state, static_cast<size_t>(STATE_RAM_SESSION_STEPS_OFFSET))) +
                              " save_history=[" + ReadHistoryFromEepromBuffer(pwsav.eeprom) + "]");

    if (!eeprom_matches || !ram_days_match)
    {
        AppendRtcDebug(save_path, "pwsav state validation failed; save-state/rtc catch-up skipped");
        return false;
    }

    std::string state_bytes(reinterpret_cast<const char*>(pwsav.state.data()), pwsav.state.size());
    std::istringstream state_stream(state_bytes, std::ios::in | std::ios::binary);
    if (!emu.LoadEmulatorState(state_stream))
    {
        AppendRtcDebug(save_path, "pwsav emulator state load failed");
        return false;
    }

    std::string rtc_bytes(reinterpret_cast<const char*>(pwsav.rtc.data()), pwsav.rtc.size());
    std::istringstream rtc_stream(rtc_bytes, std::ios::in | std::ios::binary);
    emu.LoadRtcState(rtc_stream, pwsav_path.parent_path());
    emu.ApplyRtcCatchUpOverflowDays();
    emu.PrepareRtcCatchUp();
    AppendRtcDebug(save_path, "pwsav state validation passed; loaded state and rtc metadata");
    return true;
}

bool WritePwsavFile(const std::string& save_path, const PocketWalker& emu)
{
    const std::filesystem::path pwsav_path = PwsavPathForSavePath(save_path);
    const std::filesystem::path temp_path = pwsav_path.string() + ".tmp";
    const std::filesystem::path save_directory = pwsav_path.parent_path();

    const EepromBuffer eeprom = emu.GetEepromBuffer();

    std::ostringstream rtc_stream(std::ios::out | std::ios::binary);
    emu.SaveRtcState(rtc_stream, save_directory);
    const std::string rtc_bytes = rtc_stream.str();

    std::ostringstream state_stream(std::ios::out | std::ios::binary);
    emu.SaveEmulatorState(state_stream);
    const std::string state_bytes = state_stream.str();

    std::ofstream file(temp_path, std::ios::binary);
    if (!file)
        return false;

    file.write(PWSAV_MAGIC.data(), PWSAV_MAGIC.size());
    WritePwsavChunk(file, PWSAV_CHUNK_EEPROM, eeprom.data(), eeprom.size());
    WritePwsavChunk(file, PWSAV_CHUNK_RTC, rtc_bytes);
    WritePwsavChunk(file, PWSAV_CHUNK_STATE, state_bytes);
    file.close();

    if (!file)
    {
        std::error_code ignored;
        std::filesystem::remove(temp_path, ignored);
        return false;
    }

    std::error_code error;
    std::filesystem::remove(pwsav_path, error);
    error.clear();
    std::filesystem::rename(temp_path, pwsav_path, error);
    if (error)
    {
        std::filesystem::remove(temp_path, error);
        return false;
    }

    return true;
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
        AppendRtcDebug(this->save_path, "----- EmulatorContext launch -----");
        AppendRtcDebug(this->save_path, "save_path=" + this->save_path);

        if (!LoadPwsavRuntimeState(*emu, this->save_path))
        {
            const std::string state_path = this->save_path + ".state";
            const bool eeprom_matches = StateEepromMatchesSave(this->save_path, state_path);
            const bool ram_days_match = StateRamDaysMatchSave(this->save_path, state_path);
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
                AppendRtcDebug(this->save_path, "sidecar state validation passed; loading state and rtc metadata");
                emu->LoadEmulatorState(state_path);
                emu->LoadRtcState(this->save_path + ".rtc");
                emu->ApplyRtcCatchUpOverflowDays();
                emu->PrepareRtcCatchUp();
            }
            else
            {
                AppendRtcDebug(this->save_path, "sidecar state validation failed; save-state/rtc catch-up skipped");
            }
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
    PwsavData pwsav = {};
    if (!save_path.empty() && ReadPwsavFile(PwsavPathForSavePath(save_path), pwsav) && pwsav.has_eeprom)
    {
        emu->SetEepromBuffer(pwsav.eeprom);
        return;
    }

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
    const bool saved = WritePwsavFile(save_path, *emu);
    AppendRtcDebug(save_path, "writeSave end pwsav_saved=" + std::string(saved ? "true" : "false") +
                              " pwsav_path=" + PwsavPathForSavePath(save_path).string() +
                              " eeprom_days=" + std::to_string(ReadDaysFromEepromBuffer(buf)) +
                              " eeprom_total_steps=" + std::to_string(ReadU32BEFromEepromBuffer(buf, EEPROM_TOTAL_STEPS_OFFSET)) +
                              " eeprom_history=[" + ReadHistoryFromEepromBuffer(buf) + "]");
}
