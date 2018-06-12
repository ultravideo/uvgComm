#pragma once
#include <QDialog>
#include <QSettings>

namespace Ui {
class BasicSettings;
class AdvancedSettings;
}

class QCheckBox;

// TODO: Possibly separate settings ui and settings management
// TODO: Settings of SIP server
class Settings : public QObject
{
  Q_OBJECT

public:
  explicit Settings(QWidget *parent = 0);
  ~Settings();

  void showBasicSettings();
  void showAdvancedSettings();

  void updateDevices();

signals:
  void settingsChanged();

public slots:

  void on_ok_clicked();
  void on_cancel_clicked();

  void on_advanced_ok_clicked();
  void on_advanced_cancel_clicked();

private:
  void initializeDeviceList();

  // checks if user settings make sense
  // TODO: display errors to user on ok click
  bool checkUserSettings();
  bool checkVideoSettings();
  bool checkMissingValues();

  // QSettings -> GUI
  void restoreBasicSettings();
  void restoreAdvancedSettings();

  // GUI -> QSettings
  // permanently records GUI settings to system
  void saveBasicSettings();
  void saveAdvancedSettings();
  void saveCameraCapabilities(int deviceIndex, int capabilityIndex);

  QStringList getVideoDevices();
  QStringList getAudioDevices();
  QStringList getVideoCapabilities(int deviceID);
  void getCapability(int deviceIndex, int capabilityIndex,
                     QSize& resolution, double& framerate, QString &format);

  // make sure the ui video devices is initialized before calling this
  int getVideoDeviceID();

  void resetFaultySettings();

  void restoreCheckBox(const QString settingValue, QCheckBox* box);
  void saveCheckBox(const QString settingValue, QCheckBox* box);

  void saveTextValue(const QString settingValue, const QString &text);


  QDialog basicParent_;
  Ui::BasicSettings *basicUI_;
  QDialog advancedParent_;
  Ui::AdvancedSettings *advancedUI_;

  QSettings settings_;
};
