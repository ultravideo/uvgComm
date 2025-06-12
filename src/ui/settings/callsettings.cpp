#include "callsettings.h"

#include "ui_callsettings.h"


#include "settingshelper.h"
#include "settingskeys.h"
#include "logger.h"

#include <QDateTime>


struct ResolutionBitrate
{
  int width;
  int height;
  int totalBitrate; // in bits per second
};

const std::map<QString, ResolutionBitrate> RESOLUTIONS =
{
  {"QVGA (240p)",     {320, 240,    300000}},
  {"VGA (480p)",      {640, 480,    500000}},
  {"HD (720p)",       {1280, 720,  1500000}},
  {"Full HD (1080p)", {1920, 1080, 3000000}},
  {"QHD (1440p)",     {2560, 1440, 5000000}},
  {"4K UHD (2160p)",  {3840, 2160, 10000000}}
};

const float TRANSMISSION_OVERHEAD = 0.05f; // 5% overhead for transmission

int CallSettings::getAudioBitrate(int totalBitrate) const
{
  return 24000; // Reserve 24 kbit/s for audio
}


int CallSettings::getVideoBitrate(int totalBitrate) const
{
  // Calculate video bitrate based on total bitrate and overhead
  int usableBitrate = static_cast<int>(totalBitrate * (1.0f - TRANSMISSION_OVERHEAD));

  int videoBitrate = static_cast<int>(usableBitrate - getAudioBitrate(totalBitrate));
  return videoBitrate;
}


int CallSettings::getTotalBitrate(int audioBitrate, int videoBitrate) const
{
  // Calculate total bitrate by summing audio and video bitrates
  return static_cast<int>((audioBitrate + videoBitrate) /(1.0f - TRANSMISSION_OVERHEAD));
}


QString CallSettings::bitrateString(int totalBitrate, int audioBitrate, int videoBitrate) const
{
  // Format bitrate string with total, video, and audio bitrates
  return QString("%1 kbit/s (Video: %2 kbit/s, Audio: %3 kbit/s)")
      .arg(totalBitrate / 1000)
      .arg(videoBitrate / 1000)
      .arg(audioBitrate / 1000);
}

// if one of these is missing, we load the default setting for all of them (I was too lazy to do it individually)
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
                                    SettingsKey::sipSRTP,
                                    SettingsKey::sipUpBandwidth,
                                    SettingsKey::sipDownBandwidth,
                                    SettingsKey::sipConferenceMode,
                                    SettingsKey::sipSpeakerMode,
                                    SettingsKey::sipVisibleParticipants};

CallSettings::CallSettings(QWidget* parent):
  QDialog (parent),
  advancedUI_(new Ui::AdvancedSettings),
  settings_(settingsFile, settingsFileFormat)
{
  advancedUI_->setupUi(this);
  stunQuestion_.setupUi(&stun_);
  stun_.setWindowFlags(Qt::WindowStaysOnTopHint);

  connect(advancedUI_->resolution_combo, &QComboBox::currentTextChanged,
          this, &CallSettings::resolutionComboChanged);
  connect(advancedUI_->up_slider, &QSlider::valueChanged,
          this, &CallSettings::updateBitrateUp);
  connect(advancedUI_->down_slider, &QSlider::valueChanged,
          this, &CallSettings::updateBitrateDown);
}


void CallSettings::init()
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
          this, &CallSettings::showBlocklistContextMenu);

  restoreAdvancedSettings();
}

void CallSettings::showSTUNQuestion()
{
  connect(stunQuestion_.yes_button,  &QPushButton::pressed,
          this,                      &CallSettings::acceptSTUN);

  connect(stunQuestion_.no_button,  &QPushButton::pressed,
          this,                      &CallSettings::declineSTUN);

  // stun_.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
  stun_.show();
  stun_.setWindowTitle("Use STUN?");
}


void CallSettings::closeEvent(QCloseEvent *event)
{
  on_advanced_close_clicked();
  QDialog::closeEvent(event);
}


void CallSettings::acceptSTUN()
{
  Logger::getLogger()->printNormal(this, "User accepted STUN usage");

  advancedUI_->stun_enabled->setChecked(true);
  advancedUI_->stun_address->setText(stunQuestion_.address->text());
  advancedUI_->stun_port->setValue(stunQuestion_.port->value());

  saveCheckBox(SettingsKey::sipSTUNEnabled, advancedUI_->stun_enabled, settings_);
  stun_.hide();
}

void CallSettings::declineSTUN()
{
  Logger::getLogger()->printNormal(this, "User declined STUN usage");
  advancedUI_->stun_enabled->setChecked(false);

  saveCheckBox(SettingsKey::sipSTUNEnabled, advancedUI_->stun_enabled, settings_);
  stun_.hide();
}


void CallSettings::resetSettings()
{
  Logger::getLogger()->printWarning(this, "Resetting default SIP settings from UI");
  // TODO: should we do something to blocked list in settings?

  saveAdvancedSettings();
}


void CallSettings::showBlocklistContextMenu(const QPoint& pos)
{
  if (advancedUI_->blockedUsers->rowCount() != 0)
  {
    showContextMenu(pos, advancedUI_->blockedUsers, this,
                    QStringList() << "Delete", QStringList()
                    << SLOT(deleteBlocklistItem()));
  }
}


void CallSettings::deleteBlocklistItem()
{
  Logger::getLogger()->printNormal(this, "Deleting blocklist row", "Row index",
                                   QString::number(advancedUI_->blockedUsers->currentRow()));

  advancedUI_->blockedUsers->removeRow(advancedUI_->blockedUsers->currentRow());
}


void CallSettings::on_advanced_ok_clicked()
{
  Logger::getLogger()->printNormal(this, "Saving SIP settings");

  saveAdvancedSettings();
  emit updateCallSettings();
}


void CallSettings::on_advanced_close_clicked()
{
  Logger::getLogger()->printNormal(this, "Cancelled modifyin SIP settings. Restoring settings from file");

  restoreAdvancedSettings();
  hide();
  emit hidden();
}


void CallSettings::on_addUserBlock_clicked()
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


void CallSettings::saveAdvancedSettings()
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

  saveTextValue(SettingsKey::sipSTUNAddress, advancedUI_->stun_address->text(), settings_);

  settings_.setValue(SettingsKey::sipSTUNPort,  QString::number(advancedUI_->stun_port->value()));
  settings_.setValue(SettingsKey::sipMediaPort,  QString::number(advancedUI_->media_port->value()));

  if (advancedUI_->resolution_combo->currentText() != "Custom")
  {
    if (RESOLUTIONS.find(advancedUI_->resolution_combo->currentText()) == RESOLUTIONS.end())
    {
      Logger::getLogger()->printError(this, "Invalid resolution found: " + advancedUI_->resolution_combo->currentText());
      return;
    }

    ResolutionBitrate bitrates = RESOLUTIONS.at(advancedUI_->resolution_combo->currentText());
    settings_.setValue(SettingsKey::videoResolutionWidth,  QString::number(bitrates.width));
    settings_.setValue(SettingsKey::videoResolutionHeight, QString::number(bitrates.height));
    settings_.setValue(SettingsKey::videoBitrate, QString::number(getVideoBitrate(bitrates.totalBitrate)));
    settings_.setValue(SettingsKey::audioBitrate, QString::number(getAudioBitrate(bitrates.totalBitrate)));
  }

  QString resolution = advancedUI_->resolution_combo->currentText();

  settings_.setValue(SettingsKey::sipUpBandwidth,  QString::number(advancedUI_->up_slider->value()));
  settings_.setValue(SettingsKey::sipDownBandwidth, QString::number(advancedUI_->down_slider->value()));

  settings_.setValue(SettingsKey::sipConferenceMode, advancedUI_->layout_box->currentText());
  settings_.setValue(SettingsKey::sipSpeakerMode, advancedUI_->speaker_checkbox->isChecked());

  settings_.setValue(SettingsKey::sipVisibleParticipants, advancedUI_->visible_box->value());
}


void CallSettings::restoreAdvancedSettings()
{
  listSettingsToGUI(blocklistFile, SettingsKey::blocklist, QStringList()
                    << "userName" << "date", advancedUI_->blockedUsers);

  int videoBitrate = settings_.value(SettingsKey::videoBitrate).toInt();
  int audioBitrate = settings_.value(SettingsKey::audioBitrate).toInt();
  int resolutionWidth = settings_.value(SettingsKey::videoResolutionWidth).toInt();
  int resolutionHeight = settings_.value(SettingsKey::videoResolutionHeight).toInt();

  QString resolution = "Custom";

  for (const auto& res : RESOLUTIONS)
  {
    if (res.second.width == resolutionWidth &&
        res.second.height == resolutionHeight &&
        getVideoBitrate(res.second.totalBitrate) == videoBitrate &&
        getAudioBitrate(res.second.totalBitrate) == audioBitrate)
    {
      resolution = res.first;
      break;
    }
  }

  int totalBitrate = getTotalBitrate(audioBitrate, videoBitrate);
  QString bitrateLabel = bitrateString(totalBitrate, audioBitrate, videoBitrate);

  advancedUI_->bitrate_value->setText(bitrateLabel);
  advancedUI_->resolution_combo->setCurrentText(resolution);

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

    advancedUI_->stun_address->setText(settings_.value(SettingsKey::sipSTUNAddress).toString());
    advancedUI_->stun_port->setValue  (settings_.value(SettingsKey::sipSTUNPort).toInt());
    advancedUI_->media_port->setValue (settings_.value(SettingsKey::sipMediaPort).toInt());

    advancedUI_->up_slider->setValue(settings_.value(SettingsKey::sipUpBandwidth).toInt());
    advancedUI_->down_slider->setValue(settings_.value(SettingsKey::sipDownBandwidth).toInt());

    advancedUI_->layout_box->setCurrentText(settings_.value(SettingsKey::sipConferenceMode).toString());
    advancedUI_->speaker_checkbox->setChecked(settings_.value(SettingsKey::sipSpeakerMode).toBool());

    advancedUI_->visible_box->setValue(settings_.value(SettingsKey::sipVisibleParticipants).toInt());
  }
  else
  {
    resetSettings();
  }
}


void CallSettings::showOkButton()
{
  if (advancedUI_->role_combo->currentText() == "Client" &&
      (advancedUI_->topology_combo->currentText() == "SFU" ||
       advancedUI_->topology_combo->currentText() == "MCU" ||
       advancedUI_->topology_combo->currentText() == "Relay"))
  {
    advancedUI_->advanced_ok->setEnabled(false);
  }
  else
  {
    advancedUI_->advanced_ok->setEnabled(true);
  }
}


void CallSettings::resolutionComboChanged(const QString& text)
{
  for (const auto& res : RESOLUTIONS)
  {
    if (text == res.first)
    {
      int audioBitrate = getAudioBitrate(res.second.totalBitrate);
      int videoBitrate = getVideoBitrate(res.second.totalBitrate);
      QString bitrateLabel = bitrateString(res.second.totalBitrate,
                                           audioBitrate, videoBitrate);
      advancedUI_->bitrate_value->setText(bitrateLabel);
      break;
    }
  }
}


void CallSettings::updateBitrateUp(int value)
{
  if (value < 300000)
  {
    advancedUI_->up_value->setText(getBitrateString(300000));
    advancedUI_->up_slider->setValue(300000);
  }
  else
  {
    value = roundToNumber(value, 100000);

    advancedUI_->up_value->setText(getBitrateString(value));
    advancedUI_->up_slider->setValue(value);
  }
}


void CallSettings::updateBitrateDown(int value)
{
  if (value < 300000)
  {
    advancedUI_->down_value->setText(getBitrateString(300000));
    advancedUI_->down_slider->setValue(300000);
  }
  else
  {
    value = roundToNumber(value, 100000);

    advancedUI_->down_value->setText(getBitrateString(value));
    advancedUI_->down_slider->setValue(value);
  }
}
