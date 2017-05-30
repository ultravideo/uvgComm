#pragma once

#include <QDialog>
#include <QSettings>

namespace Ui {
class BasicSettings;
class AdvancedSettings;
}

class Settings : public QObject// : public QDialog
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
  QDialog basicParent_;
  Ui::BasicSettings *basicUI_;
  QDialog advancedParent_;
  Ui::AdvancedSettings *advancedUI_;
  bool checkGUISettings();

  // QSettings -> GUI
  void restoreSettings();

  // GUI -> QSettings
  // permanently records GUI settings to system
  void saveSettings();
};
