#pragma once

#include <QDialog>
#include <QSettings>

#include <memory>

namespace Ui {
class AdvancedSettings;
}

class CameraInfo;
class QCheckBox;

class CustomSettings  : public QDialog
{
  Q_OBJECT
public:
  CustomSettings(std::shared_ptr<CameraInfo> info);

  void changedDevice(uint16_t deviceIndex);

  void showAdvancedSettings();

  void resetSettings();

signals:

  void customSettingsChanged();

public slots:

  void on_advanced_ok_clicked();
  void on_advanced_cancel_clicked();

private:
  // QSettings -> GUI
  void restoreAdvancedSettings();

  // GUI -> QSettings
  // permanently records GUI settings
  void saveAdvancedSettings();

  void saveCameraCapabilities(int deviceIndex, int capabilityIndex);

  bool checkVideoSettings();
  bool checkMissingValues(); // TODO: in two places

  // simpler functions for checkbox management.
  void restoreCheckBox(const QString settingValue, QCheckBox* box);
  void saveCheckBox(const QString settingValue, QCheckBox* box);

  // TODO: this is now in two places
  void saveTextValue(const QString settingValue, const QString &text);

  uint16_t currentDevice_;

  Ui::AdvancedSettings *advancedUI_;

  std::shared_ptr<CameraInfo> cam_;

  QSettings settings_;
};
