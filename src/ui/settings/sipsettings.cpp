#include "sipsettings.h"

#include "ui_sipsettings.h"


#include "settingshelper.h"
#include "settingskeys.h"
#include "logger.h"

#include <QDateTime>


const QStringList neededSettings = {SettingsKey::localAutoAccept,
                                    SettingsKey::sipRole,
                                    SettingsKey::sipTopology,
                                    SettingsKey::sipMediaPort,
#ifdef uvgComm_NO_RTP_MULTIPLEXING
                                    SettingsKey::sipICEEnabled,
#endif
                                    SettingsKey::privateAddresses,
                                    SettingsKey::sipSTUNEnabled,
                                    SettingsKey::sipSTUNAddress,
                                    SettingsKey::sipSTUNPort,
                                    SettingsKey::sipSRTP};

SIPSettings::SIPSettings(QWidget* parent):
  QDialog (parent),
  advancedUI_(new Ui::AdvancedSettings),
  settings_(settingsFile, settingsFileFormat)
{
  advancedUI_->setupUi(this);
  stunQuestion_.setupUi(&stun_);
  stun_.setWindowFlags(Qt::WindowStaysOnTopHint);

  connect(advancedUI_->role_combo, &QComboBox::currentTextChanged,
          this,                    &SIPSettings::showServerAddress);

  connect(advancedUI_->topology_combo, &QComboBox::currentTextChanged,
          this,                        &SIPSettings::showServerAddress);

  connect(advancedUI_->server_address, &QLineEdit::textChanged,
          this,                        &SIPSettings::showOkButton);

  connect(advancedUI_->server_port, &QLineEdit::textChanged,
          this,                     &SIPSettings::showOkButton);
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

void SIPSettings::showSTUNQuestion()
{
  connect(stunQuestion_.yes_button,  &QPushButton::pressed,
          this,                      &SIPSettings::acceptSTUN);

  connect(stunQuestion_.no_button,  &QPushButton::pressed,
          this,                      &SIPSettings::declineSTUN);

  // stun_.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
  stun_.show();
  stun_.setWindowTitle("Use STUN?");
}


void SIPSettings::closeEvent(QCloseEvent *event)
{
  on_advanced_close_clicked();
  QDialog::closeEvent(event);
}


void SIPSettings::acceptSTUN()
{
  Logger::getLogger()->printNormal(this, "User accepted STUN usage");

  advancedUI_->stun_enabled->setChecked(true);
  advancedUI_->stun_address->setText(stunQuestion_.address->text());
  advancedUI_->stun_port->setValue(stunQuestion_.port->value());

  saveCheckBox(SettingsKey::sipSTUNEnabled, advancedUI_->stun_enabled, settings_);
  stun_.hide();
}

void SIPSettings::declineSTUN()
{
  Logger::getLogger()->printNormal(this, "User declined STUN usage");
  advancedUI_->stun_enabled->setChecked(false);

  saveCheckBox(SettingsKey::sipSTUNEnabled, advancedUI_->stun_enabled, settings_);
  stun_.hide();
}


void SIPSettings::resetSettings()
{
  Logger::getLogger()->printWarning(this, "Resetting default SIP settings from UI");
  // TODO: should we do something to blocked list in settings?

  saveAdvancedSettings();
}


void SIPSettings::showBlocklistContextMenu(const QPoint& pos)
{
  if (advancedUI_->blockedUsers->rowCount() != 0)
  {
    showContextMenu(pos, advancedUI_->blockedUsers, this,
                    QStringList() << "Delete", QStringList()
                    << SLOT(deleteBlocklistItem()));
  }
}


void SIPSettings::deleteBlocklistItem()
{
  Logger::getLogger()->printNormal(this, "Deleting blocklist row", "Row index",
                                   QString::number(advancedUI_->blockedUsers->currentRow()));

  advancedUI_->blockedUsers->removeRow(advancedUI_->blockedUsers->currentRow());
}


void SIPSettings::on_advanced_ok_clicked()
{
  Logger::getLogger()->printNormal(this, "Saving SIP settings");

  saveAdvancedSettings();
  emit updateCallSettings();
}


void SIPSettings::on_advanced_close_clicked()
{
  Logger::getLogger()->printNormal(this, "Cancelled modifyin SIP settings. Restoring settings from file");

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
        Logger::getLogger()->printWarning(this, "Name already exists", "Row", QString::number(i + 1));
        advancedUI_->BlockUsernameLabel->setText("Name already blocked");
        return;
      }
    }

    Logger::getLogger()->printNormal(this, "Blocking an user");

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
  listGUIToSettings(blocklistFile, SettingsKey::blocklist,
                    QStringList() << "userName" << "date", advancedUI_->blockedUsers);

  // sip settings.
  saveCheckBox(SettingsKey::localAutoAccept,    advancedUI_->auto_accept, settings_);
  saveCheckBox(SettingsKey::sipSTUNEnabled,     advancedUI_->stun_enabled, settings_);
#ifdef uvgComm_NO_RTP_MULTIPLEXING
  saveCheckBox(SettingsKey::sipICEEnabled,      advancedUI_->ice_checkbox, settings_);
#endif
  saveCheckBox(SettingsKey::privateAddresses,   advancedUI_->local_checkbox, settings_);
  saveCheckBox(SettingsKey::sipSRTP,            advancedUI_->srtp_enabled, settings_);

  saveTextValue(SettingsKey::sipRole, advancedUI_->role_combo->currentText(), settings_);
  saveTextValue(SettingsKey::sipTopology, advancedUI_->topology_combo->currentText(), settings_);

  saveTextValue(SettingsKey::sipMediaServerAddress, advancedUI_->server_address->text(), settings_);
  saveTextValue(SettingsKey::sipMediaServerPort,    advancedUI_->server_port->text(), settings_);

  saveTextValue(SettingsKey::sipSTUNAddress, advancedUI_->stun_address->text(), settings_);

  settings_.setValue(SettingsKey::sipSTUNPort,  QString::number(advancedUI_->stun_port->value()));
  settings_.setValue(SettingsKey::sipMediaPort,  QString::number(advancedUI_->media_port->value()));
}


void SIPSettings::restoreAdvancedSettings()
{
  listSettingsToGUI(blocklistFile, SettingsKey::blocklist, QStringList()
                    << "userName" << "date", advancedUI_->blockedUsers);

  if(checkSettingsList(settings_, neededSettings))
  {
    restoreCheckBox(SettingsKey::localAutoAccept,    advancedUI_->auto_accept, settings_);
    restoreCheckBox(SettingsKey::sipSTUNEnabled,     advancedUI_->stun_enabled, settings_);

// TODO: Remove this once ICE works with multiplexing
#ifdef uvgComm_NO_RTP_MULTIPLEXING
    restoreCheckBox(SettingsKey::sipICEEnabled,     advancedUI_->ice_checkbox, settings_);
#else
    advancedUI_->ice_label->hide();
    advancedUI_->ice_checkbox->hide();
#endif
    restoreCheckBox(SettingsKey::privateAddresses,     advancedUI_->local_checkbox, settings_);
    restoreCheckBox(SettingsKey::sipSRTP,            advancedUI_->srtp_enabled, settings_);

    QString type = settings_.value(SettingsKey::sipRole).toString();
    advancedUI_->role_combo->setCurrentText(type);

    type = settings_.value(SettingsKey::sipTopology).toString();
    advancedUI_->topology_combo->setCurrentText(type);

    advancedUI_->server_address->setText(settings_.value(SettingsKey::sipMediaServerAddress).toString());
    advancedUI_->server_port->setText(settings_.value(SettingsKey::sipMediaServerPort).toString());

    advancedUI_->stun_address->setText(settings_.value(SettingsKey::sipSTUNAddress).toString());
    advancedUI_->stun_port->setValue  (settings_.value(SettingsKey::sipSTUNPort).toInt());
    advancedUI_->media_port->setValue (settings_.value(SettingsKey::sipMediaPort).toInt());
  }
  else
  {
    resetSettings();
  }

  showServerAddress("");
}


void SIPSettings::showServerAddress(QString text)
{
  if (advancedUI_->role_combo->currentText() == "Client" &&
      (advancedUI_->topology_combo->currentText() == "SFU" ||
       advancedUI_->topology_combo->currentText() == "MCU" ||
       advancedUI_->topology_combo->currentText() == "Relay"))
  {
    advancedUI_->server_address_label->show();
    advancedUI_->server_port_label->show();

    advancedUI_->server_address->show();
    advancedUI_->server_port->show();
  }
  else
  {
    advancedUI_->server_address_label->hide();
    advancedUI_->server_port_label->hide();

    advancedUI_->server_address->hide();
    advancedUI_->server_port->hide();

    advancedUI_->advanced_ok->setEnabled(true);
  }

  showOkButton();
}


void SIPSettings::showOkButton()
{
  if (advancedUI_->role_combo->currentText() == "Client" &&
      (advancedUI_->topology_combo->currentText() == "SFU" ||
       advancedUI_->topology_combo->currentText() == "MCU" ||
       advancedUI_->topology_combo->currentText() == "Relay") &&
      (advancedUI_->server_address->text() == ""
        || advancedUI_->server_port->text() == ""))
  {
    advancedUI_->advanced_ok->setEnabled(false);
  }
  else
  {
    advancedUI_->advanced_ok->setEnabled(true);
  }
}
