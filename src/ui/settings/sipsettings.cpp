#include "sipsettings.h"

#include "ui_sipsettings.h"
#include "settingshelper.h"
#include "settingskeys.h"

#include <QDateTime>
#include <QDebug>

SIPSettings::SIPSettings(QWidget* parent):
  QDialog (parent),
  advancedUI_(new Ui::AdvancedSettings),
  settings_(settingsFile, settingsFileFormat)
{
  advancedUI_->setupUi(this);
}


void SIPSettings::init()
{
  advancedUI_->blockedUsers->setColumnCount(2);
  advancedUI_->blockedUsers->setColumnWidth(0, 240);
  advancedUI_->blockedUsers->setColumnWidth(1, 180);

  // TODO: Does not work for some reason.
  QStringList longerList = (QStringList() << "Username" << "Date");
  advancedUI_->blockedUsers->setHorizontalHeaderLabels(longerList);

  // disallow editing of fields.
  advancedUI_->blockedUsers->setEditTriggers(QAbstractItemView::NoEditTriggers);

  advancedUI_->blockedUsers->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(advancedUI_->blockedUsers, &QWidget::customContextMenuRequested,
          this, &SIPSettings::showBlocklistContextMenu);

  restoreAdvancedSettings();
}


void SIPSettings::resetSettings()
{
  qDebug() << "Settings," << metaObject()->className()
           << "Resetting Advanced settings from UI";
  // TODO: should we do something to blocked list in settings?

  saveAdvancedSettings();
}


void SIPSettings::showBlocklistContextMenu(const QPoint& pos)
{
  if (advancedUI_->blockedUsers->rowCount() != 0)
  {
    showContextMenu(pos, advancedUI_->blockedUsers, this,
                    QStringList() << "Delete", QStringList() << SLOT(deleteBlocklistItem()));
  }
}


void SIPSettings::deleteBlocklistItem()
{
  qDebug() << "Settings," << metaObject()->className()
           << ": deleting row:" << advancedUI_->blockedUsers->currentRow();
  advancedUI_->blockedUsers->removeRow(advancedUI_->blockedUsers->currentRow());
}


void SIPSettings::on_advanced_ok_clicked()
{
  qDebug() << "Settings," << metaObject()->className() << ": Saving advanced settings";
  saveAdvancedSettings();
  emit advancedSettingsChanged();
}


void SIPSettings::on_advanced_close_clicked()
{
  qDebug() << "Settings," << metaObject()->className()
           << ": Cancelled modifying custom settings. Getting settings from system";
  restoreAdvancedSettings();
  hide();
  emit hidden();
}


void SIPSettings::on_addUserBlock_clicked()
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


void SIPSettings::saveAdvancedSettings()
{
  qDebug() << "Settings," << metaObject()->className() << ": Saving advanced Settings";

  listGUIToSettings(blocklistFile, SettingsKey::blocklist,
                    QStringList() << "userName" << "date", advancedUI_->blockedUsers);

  // sip settings.
  saveCheckBox(SettingsKey::localAutoAccept, advancedUI_->auto_accept, settings_);
}


void SIPSettings::restoreAdvancedSettings()
{
  listSettingsToGUI(blocklistFile, SettingsKey::blocklist, QStringList()
                    << "userName" << "date", advancedUI_->blockedUsers);

  bool validSettings = checkMissingValues(settings_);

  if(validSettings && checkSipSettings())
  {
    restoreCheckBox(SettingsKey::localAutoAccept, advancedUI_->auto_accept, settings_);
  }
  else
  {
    resetSettings();
  }
}


bool SIPSettings::checkSipSettings()
{
  return settings_.contains(SettingsKey::sipServerAddress)
      && settings_.contains(SettingsKey::sipAutoConnect)
      && settings_.contains(SettingsKey::localAutoAccept);
}
