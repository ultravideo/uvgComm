#pragma once

#include "gui/customSettings.h"

#include <QDialog>
#include <QSettings>

#include <memory>

namespace Ui {
class BasicSettings;
}

class CameraInfo;

class QCheckBox;

// TODO: Settings of SIP server
class Settings : public QDialog
{
  Q_OBJECT

public:
  explicit Settings(QWidget *parent = 0);
  ~Settings();

  void updateDevices();

signals:
  void settingsChanged();

public slots:

  virtual void show();

  // button slots, called automatically by Qt
  void on_ok_clicked();
  void on_cancel_clicked();
  void on_advanced_settings_button_clicked();

private:
  void initializeDeviceList();

  // checks if user settings make sense
  // TODO: display errors to user on ok click
  bool checkUserSettings();
  bool checkMissingValues();

  // QSettings -> GUI
  void restoreBasicSettings();

  // GUI -> QSettings
  // permanently records GUI settings
  void saveBasicSettings();

  QStringList getAudioDevices();

  // make sure the ui video devices is initialized before calling this
  int getVideoDeviceID();

  void resetFaultySettings();

  void saveTextValue(const QString settingValue, const QString &text);

  Ui::BasicSettings *basicUI_;

  std::shared_ptr<CameraInfo> cam_;

  CustomSettings custom_;

  QSettings settings_;
};
