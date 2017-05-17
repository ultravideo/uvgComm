#pragma once

#include <QDialog>
#include <QSettings>

namespace Ui {
class Settings;
}

class Settings : public QDialog
{
  Q_OBJECT

public:
  explicit Settings(QWidget *parent = 0);
  ~Settings();

signals:
  void settingsChanged();

public slots:

  void on_ok_clicked();
  void on_cancel_clicked();

private:
  Ui::Settings *ui;
  bool checkGUISettings();

  // QSettings -> GUI
  void restoreSettings();

  // GUI -> QSettings
  // permanently records GUI settings to system
  void saveSettings();
};
