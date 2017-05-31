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

signals:
  void settingsChanged();

public slots:

  void on_ok_clicked();
  void on_cancel_clicked();

private:

  // checks if user settings make sense
  // TODO: display errors to user on ok click
  bool checkUIBasicSettings();
  bool checkUIAdvancedSettings();
  bool checkSavedSettings();

  // QSettings -> GUI
  void restoreBasicSettings();
  void restoreAdvancedSettings();

  // GUI -> QSettings
  // permanently records GUI settings to system
  void saveBasicSettings();
  void saveAdvancedSettings();

  QStringList getVideoDevices();
  QStringList getAudioDevices();
  QStringList getVideoCapabilities(QString device);

  void resetFaultySettings();

  int getSettingsDeviceID();

  QDialog basicParent_;
  Ui::BasicSettings *basicUI_;
  QDialog advancedParent_;
  Ui::AdvancedSettings *advancedUI_;
};
