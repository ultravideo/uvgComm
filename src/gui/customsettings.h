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
  CustomSettings(QWidget* parent, std::shared_ptr<CameraInfo> info);

  void init(int deviceID);

  void changedDevice(uint16_t deviceIndex);
  void resetSettings(int deviceID);

signals:

  void customSettingsChanged();
  void hidden();

public slots:

  virtual void show();

  // button slots, called automatically by Qt
  void on_custom_ok_clicked();
  void on_custom_cancel_clicked();

private:
  // QSettings -> GUI
  void restoreAdvancedSettings();

  // GUI -> QSettings
  // permanently records GUI settings
  void saveAdvancedSettings();

  void saveCameraCapabilities(int deviceIndex);

  // initializes the UI with correct formats and resolutions
  void initializeFormatAndResolutions();

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
