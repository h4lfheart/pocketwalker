#include "qt_window_system.h"
#include <fstream>
#include <QMenuBar>
#include <QTimer>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStyleFactory>
#include <QStyleHints>


#include "../dialog/settings/control_settings_dialog.h"
#include "../dialog/settings/emulation_settings_dialog.h"
#include "../dialog/settings/general_settings_dialog.h"
#include "../dialog/settings/ir_settings_dialog.h"
#include "desktop/src/qt/dialog/about_dialog.h"
#include "../dialog/emulator/set_watts_dialog.h"
#include "desktop/src/qt/dialog/set_session_steps_dialog.h"
#include "desktop/src/qt/dialog/settings/audio_settings_dialog.h"
#include "desktop/src/qt/settings/app_settings.h"

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

    display = new DisplayWidget(this);
    setCentralWidget(display);
    adjustSize();

    render_timer = new QTimer(this);
    connect(render_timer, &QTimer::timeout, display, QOverload<>::of(&QWidget::update));
    render_timer->start(16);

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
}

void QtWindowSystem::shutdownEmulator()
{
    if (!context)
        return;

    render_timer->stop();
    display->setEmulator(nullptr);
    display->update();
    context.reset();
    setEmulatorActionsEnabled(false);
    setWindowTitle("PocketWalker");
}

void QtWindowSystem::setEmulatorActionsEnabled(bool enabled)
{
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
    if (!context)
    {
        QMainWindow::keyPressEvent(event);
        return;
    }

    const auto& controls = AppSettings::instance.controls;
    const int key = event->key();

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
    if (!context)
    {
        QMainWindow::keyReleaseEvent(event);
        return;
    }

    const auto& controls = AppSettings::instance.controls;
    const int key = event->key();

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
    releaseHeldInputs();
    shutdownEmulator();
    event->accept();
}
