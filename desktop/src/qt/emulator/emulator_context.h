#pragma once

#include <memory>
#include <thread>
#include <string>
#include <optional>
#include <QObject>
#include <QThread>
#include "core/pokewalker/pocketwalker.h"
#include "desktop/src/qt/audio/qt_audio_system.h"
#include "desktop/src/qt/network/qt_network_system.h"
#include "desktop/src/qt/application_args.h"

class EmulatorContext : public QObject
{
    Q_OBJECT

public:
    explicit EmulatorContext(const std::string& rom_path, const std::string& save_path,
                             const ApplicationArguments& args = {}, QObject* parent = nullptr);
    explicit EmulatorContext(const std::string& rom_path,
                             const ApplicationArguments& args = {}, QObject* parent = nullptr);
    ~EmulatorContext() override;

    PocketWalker& emulator() { return *emu; }
    const std::string& savePath() const { return save_path; }
    const std::string& romPath() const { return rom_path; }

private:
    void loadSave();
    void writeSave();

    std::string rom_path;
    std::string save_path;
    std::optional<PocketWalker> emu;
    std::unique_ptr<QtAudioSystem> audio;
    std::unique_ptr<QtNetworkSystem> network;
    std::unique_ptr<QThread> network_thread;
    std::unique_ptr<std::thread> emulator_thread;
};
