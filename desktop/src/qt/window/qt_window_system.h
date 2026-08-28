#pragma once
#include <memory>
#include <QMainWindow>
#include <QKeyEvent>
#include <QTimer>

#include "desktop/src/qt/application_args.h"
#include "desktop/src/qt/widget/display_widget.h"
#include "desktop/src/qt/emulator/emulator_context.h"

class DisplayWidget;

class QtWindowSystem : public QMainWindow
{
    Q_OBJECT

public:
    explicit QtWindowSystem(ApplicationArguments args, QWidget* parent = nullptr);
    ~QtWindowSystem() override;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void openROM();
    void openRecentROM(const QString& path);
    void importSave();
    void resetEmulator();

private:
    void launchEmulator(const std::string& rom_path, const std::string& save_path = "");
    void shutdownEmulator();
    void addToRecentROMs(const std::string& path);
    void updateRecentROMsMenu();
    void setEmulatorActionsEnabled(bool enabled);
    void releaseHeldInputs();
    void applyTheme();
    void setBypassPowerSave();

    ApplicationArguments args;

    std::unique_ptr<EmulatorContext> context;
    DisplayWidget* display = nullptr;
    QTimer* render_timer = nullptr;
    QMenu* recent_roms_menu = nullptr;
    QAction* import_save_action = nullptr;

    QAction* pause_action = nullptr;
    QAction* reset_action = nullptr;
    QAction* stop_action = nullptr;
    QAction* synthetic_steps_action = nullptr;
    QAction* set_watts_action = nullptr;
    QAction* set_session_steps_action = nullptr;
};
