#pragma once
#include <array>
#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include "desktop/src/qt/settings/types/emulation_settings.h"

class EmulationSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EmulationSettingsDialog(QWidget* parent = nullptr);

signals:
    void paletteChanged();
    void bypassPowerSaveChanged();
    void stepPeriodChanged();

private slots:
    void apply();
    void reset();

private:
    void pickColor(int index);
    void updateSwatch(int index);
    void readStepControls();

    std::array<QPushButton*, 4> swatches;
    std::array<EmulationSettings::Color, 4> pending_palette;

    QCheckBox* bypass_power_save_check;

    QSlider* step_rate;
};
