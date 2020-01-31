#include "advancedsettings.h"

#include "ui_advancedsettings.h"
#include "settingshelper.h"

#include <QDateTime>
#include <QDebug>

AdvancedSettings::AdvancedSettings(QWidget* parent):
  QDialog (parent),
  advancedUI_(new Ui::AdvancedSettings),
  settings_("kvazzup.ini", QSettings::IniFormat)
{
  advancedUI_->setupUi(this);
}


void AdvancedSettings::init()
{
  advancedUI_->blockedUsers->setColumnCount(2);
  advancedUI_->blockedUsers->setColumnWidth(0, 240);
  advancedUI_->blockedUsers->setColumnWidth(1, 180);

  // TODO: Does not work for some reason.
  QStringList longerList = (QStringList() << "Username" << "Date");
  advancedUI_->blockedUsers->setHorizontalHeaderLabels(longerList);

  advancedUI_->blockedUsers->setEditTriggers(QAbstractItemView::NoEditTriggers); // disallow editing of fields.

  advancedUI_->blockedUsers->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(advancedUI_->blockedUsers, &QWidget::customContextMenuRequested,
          this, &AdvancedSettings::showBlocklistContextMenu);

  restoreAdvancedSettings();
}


void AdvancedSettings::resetSettings()
{
  qDebug() << "Settings," << metaObject()->className()
           << "Resetting Advanced settings from UI";
  // TODO: should we do something to blocked list in settings?
  saveAdvancedSettings();
}


void AdvancedSettings::showBlocklistContextMenu(const QPoint& pos)
{
  if (advancedUI_->blockedUsers->rowCount() != 0)
  {
    showContextMenu(pos, advancedUI_->blockedUsers, this,
                    QStringList() << "Delete", QStringList() << SLOT(deleteBlocklistItem()));
  }
}


void AdvancedSettings::deleteBlocklistItem()
{
  qDebug() << "Settings," << metaObject()->className()
           << ": deleting row:" << advancedUI_->blockedUsers->currentRow();
  advancedUI_->blockedUsers->removeRow(advancedUI_->blockedUsers->currentRow());
}


void AdvancedSettings::on_advanced_ok_clicked()
{
  qDebug() << "Settings," << metaObject()->className() << ": Saving advanced settings";
  saveAdvancedSettings();
  emit advancedSettingsChanged();
}


void AdvancedSettings::on_advanced_close_clicked()
{
  qDebug() << "Settings," << metaObject()->className()
           << ": Cancelled modifying custom settings. Getting settings from system";
  restoreAdvancedSettings();
  hide();
  emit hidden();
}


void AdvancedSettings::on_addUserBlock_clicked()
{
  if(advancedUI_->blockUser->text() != "")
  {
    for(int i = 0; i < advancedUI_->blockedUsers->rowCount(); ++i)
    {
      if(advancedUI_->blockedUsers->item(i,0)->text() == advancedUI_->blockUser->text())
      {
        qDebug() << "Settings," << metaObject()->className() << ": Name already exists at row:" << i;
        advancedUI_->BlockUsernameLabel->setText("Name already blocked");
        return;
      }
    }

    qDebug() << "Settings," << metaObject()->className() << ":Blocking an user";

    QStringList list = QStringList() << advancedUI_->blockUser->text()
                                     << QDateTime::currentDateTime().toString("yyyy-mm-dd hh:mm");
    addFieldsToTable(list, advancedUI_->blockedUsers);

    advancedUI_->BlockUsernameLabel->setText("Block contacts from username:");
    advancedUI_->blockUser->setText("");
  }
  else
  {
    advancedUI_->BlockUsernameLabel->setText("Write username below:");
  }
}


void AdvancedSettings::serverStatusChange(QString status)
{
  advancedUI_->StatusLabel->setText(status);

  if(status == "Connected")
  {
    advancedUI_->connectButton->setText("Disconnect");
  }
}


void AdvancedSettings::saveAdvancedSettings()
{
  qDebug() << "Settings," << metaObject()->className() << ": Saving advanced Settings";

  listGUIToSettings("blocklist.local", "blocklist", QStringList() << "userName" << "date", advancedUI_->blockedUsers);

  // sip settings.
  saveTextValue("sip/ServerAddress", advancedUI_->serverAddress->text(), settings_);
  saveCheckBox("sip/AutoConnect", advancedUI_->autoConnect, settings_);
  saveCheckBox("sip/ice", advancedUI_->ice, settings_);
  saveCheckBox("sip/conference", advancedUI_->conference, settings_);
  saveCheckBox("sip/kvzRTP", advancedUI_->kvzRTP, settings_);
  saveCheckBox("local/Auto-Accept", advancedUI_->auto_accept, settings_);
}


void AdvancedSettings::restoreAdvancedSettings()
{
  listSettingsToGUI("blocklist.local", "blocklist", QStringList() << "userName" << "date", advancedUI_->blockedUsers);

  bool validSettings = checkMissingValues(settings_);

  if(validSettings && checkSipSettings())
  {
    restoreCheckBox("sip/AutoConnect", advancedUI_->autoConnect, settings_);
    restoreCheckBox("sip/ice", advancedUI_->ice, settings_);
    restoreCheckBox("sip/conference", advancedUI_->conference, settings_);
    restoreCheckBox("sip/kvzrtp", advancedUI_->kvzRTP, settings_);
    advancedUI_->serverAddress->setText(settings_.value("sip/ServerAddress").toString());
    restoreCheckBox("local/Auto-Accept", advancedUI_->auto_accept, settings_);
  }
  else
  {
    resetSettings();
  }
}


bool AdvancedSettings::checkSipSettings()
{
  return settings_.contains("sip/ServerAddress")
      && settings_.contains("sip/AutoConnect")
      && settings_.contains("local/Auto-Accept");
}
