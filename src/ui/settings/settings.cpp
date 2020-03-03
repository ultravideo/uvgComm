#include "settings.h"

#include "ui_settings.h"

#include <ui/settings/camerainfo.h>
#include <ui/settings/microphoneinfo.h>
#include "settingshelper.h"

#include <common.h>

Settings::Settings(QWidget *parent) :
  QDialog(parent),
  basicUI_(new Ui::BasicSettings),
  cam_(std::shared_ptr<CameraInfo> (new CameraInfo())),
  mic_(std::shared_ptr<MicrophoneInfo> (new MicrophoneInfo())),
  sipSettings_(this),
  mediaSettings_(this, cam_),
  settings_("kvazzup.ini", QSettings::IniFormat)
{}


Settings::~Settings()
{
  // I believe the UI:s are destroyed when parents are destroyed
}


void Settings::init()
{
  basicUI_->setupUi(this);

  // Checks that settings values are correct for the program to start. Also sets GUI.
  getSettings(false);

  int videoID = getDeviceID(basicUI_->videoDevice, "video/DeviceID", "video/Device");

  mediaSettings_.init(videoID);
  sipSettings_.init();

  //QObject::connect(basicUI_->save, &QPushButton::clicked, this, &Settings::on_ok_clicked);
  //QObject::connect(basicUI_->close, &QPushButton::clicked, this, &Settings::on_cancel_clicked);

  QObject::connect(&mediaSettings_, &MediaSettings::customSettingsChanged,
                   this, &Settings::settingsChanged);
  QObject::connect(&mediaSettings_, &MediaSettings::hidden, this, &Settings::show);

  QObject::connect(&sipSettings_, &SIPSettings::advancedSettingsChanged,
                   this, &Settings::settingsChanged);
  QObject::connect(&sipSettings_, &SIPSettings::hidden,
                   this, &Settings::show);

  QObject::connect(basicUI_->serverAddress, &QLineEdit::textChanged,
                   this, &Settings::changedSIPText);
  QObject::connect(basicUI_->username, &QLineEdit::textChanged,
                   this, &Settings::changedSIPText);
}


void Settings::show()
{
  // initialize everytime in case they have changed
  initDeviceSelector(basicUI_->videoDevice, "video/DeviceID", "video/Device", cam_);
  initDeviceSelector(basicUI_->audioDevice, "audio/DeviceID", "audio/Device", mic_);
  QWidget::show();
}


void Settings::on_save_clicked()
{
  printNormal(this, "Saving settings");
  // The UI values are saved to settings.
  saveSettings();
  emit settingsChanged(); // TODO: check have the settings actually been changed
}


void Settings::on_close_clicked()
{
  printNormal(this, "Closing. Gettings recorded settings");

  // discard UI values and restore the settings from file
  getSettings(false);
  hide();
}


void Settings::on_advanced_settings_button_clicked()
{
  saveSettings();
  hide();
  sipSettings_.show();
}


void Settings::on_custom_settings_button_clicked()
{
  saveSettings();
  hide();
  mediaSettings_.show();
}



// records the settings
void Settings::saveSettings()
{
  printNormal(this, "Recording settings");

  // Local settings
  saveTextValue("local/Name", basicUI_->name_edit->text(), settings_);
  saveTextValue("local/Username", basicUI_->username->text(), settings_);
  saveTextValue("sip/ServerAddress", basicUI_->serverAddress->text(), settings_);

  saveCheckBox("sip/AutoConnect", basicUI_->autoConnect, settings_);

  saveCheckBox("sip/kvzRTP", basicUI_->kvzRTP, settings_);

  saveDevice(basicUI_->videoDevice, "video/DeviceID", "video/Device", true);
  saveDevice(basicUI_->audioDevice, "audio/DeviceID", "audio/Device", false);
}


// restores recorded settings
void Settings::getSettings(bool changedDevice)
{
  initDeviceSelector(basicUI_->videoDevice, "video/DeviceID", "video/Device", cam_);
  initDeviceSelector(basicUI_->audioDevice, "audio/DeviceID", "audio/Device", mic_);

  //get values from QSettings
  if(checkMissingValues() && checkUserSettings())
  {
    qDebug() << "Settings," << metaObject()->className()
             << ": Restoring user settings from file:" << settings_.fileName();
    basicUI_->name_edit->setText      (settings_.value("local/Name").toString());
    basicUI_->username->setText  (settings_.value("local/Username").toString());

    basicUI_->serverAddress->setText(settings_.value("sip/ServerAddress").toString());

    restoreCheckBox("sip/AutoConnect", basicUI_->autoConnect, settings_);

    // updates the sip text label
    changedSIPText("");

    restoreCheckBox("sip/kvzrtp", basicUI_->kvzRTP, settings_);

    int videoIndex = getDeviceID(basicUI_->videoDevice, "video/DeviceID", "video/Device");
    if(changedDevice)
    {
      mediaSettings_.changedDevice(videoIndex);
    }
    basicUI_->videoDevice->setCurrentIndex(videoIndex);

    // set correct entry for microphone selector
    int audioIndex = getDeviceID(basicUI_->audioDevice, "audio/DeviceID", "audio/Device");
    if (basicUI_->audioDevice->count() != 0)
    {
      if (audioIndex != -1)
      {
        basicUI_->audioDevice->setCurrentIndex(audioIndex);
      }
      else
      {
        basicUI_->audioDevice->setCurrentIndex(0);
      }
    }
  }
  else
  {
    resetFaultySettings();
  }
}


void Settings::resetFaultySettings()
{
  qDebug() << "WARNING," << metaObject()->className()
           << ": Could not restore settings because they were corrupted!";

  printWarning(this, "Resettings settings from UI");

  // record GUI settings in hope that they are correct ( is case by default )
  saveSettings();
  mediaSettings_.resetSettings(getDeviceID(basicUI_->videoDevice, "video/DeviceID", "video/Device"));
}


void Settings::initDeviceSelector(QComboBox* deviceSelector,
                                  QString settingID,
                                  QString settingsDevice,
                                  std::shared_ptr<DeviceInfoInterface> interface)
{
  deviceSelector->clear();
  QStringList devices = interface->getDeviceList();
  for(int i = 0; i < devices.size(); ++i)
  {
    deviceSelector->addItem( devices[i]);
  }
  int deviceIndex = getDeviceID(deviceSelector, settingID, settingsDevice);

  if(deviceIndex >= deviceSelector->count())
  {
    deviceSelector->setCurrentIndex(0);
  }
  else
  {
    deviceSelector->setCurrentIndex(deviceIndex);
  }

  printDebug(DEBUG_NORMAL, this, "Added devices to selector",
      {"Number of items", "SettingsID"}, {QString::number(deviceSelector->count()), settingID});
}


int Settings::getDeviceID(QComboBox* deviceSelector, QString settingID, QString settingsDevice)
{
  Q_ASSERT(deviceSelector);
  QString deviceName = settings_.value(settingsDevice).toString();

  int deviceIndex = deviceSelector->findText(deviceName);
  int deviceID = settings_.value(settingID).toInt();

//  printDebug(DEBUG_NORMAL, this, "Getting device ID from selector list",
//      {"SettingsID", "DeviceName", "List Index", "Number of items"},
//      {settingID, deviceName, QString::number(deviceIndex), QString::number(deviceSelector->count())});

  // if the device exists in list
  if(deviceIndex != -1 && deviceSelector->count() != 0)
  {
    // if we have multiple devices with same name we use id
    if(deviceID != deviceIndex
       && deviceSelector->itemText(deviceID) == settings_.value(settingsDevice).toString())
    {
      return deviceID;
    }
    else
    {
      // the recorded info was false and our found device is chosen
      settings_.setValue(settingID, deviceIndex);
      return deviceIndex;
    }
  } // if there are devices available, use first
  else if(deviceSelector->count() != 0)
  {
    // could not find the device. Choosing first one
    settings_.setValue(settingID, 0);
    return 0;
  }

  // no devices attached
  return -1;
}


void Settings::saveDevice(QComboBox* deviceSelector, QString settingsID, QString settingsDevice, bool video)
{
  int currentIndex = deviceSelector->currentIndex();
  if( currentIndex != -1)
  {
    if(deviceSelector->currentText() != settings_.value(settingsDevice))
    {
      settings_.setValue(settingsDevice,        deviceSelector->currentText());
      // set capability to first

      if (video)
      {
        mediaSettings_.changedDevice(currentIndex);
      }
    }
    else if(basicUI_->videoDevice->currentIndex() != settings_.value(settingsID))
    {
      if (video)
      {
        mediaSettings_.changedDevice(currentIndex);
      }
    }

    // record index in all cases
    settings_.setValue(settingsID,      currentIndex);
  }
}


void Settings::changedSIPText(const QString &text)
{
  Q_UNUSED(text);
  basicUI_->sipAddress->setText("sip:" + basicUI_->username->text()
                                + "@" + basicUI_->serverAddress->text());
}


void Settings::updateServerStatus(QString status)
{
  basicUI_->status->setText(status);
}


bool Settings::checkUserSettings()
{
  return settings_.contains("local/Name")
      && settings_.contains("local/Username");
}


bool Settings::checkMissingValues()
{
  QStringList list = settings_.allKeys();

  bool foundEverything = true;
  for(auto& key : list)
  {
    if(settings_.value(key).isNull() || settings_.value(key) == "")
    {
      printWarning(this, "Missing settings value", {"Key"}, {key});
      foundEverything = false;
    }
  }
  return foundEverything;
}
