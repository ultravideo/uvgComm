#include "advancedsettings.h"

#include "ui_advancedSettings.h"
#include "settingshelper.h"

#include <QMenu>
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
  qDebug() << "Settings," << metaObject()->className() << ": Showing context menu for blocked users.";

  // Handle global position
  QPoint globalPos = advancedUI_->blockedUsers->mapToGlobal(pos);

  // Create menu and insert some actions
  QMenu myMenu;
  myMenu.addAction("Delete", this, SLOT(deleteBlocklistItem()));

  // Show context menu at handling position
  myMenu.exec(globalPos);
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
  emit hidden();
  hide();
}


void AdvancedSettings::on_advanced_cancel_clicked()
{
  qDebug() << "Settings," << metaObject()->className() << ": Cancelled modifying custom settings. Getting settings from system";
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

    addUsernameToList(advancedUI_->blockUser->text(), QDateTime::currentDateTime().toString("yyyy-mm-dd hh:mm"));

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

  writeBlocklistToSettings();

  // sip settings.
  saveTextValue("sip/ServerAddress", advancedUI_->serverAddress->text(), settings_);
  saveCheckBox("sip/AutoConnect", advancedUI_->autoConnect, settings_);
  saveCheckBox("sip/ice", advancedUI_->ice, settings_);
}


void AdvancedSettings::restoreAdvancedSettings()
{
  initializeBlocklist();

  bool validSettings = checkMissingValues(settings_);

  if(validSettings && checkSipSettings())
  {
    restoreCheckBox("sip/AutoConnect", advancedUI_->autoConnect, settings_);
    restoreCheckBox("sip/ice", advancedUI_->ice, settings_);
    advancedUI_->serverAddress->setText        (settings_.value("sip/ServerAddress").toString());
  }
  else
  {
    resetSettings();
  }
}


void AdvancedSettings::initializeBlocklist()
{
  //list_->setContextMenuPolicy(Qt::CustomContextMenu);
  //connect(list, SIGNAL(customContextMenuRequested(QPoint)),
  //        this, SLOT(showContextMenu(QPoint)));

  //advancedUI_->blockedUsers->clear();
  advancedUI_->blockedUsers->setRowCount(0);

  QSettings settings("blocklist.local", QSettings::IniFormat);

  int size = settings.beginReadArray("blocklist");
  qDebug() << "Settings,"  << metaObject()->className() << ": Reading blocklist with" << size << "usernames";
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    QString username = settings.value("userName").toString();
    QString date = settings.value("date").toString();

    addUsernameToList(username, date);
  }
  settings.endArray();
}


void AdvancedSettings::writeBlocklistToSettings()
{
  qDebug() << "Settings," << metaObject()->className() << "Writing blocklist with"
           << advancedUI_->blockedUsers->rowCount() << "items to settings.";

  QSettings settings("blocklist.local", QSettings::IniFormat);

  settings.beginWriteArray("blocklist");
  for(int i = 0; i < advancedUI_->blockedUsers->rowCount(); ++i)
  {
    settings.setArrayIndex(i);
    settings.setValue("username", advancedUI_->blockedUsers->item(i,0)->text());
    settings.setValue("date", advancedUI_->blockedUsers->item(i,1)->text());
  }
  settings.endArray();
}


void AdvancedSettings::addUsernameToList(QString username, QString date)
{
  QTableWidgetItem* itemUser = new QTableWidgetItem(username);
  QTableWidgetItem* itemDate = new QTableWidgetItem(date);

  advancedUI_->blockedUsers->insertRow(advancedUI_->blockedUsers->rowCount());

  advancedUI_->blockedUsers->setItem(advancedUI_->blockedUsers->rowCount() - 1, 0, itemUser);
  advancedUI_->blockedUsers->setItem(advancedUI_->blockedUsers->rowCount() - 1, 1, itemDate);
}


bool AdvancedSettings::checkSipSettings()
{
  return settings_.contains("sip/ServerAddress")
      && settings_.contains("sip/AutoConnect");
}
