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
  bool checkGUISettings();

  // QSettings -> GUI
  void restoreSettings();

  // GUI -> QSettings
  // permanently records GUI settings to system
  void saveSettings();

  QStringList getVideoDevices();
  QStringList getAudioDevices();
  QStringList getVideoCapabilities(QString device);

  QDialog basicParent_;
  Ui::BasicSettings *basicUI_;
  QDialog advancedParent_;
  Ui::AdvancedSettings *advancedUI_;
};
