#include "emulation_settings_dialog.h"

#include <QCheckBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QColorDialog>

#include "desktop/src/qt/settings/app_settings.h"
#include "desktop/src/qt/settings/types/emulation_settings.h"

static const char* SWATCH_LABELS[4] = {"Color 0", "Color 1:", "Color 2:", "Color 3:"};

EmulationSettingsDialog::EmulationSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Emulation Settings");
    setMinimumWidth(300);

    auto& emulation = AppSettings::instance.emulation;


    auto* enhancements_group = new QGroupBox("Enhancements", this);

    auto* enhancements_form = new QFormLayout();

    bypass_power_save_check = new QCheckBox("Bypass Power Save Mode", this);
    bypass_power_save_check->setChecked(emulation.bypass_power_save);

    enhancements_form->addRow(bypass_power_save_check);

    enhancements_group->setLayout(enhancements_form);

    step_rate = new QSlider(Qt::Horizontal, this);
    step_rate->setRange(0, STEP_SAMPLE_MAX_PERIOD - STEP_SAMPLE_MIN_PERIOD);
    step_rate->setValue(STEP_SAMPLE_MAX_PERIOD - emulation.step_period);

    auto* slow_label = new QLabel("Slow", this);
    auto* fast_label = new QLabel("Fast", this);
    slow_label->setStyleSheet("color: gray;");
    fast_label->setStyleSheet("color: gray;");

    auto* step_rate_row = new QHBoxLayout();
    step_rate_row->addWidget(slow_label);
    step_rate_row->addWidget(step_rate);
    step_rate_row->addWidget(fast_label);

    auto* step_form = new QFormLayout();
    step_form->addRow("Pace", step_rate_row);

    auto* step_group = new QGroupBox("Synthetic Steps", this);
    step_group->setLayout(step_form);

    pending_palette = AppSettings::instance.emulation.palette;

    auto* palette_form = new QFormLayout();

    for (int i = 0; i < 4; i++)
    {
        swatches[i] = new QPushButton(this);
        swatches[i]->setFixedHeight(28);
        updateSwatch(i);
        connect(swatches[i], &QPushButton::clicked, this, [this, i]
        {
            pickColor(i);
        });

        palette_form->addRow(SWATCH_LABELS[i], swatches[i]);
    }

    auto* reset_button = new QPushButton("Reset");
    connect(reset_button, &QPushButton::clicked, this, &EmulationSettingsDialog::reset);
    palette_form->addRow(reset_button);

    auto* palette_group = new QGroupBox("LCD Palette", this);
    palette_group->setLayout(palette_form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(enhancements_group);
    layout->addWidget(step_group);
    layout->addWidget(palette_group);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &EmulationSettingsDialog::apply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void EmulationSettingsDialog::pickColor(int index)
{
    const auto& c = pending_palette[index];
    const QColor initial(c.r, c.g, c.b);

    const QColor chosen = QColorDialog::getColor(initial, this, SWATCH_LABELS[index]);
    if (!chosen.isValid())
        return;

    pending_palette[index] = {
        static_cast<uint8_t>(chosen.red()),
        static_cast<uint8_t>(chosen.green()),
        static_cast<uint8_t>(chosen.blue())
    };
    updateSwatch(index);
}


void EmulationSettingsDialog::updateSwatch(int index)
{
    const auto& [r, g, b] = pending_palette[index];

    const QColor color(r, g, b);
    const int luminance = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b);
    const QString foreground_color = (luminance < 128) ? "#ffffff" : "#000000";

    swatches[index]->setStyleSheet(
        QString("background-color: %1; color: %2; border: 1px solid gray; border-radius: 4px;").arg(color.name()).arg(
            foreground_color));

    swatches[index]->setText(color.name().toUpper());
}

void EmulationSettingsDialog::readStepControls()
{
    AppSettings::instance.emulation.step_period =
        static_cast<uint8_t>(STEP_SAMPLE_MAX_PERIOD - step_rate->value());
}

void EmulationSettingsDialog::reset()
{
    pending_palette = {
        {
            {0xCC, 0xCC, 0xCC},
            {0x99, 0x99, 0x99},
            {0x66, 0x66, 0x66},
            {0x33, 0x33, 0x33}
        }
    };

    for (int i = 0; i < 4; i++)
        updateSwatch(i);
}

void EmulationSettingsDialog::apply()
{
    auto& emulation = AppSettings::instance.emulation;
    emulation.palette = pending_palette;
    emulation.bypass_power_save = bypass_power_save_check->isChecked();
    readStepControls();

    emit paletteChanged();
    emit bypassPowerSaveChanged();
    emit stepPeriodChanged();
    accept();
}
