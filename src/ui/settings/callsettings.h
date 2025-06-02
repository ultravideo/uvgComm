#pragma once

#include "ui_stunmessage.h"

#include <QDialog>
#include <QSettings>



namespace Ui {
class AdvancedSettings;
class StunMessage;
}

class QCheckBox;

class CallSettings : public QDialog
{
  Q_OBJECT
public:
  CallSettings(QWidget* parent);

  // initializes the custom view with values from settings.
  void init();

  void resetSettings();

  void showSTUNQuestion();

  virtual void closeEvent(QCloseEvent *event);

signals:
  void updateCallSettings();
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

  void acceptSTUN();
  void declineSTUN();

  void showOkButton();

  void resolutionComboChanged(const QString& text);

private:

  // QSettings -> GUI
  void restoreAdvancedSettings();

  // GUI -> QSettings
  void saveAdvancedSettings();

  void addUsernameToList(QString username, QString date);

  int getAudioBitrate(int totalBitrate) const;
  int getVideoBitrate(int totalBitrate) const;

  int getTotalBitrate(int audioBitrate, int videoBitrate) const;

  QString bitrateString(int totalBitrate, int audioBitrate, int videoBitrate) const;



  Ui::AdvancedSettings *advancedUI_;
  Ui::StunMessage stunQuestion_;
  QWidget stun_;

  QSettings settings_;
};
