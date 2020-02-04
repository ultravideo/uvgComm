#pragma once

#include <QDialog>
#include <QSettings>

namespace Ui {
class AdvancedSettings;
}

class QCheckBox;

class AdvancedSettings : public QDialog
{
  Q_OBJECT
public:
  AdvancedSettings(QWidget* parent);

  // initializes the custom view with values from settings.
  void init();

  void resetSettings();

signals:
  void advancedSettingsChanged();
  void hidden();

public slots:

  // slots related to blocklist
  void showBlocklistContextMenu(const QPoint& pos);
  void deleteBlocklistItem();

  // button slot to add a user to list.
  void on_addUserBlock_clicked();

  // button slots, called automatically by Qt if they are named correctly
  void on_advanced_ok_clicked();
  void on_advanced_close_clicked();

private:

  // QSettings -> GUI
  void restoreAdvancedSettings();

  // GUI -> QSettings
  void saveAdvancedSettings();

  void addUsernameToList(QString username, QString date);

  bool checkSipSettings();

  Ui::AdvancedSettings *advancedUI_;

  QSettings settings_;
};
