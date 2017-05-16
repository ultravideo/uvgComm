#pragma once

#include <QDialog>

namespace Ui {
class Settings;
}

class Settings : public QDialog
{
  Q_OBJECT

public:
  explicit Settings(QWidget *parent = 0);
  ~Settings();

  virtual void show();

  // the settings have three locations: GUI, temp storage and file

  // returns GUI settings
  void getSettings(std::map<QString, QString>& settings);

  // GUI -> file
  void settingsToFile(QString filename);

  // GUI <- file
  void loadSettingsFromFile(QString filename);

signals:
  void settingsChanged();

public slots:

  // restores settings
  void cancel();

private:
  Ui::Settings *ui;
  bool checkGUISettings();

  // GUI -> temp
  // use this when editing that can be reverted starts
  void saveSettings();

  // GUI <- temp
  // use this when to revert changes
  void restoreSettings();

  // purpose of this is a place record/restore settings in case the cancel button is pushed
  std::map<QString, QString> tempSettings_;
};
