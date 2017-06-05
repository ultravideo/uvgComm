#pragma once
#include <QDialog>
#include <QSettings>

namespace Ui {
class BasicSettings;
class AdvancedSettings;
}

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

private:

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
  void saveCameraCapabilities(QSettings &settings,
                              int deviceIndex, int capabilityIndex);

  QStringList getVideoDevices();
  QStringList getAudioDevices();
  QStringList getVideoCapabilities(int deviceID);
  void getCapability(int deviceIndex, int capabilityIndex,
                     QSize& resolution, double& framerate);

  // make sure the ui video devices is initialized before calling this
  int getVideoDeviceID(QSettings& settings);

  void resetFaultySettings();

  QDialog basicParent_;
  Ui::BasicSettings *basicUI_;
  QDialog advancedParent_;
  Ui::AdvancedSettings *advancedUI_;
};
