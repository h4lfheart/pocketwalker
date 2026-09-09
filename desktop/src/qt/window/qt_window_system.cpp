#include "qt_window_system.h"
#include <algorithm>
#include <fstream>
#include <QMenuBar>
#include <QTimer>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QStackedWidget>
#include <QStyleFactory>
#include <QStyleHints>
#include <QVBoxLayout>


#include "../dialog/settings/control_settings_dialog.h"
#include "../dialog/settings/emulation_settings_dialog.h"
#include "../dialog/settings/general_settings_dialog.h"
#include "../dialog/settings/ir_settings_dialog.h"
#include "desktop/src/qt/dialog/about_dialog.h"
#include "../dialog/emulator/set_watts_dialog.h"
#include "desktop/src/qt/dialog/set_session_steps_dialog.h"
#include "desktop/src/qt/dialog/settings/audio_settings_dialog.h"
#include "desktop/src/qt/settings/app_settings.h"

namespace
{
constexpr bool SHUTDOWN_SETTLE_ENABLED = false;
constexpr int SHUTDOWN_SETTLE_SECONDS = 15;
}

QtWindowSystem::QtWindowSystem(ApplicationArguments args, QWidget* parent)
    : QMainWindow(parent), args(args)
{
    setWindowTitle("PocketWalker");
    if (!args.no_menu)
    {
    menuBar()->setNativeMenuBar(true);
#if WIN32
    if (QApplication::style()->name() == "windows11")
        setStyleSheet("QMenuBar::item { padding: 4px 8px; }");
#endif


    auto* file_menu = menuBar()->addMenu("File");
    auto* open_action = file_menu->addAction("Open ROM");
    open_action->setShortcut(QKeySequence::Open);
    connect(open_action, &QAction::triggered, this, &QtWindowSystem::openROM);

    recent_roms_menu = file_menu->addMenu("Recent ROMs");
    updateRecentROMsMenu();

    file_menu->addSeparator();
    import_save_action = file_menu->addAction("Import Save");
    import_save_action->setEnabled(false);
    connect(import_save_action, &QAction::triggered, this, &QtWindowSystem::importSave);

    file_menu->addSeparator();
    connect(file_menu->addAction("Exit"), &QAction::triggered, qApp, &QApplication::quit);

    auto* system_menu = menuBar()->addMenu("System");

    pause_action = system_menu->addAction("Pause");
    pause_action->setCheckable(true);
    pause_action->setEnabled(false);

    connect(pause_action, &QAction::toggled, this, [this](bool enabled)
    {
        context->emulator().SetPause(enabled);
    });

    reset_action = system_menu->addAction("Reset");
    reset_action->setEnabled(false);
    connect(reset_action, &QAction::triggered, this, &QtWindowSystem::resetEmulator);

    stop_action = system_menu->addAction("Stop");
    stop_action->setEnabled(false);
    connect(stop_action, &QAction::triggered, this, &QtWindowSystem::shutdownEmulator);

    system_menu->addSeparator();

    set_watts_action = system_menu->addAction("Set Watts");
    set_watts_action->setEnabled(false);

    connect(set_watts_action, &QAction::triggered, this, [this]
    {
        auto* dlg = new SetWattsDialog(this);
        if (const auto result = dlg->exec(); result == QDialog::Accepted)
        {
            context->emulator().SetWatts(dlg->watts());
        }
    });

    set_session_steps_action = system_menu->addAction("Set Session Steps");
    set_session_steps_action->setEnabled(false);

    connect(set_session_steps_action, &QAction::triggered, this, [this]
    {
        auto* dlg = new SetSessionStepsDialog(this);
        if (const auto result = dlg->exec(); result == QDialog::Accepted)
        {
            context->emulator().SetSessionSteps(dlg->steps());
        }
    });

    system_menu->addSeparator();

    synthetic_steps_action = system_menu->addAction("Use Synthetic Steps");
    synthetic_steps_action->setCheckable(true);
    synthetic_steps_action->setChecked(false);
    synthetic_steps_action->setEnabled(false);

    connect(synthetic_steps_action, &QAction::toggled, this, [this](bool enabled)
    {
        context->emulator().UseSyntheticSteps(enabled);
    });

    auto* settings_menu = menuBar()->addMenu("Settings");
    auto general_settings_action = settings_menu->addAction("General");
    connect(general_settings_action, &QAction::triggered, this, [this]
    {
        auto* dlg = new GeneralSettingsDialog(this);
        connect(dlg, &GeneralSettingsDialog::themeChanged, this, &QtWindowSystem::applyTheme);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->exec();
    });

    auto* emulation_settings_action = settings_menu->addAction("Emulation");
    connect(emulation_settings_action, &QAction::triggered, this, [this]
    {
        auto* dlg = new EmulationSettingsDialog(this);
        connect(dlg, &EmulationSettingsDialog::bypassPowerSaveChanged, this, &QtWindowSystem::setBypassPowerSave);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->exec();
    });

    auto* audio_settings_action = settings_menu->addAction("Audio");
    connect(audio_settings_action, &QAction::triggered, this, [this]
    {
        auto* dlg = new AudioSettingsDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->exec();
    });

    auto* controls_settings_action = settings_menu->addAction("Controls");
    connect(controls_settings_action, &QAction::triggered, this, [this]
    {
        auto* dlg = new ControlSettingsDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->exec();
    });

    auto ir_settings_action = settings_menu->addAction("IR");
    connect(ir_settings_action, &QAction::triggered, this, [this]
    {
        auto* dlg = new IRSettingsDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->exec();
    });

    auto* about_menu = menuBar()->addAction("About");
    connect(about_menu, &QAction::triggered, this, [this]
    {
        auto* dlg = new AboutDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->exec();
    });
    }

    central_stack = new QStackedWidget(this);
    display = new DisplayWidget(this);

    startup_catch_up_widget = new QWidget(this);
    auto* startup_catch_up_layout = new QVBoxLayout(startup_catch_up_widget);
    startup_catch_up_layout->setContentsMargins(24, 24, 24, 24);
    startup_catch_up_layout->setSpacing(12);

    startup_catch_up_label = new QLabel("Updating Pokéwalker! Please wait...", startup_catch_up_widget);
    startup_catch_up_label->setAlignment(Qt::AlignCenter);
    startup_catch_up_label->setStyleSheet("font-weight: 600;");
    startup_catch_up_progress = new QProgressBar(startup_catch_up_widget);
    startup_catch_up_progress->setTextVisible(false);

    startup_catch_up_layout->addStretch();
    startup_catch_up_layout->addWidget(startup_catch_up_label);
    startup_catch_up_layout->addWidget(startup_catch_up_progress);
    startup_catch_up_layout->addStretch();

    central_stack->addWidget(display);
    central_stack->addWidget(startup_catch_up_widget);
    setCentralWidget(central_stack);
    central_stack->setCurrentWidget(display);
    adjustSize();

    render_timer = new QTimer(this);
    connect(render_timer, &QTimer::timeout, display, QOverload<>::of(&QWidget::update));
    render_timer->start(16);

    shutdown_settle_timer = new QTimer(this);
    shutdown_settle_timer->setInterval(1000);
    connect(shutdown_settle_timer, &QTimer::timeout, this, &QtWindowSystem::updateSettledShutdownCountdown);

    startup_catch_up_timer = new QTimer(this);
    startup_catch_up_timer->setInterval(50);
    connect(startup_catch_up_timer, &QTimer::timeout, this, &QtWindowSystem::updateStartupCatchUp);

    applyTheme();

    if (args.rom_path.has_value())
    {
        const QFileInfo rom_file(QString::fromStdString(*args.rom_path));
        if (!rom_file.exists() || !rom_file.isFile())
        {
            Log::Warn("Invalid ROM path: {}", *args.rom_path);
            return;
        }

        std::string save_path;
        if (args.save_path)
        {
            const QFileInfo save_file(QString::fromStdString(*args.save_path));
            if (!save_file.exists())
            {
                Log::Warn("Invalid save path: {}", *args.save_path);
                return;
            }
            save_path = save_file.absoluteFilePath().toStdString();
        }

        launchEmulator(rom_file.absoluteFilePath().toStdString(), save_path);
        if (args.test_auto_close_ms.has_value())
        {
            QTimer::singleShot(static_cast<int>(*args.test_auto_close_ms), this, [this]
            {
                shutdownEmulator();
                qApp->quit();
            });
        }
    }
    else
    {
        const auto& general = AppSettings::instance.general;
        if (general.boot_on_launch && !general.default_rom.empty())
            launchEmulator(general.default_rom);
    }
}

QtWindowSystem::~QtWindowSystem()
{
    if (shutdown_settle_timer)
        shutdown_settle_timer->stop();
    if (startup_catch_up_timer)
        startup_catch_up_timer->stop();
    shutdownEmulator();
}

void QtWindowSystem::openROM()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Open ROM", QString(), "ROM Files (*.bin *.rom);;All Files (*)");

    if (path.isEmpty())
        return;

    launchEmulator(path.toStdString());
}

void QtWindowSystem::openRecentROM(const QString& path)
{
    if (!QFileInfo::exists(path))
    {
        QMessageBox::warning(this, "File Not Found",
            QString("Could not find:\n%1\n\nIt will be removed from the recent list.").arg(path));

        auto& recent = AppSettings::instance.general.recent_roms;
        std::erase(recent, path.toStdString());
        updateRecentROMsMenu();
        return;
    }

    launchEmulator(path.toStdString());
}

void QtWindowSystem::importSave()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Import Save", QString(), "Save Files (*.sav *.bin);;All Files (*)");

    if (path.isEmpty())
        return;

    const auto result = QMessageBox::warning(
        this, "Import Save",
        "Importing a new save file will overwrite your existing save. Are you sure you want to continue?",
        QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes)
        return;

    const std::string rom_path = context->romPath();
    const std::string save_path = context->savePath();

    shutdownEmulator();

    std::ifstream src(path.toStdString(), std::ios::binary);
    std::ofstream dst(save_path, std::ios::binary);
    dst << src.rdbuf();

    launchEmulator(rom_path);
}

void QtWindowSystem::resetEmulator()
{
    if (!context)
        return;

    const std::string path = context->romPath();
    launchEmulator(path);

    if (synthetic_steps_action)
        context->emulator().UseSyntheticSteps(synthetic_steps_action->isChecked());
}

void QtWindowSystem::launchEmulator(const std::string& rom_path, const std::string& save_path)
{
    shutdownEmulator();
    addToRecentROMs(rom_path);

    if (save_path.empty())
        context = std::make_unique<EmulatorContext>(rom_path, args, this);
    else
        context = std::make_unique<EmulatorContext>(rom_path, save_path, args, this);

    display->setEmulator(&context->emulator());
    setEmulatorActionsEnabled(true);
    render_timer->start(16);

    const std::string filename = rom_path.substr(rom_path.find_last_of("/\\") + 1);
    setWindowTitle(QString("PocketWalker - %1").arg(QString::fromStdString(filename)));

    setBypassPowerSave();

    if (context->emulator().IsRtcCatchUpActive())
        beginStartupCatchUp();
    else if (central_stack)
        central_stack->setCurrentWidget(display);
}

void QtWindowSystem::shutdownEmulator()
{
    if (!context)
        return;

    if (startup_catch_up_timer)
        startup_catch_up_timer->stop();
    startup_catch_up_active = false;

    render_timer->stop();
    display->setEmulator(nullptr);
    if (central_stack)
        central_stack->setCurrentWidget(display);
    display->update();
    context.reset();
    setEmulatorActionsEnabled(false);
    setWindowTitle("PocketWalker");
}

void QtWindowSystem::setEmulatorActionsEnabled(bool enabled)
{
    if (shutdown_settle_active || isStartupCatchUpActive())
        enabled = false;

    if (import_save_action) import_save_action->setEnabled(enabled);
    if (reset_action) reset_action->setEnabled(enabled);
    if (pause_action) pause_action->setEnabled(enabled);
    if (stop_action) stop_action->setEnabled(enabled);
    if (synthetic_steps_action) synthetic_steps_action->setEnabled(enabled);
    if (set_watts_action) set_watts_action->setEnabled(enabled);
    if (set_session_steps_action) set_session_steps_action->setEnabled(enabled);
}

void QtWindowSystem::releaseHeldInputs()
{
    if (!context)
        return;

    context->emulator().ReleaseButton(ButtonType::LEFT);
    context->emulator().ReleaseButton(ButtonType::RIGHT);
    context->emulator().ReleaseButton(ButtonType::CENTER);
    context->emulator().UseFastMode(false);
    context->emulator().UseSyntheticSteps(false);

    if (synthetic_steps_action)
        synthetic_steps_action->setChecked(false);
}

void QtWindowSystem::beginSettledShutdown()
{
    if (!context || shutdown_settle_active)
        return;

    shutdown_settle_active = true;
    shutdown_settle_seconds_remaining = SHUTDOWN_SETTLE_SECONDS;

    releaseHeldInputs();
    context->emulator().UseFastMode(false);
    context->emulator().SetPause(false);

    if (pause_action)
        pause_action->setChecked(false);

    setEmulatorActionsEnabled(false);

    shutdown_wait_dialog = new QMessageBox(this);
    shutdown_wait_dialog->setAttribute(Qt::WA_DeleteOnClose);
    shutdown_wait_dialog->setWindowTitle("PocketWalker is shutting down");
    shutdown_wait_dialog->setIcon(QMessageBox::Information);
    shutdown_wait_dialog->setModal(false);
    shutdown_wait_dialog->addButton("Close Now", QMessageBox::AcceptRole);
    shutdown_wait_dialog->setText(QString("Waiting for the game to idle so it can shut down!\n\nSeconds [%1]")
        .arg(shutdown_settle_seconds_remaining));
    connect(shutdown_wait_dialog, &QMessageBox::buttonClicked, this, [this]
    {
        if (shutdown_settle_active)
            finishSettledShutdown();
    });
    shutdown_wait_dialog->show();

    shutdown_settle_timer->start();
    QTimer::singleShot(SHUTDOWN_SETTLE_SECONDS * 1000 + 250, this, [this]
    {
        if (shutdown_settle_active)
            finishSettledShutdown();
    });
}

void QtWindowSystem::updateSettledShutdownCountdown()
{
    if (!shutdown_settle_active)
        return;

    --shutdown_settle_seconds_remaining;

    if (shutdown_settle_seconds_remaining <= 0)
    {
        finishSettledShutdown();
        return;
    }

    if (shutdown_wait_dialog)
    {
        shutdown_wait_dialog->setText(QString("Waiting for the game to idle so it can shut down!\n\nSeconds [%1]")
            .arg(shutdown_settle_seconds_remaining));
    }
}

void QtWindowSystem::finishSettledShutdown()
{
    if (shutdown_settle_timer)
        shutdown_settle_timer->stop();

    if (shutdown_wait_dialog)
    {
        shutdown_wait_dialog->close();
        shutdown_wait_dialog = nullptr;
    }

    shutdown_settle_active = false;
    shutdownEmulator();
    qApp->quit();
}

void QtWindowSystem::beginStartupCatchUp()
{
    if (!context || startup_catch_up_active)
        return;

    startup_catch_up_active = true;
    setEmulatorActionsEnabled(false);

    if (startup_catch_up_label)
    {
        QString text = "Updating Pokéwalker! Please wait...";
        const std::string& last_active_date = context->rtcLastActiveDate();
        if (!last_active_date.empty())
        {
            text += "\n\nLast active: ";
            text += QString::fromStdString(last_active_date);
        }
        startup_catch_up_label->setText(text);
    }

    if (central_stack)
        central_stack->setCurrentWidget(startup_catch_up_widget);

    if (startup_catch_up_progress)
    {
        startup_catch_up_progress->setMinimum(0);
        startup_catch_up_progress->setMaximum(static_cast<int>(std::max<size_t>(
            context->emulator().RtcCatchUpMidnightsTotal(), 1)));
        startup_catch_up_progress->setValue(0);
    }

    startup_catch_up_timer->start();
    updateStartupCatchUp();
}

void QtWindowSystem::updateStartupCatchUp()
{
    if (!context || !startup_catch_up_active)
        return;

    const size_t total = context->emulator().RtcCatchUpMidnightsTotal();
    const size_t completed = context->emulator().RtcCatchUpMidnightsCompleted();

    if (startup_catch_up_progress)
    {
        startup_catch_up_progress->setMaximum(static_cast<int>(std::max<size_t>(total, 1)));
        startup_catch_up_progress->setValue(static_cast<int>(std::min(completed, total)));
    }

    if (context->emulator().IsRtcCatchUpActive())
        return;

    startup_catch_up_timer->stop();
    startup_catch_up_active = false;

    if (central_stack)
        central_stack->setCurrentWidget(display);

    setEmulatorActionsEnabled(true);
}

bool QtWindowSystem::isStartupCatchUpActive() const
{
    return startup_catch_up_active;
}

void QtWindowSystem::addToRecentROMs(const std::string& path)
{
    auto& recent = AppSettings::instance.general.recent_roms;
    std::erase(recent, path);
    recent.insert(recent.begin(), path);
    if (recent.size() > MAX_RECENT_ROMS)
        recent.resize(MAX_RECENT_ROMS);

    updateRecentROMsMenu();
}

void QtWindowSystem::updateRecentROMsMenu()
{
    if (!recent_roms_menu)
        return;

    recent_roms_menu->clear();

    const auto& recent = AppSettings::instance.general.recent_roms;

    if (recent.empty())
    {
        recent_roms_menu->addAction("(empty)")->setEnabled(false);
        return;
    }

    for (const std::string& path : recent)
    {
        const QString qpath = QString::fromStdString(path);
        const QAction* action = recent_roms_menu->addAction(qpath);
        connect(action, &QAction::triggered, this, [this, qpath]
        {
            openRecentROM(qpath);
        });
    }

    recent_roms_menu->addSeparator();
    connect(recent_roms_menu->addAction("Clear Recent ROMs"), &QAction::triggered, this, [this]
    {
        AppSettings::instance.general.recent_roms.clear();
        updateRecentROMsMenu();
    });
}

void QtWindowSystem::applyTheme()
{
    switch (AppSettings::instance.general.theme)
    {
    case GeneralSettings::AppTheme::Light:
        qApp->styleHints()->setColorScheme(Qt::ColorScheme::Light);
        break;
    case GeneralSettings::AppTheme::Dark:
        qApp->styleHints()->setColorScheme(Qt::ColorScheme::Dark);
        break;
    case GeneralSettings::AppTheme::System:
        qApp->styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
        break;
    }
}

void QtWindowSystem::setBypassPowerSave()
{
    if (context)
        context->emulator().SetBypassPowerSave(AppSettings::instance.emulation.bypass_power_save);
}

void QtWindowSystem::keyPressEvent(QKeyEvent* event)
{
    if (shutdown_settle_active || isStartupCatchUpActive())
    {
        event->ignore();
        return;
    }

    if (!context)
    {
        QMainWindow::keyPressEvent(event);
        return;
    }

    const auto& controls = AppSettings::instance.controls;
    const int key = event->key();
    const bool is_control_key =
        key == controls.key_left ||
        key == controls.key_right ||
        key == controls.key_center ||
        key == controls.key_speedup ||
        key == controls.key_synthetic_steps_hold;

    if (event->isAutoRepeat() && is_control_key)
    {
        event->accept();
        return;
    }

    if (key == controls.key_left) context->emulator().PressButton(ButtonType::LEFT);
    else if (key == controls.key_right) context->emulator().PressButton(ButtonType::RIGHT);
    else if (key == controls.key_center) context->emulator().PressButton(ButtonType::CENTER);
    else if (key == controls.key_speedup) context->emulator().UseFastMode(true);
    else if (key == controls.key_synthetic_steps_hold)
    {
        context->emulator().UseSyntheticSteps(true);
        if (synthetic_steps_action)
            synthetic_steps_action->setChecked(true);
    }
    else QMainWindow::keyPressEvent(event);
}

void QtWindowSystem::keyReleaseEvent(QKeyEvent* event)
{
    if (shutdown_settle_active || isStartupCatchUpActive())
    {
        event->ignore();
        return;
    }

    if (!context)
    {
        QMainWindow::keyReleaseEvent(event);
        return;
    }

    const auto& controls = AppSettings::instance.controls;
    const int key = event->key();
    const bool is_control_key =
        key == controls.key_left ||
        key == controls.key_right ||
        key == controls.key_center ||
        key == controls.key_speedup ||
        key == controls.key_synthetic_steps_hold;

    if (event->isAutoRepeat() && is_control_key)
    {
        event->accept();
        return;
    }

    if (key == controls.key_left) context->emulator().ReleaseButton(ButtonType::LEFT);
    else if (key == controls.key_right) context->emulator().ReleaseButton(ButtonType::RIGHT);
    else if (key == controls.key_center) context->emulator().ReleaseButton(ButtonType::CENTER);
    else if (key == controls.key_speedup) context->emulator().UseFastMode(false);
    else if (key == controls.key_synthetic_steps_hold)
    {
        context->emulator().UseSyntheticSteps(false);
        if (synthetic_steps_action)
            synthetic_steps_action->setChecked(false);
    }
    else QMainWindow::keyReleaseEvent(event);
}

void QtWindowSystem::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::ActivationChange && !isActiveWindow())
        releaseHeldInputs();

    QMainWindow::changeEvent(event);
}

void QtWindowSystem::closeEvent(QCloseEvent* event)
{
    if (SHUTDOWN_SETTLE_ENABLED && context && !shutdown_settle_active)
    {
        beginSettledShutdown();
        event->ignore();
        return;
    }

    releaseHeldInputs();
    shutdownEmulator();
    event->accept();
}
