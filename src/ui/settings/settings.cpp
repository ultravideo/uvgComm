#include "settings.h"

#include "ui_settings.h"

#include <ui/settings/camerainfo.h>
#include <ui/settings/microphoneinfo.h>
#include <ui/settings/screeninfo.h>
#include "settingshelper.h"

#include <common.h>

Settings::Settings(QWidget *parent) :
  QDialog(parent),
  basicUI_(new Ui::BasicSettings),
  cam_(std::shared_ptr<CameraInfo> (new CameraInfo())),
  mic_(std::shared_ptr<MicrophoneInfo> (new MicrophoneInfo())),
  screen_(std::shared_ptr<ScreenInfo> (new ScreenInfo())),
  sipSettings_(this),
  videoSettings_(this, cam_),
  audioSettings_(this, mic_),
  settings_("kvazzup.ini", QSettings::IniFormat)
{}


Settings::~Settings()
{
  // I believe the UI:s are destroyed when parents are destroyed
}


void Settings::init()
{ 
  basicUI_->setupUi(this);

  QString deviceName = settings_.value("video/Device").toString();

  // Checks that settings values are correct for the program to start. Also sets GUI.
  getSettings(false);

  int videoID = getDeviceID(basicUI_->videoDevice_combo, "video/DeviceID", "video/Device");
  int audioIndex = getDeviceID(basicUI_->audioDevice_combo, "audio/DeviceID", "audio/Device");

  audioSettings_.init(audioIndex);
  videoSettings_.init(videoID);
  sipSettings_.init();

  //QObject::connect(basicUI_->save, &QPushButton::clicked, this, &Settings::on_ok_clicked);
  //QObject::connect(basicUI_->close, &QPushButton::clicked, this, &Settings::on_cancel_clicked);

  QObject::connect(&videoSettings_, &VideoSettings::settingsChanged,
                   this, &Settings::settingsChanged);
  QObject::connect(&videoSettings_, &VideoSettings::hidden, this, &Settings::show);

  QObject::connect(&audioSettings_, &AudioSettings::settingsChanged,
                   this, &Settings::settingsChanged);
  QObject::connect(&audioSettings_, &AudioSettings::hidden, this, &Settings::show);

  QObject::connect(&sipSettings_, &SIPSettings::advancedSettingsChanged,
                   this, &Settings::settingsChanged);
  QObject::connect(&sipSettings_, &SIPSettings::hidden,
                   this, &Settings::show);

  QObject::connect(basicUI_->serverAddress_edit, &QLineEdit::textChanged,
                   this, &Settings::changedSIPText);
  QObject::connect(basicUI_->username_edit, &QLineEdit::textChanged,
                   this, &Settings::changedSIPText);

  QObject::connect(basicUI_->serverAddress_edit, &QLineEdit::textChanged,
                   this, &Settings::uiChangedString);

  QObject::connect(basicUI_->name_edit, &QLineEdit::textChanged,
                   this, &Settings::uiChangedString);

  QObject::connect(basicUI_->username_edit, &QLineEdit::textChanged,
                   this, &Settings::uiChangedString);

  QObject::connect(basicUI_->auto_connect_box, &QCheckBox::stateChanged,
                   this, &Settings::uiChangedBool);

  QObject::connect(basicUI_->videoDevice_combo, &QComboBox::currentTextChanged,
                   this, &Settings::uiChangedString);
  QObject::connect(basicUI_->audioDevice_combo, &QComboBox::currentTextChanged,
                   this, &Settings::uiChangedString);
  QObject::connect(basicUI_->screenDevice_combo, &QComboBox::currentTextChanged,
                   this, &Settings::uiChangedString);

  // TODO: Also emit the position of closed window and move this setting there
}


void Settings::show()
{
  // initialize everytime in case they have changed
  initDeviceSelector(basicUI_->videoDevice_combo, "video/DeviceID", "video/Device", cam_);
  initDeviceSelector(basicUI_->audioDevice_combo, "audio/DeviceID", "audio/Device", mic_);
  initDeviceSelector(basicUI_->screenDevice_combo, "user/ScreenID", "user/Screen", screen_);

  QWidget::show();
  basicUI_->save->hide();
}


void Settings::uiChangedString(QString text)
{
  Q_UNUSED(text);
  basicUI_->save->show();
}


void Settings::uiChangedBool(bool state)
{
  Q_UNUSED(state);
  basicUI_->save->show();
}


void Settings::on_save_clicked()
{
  printNormal(this, "Saving settings");
  // The UI values are saved to settings.
  saveSettings();
  emit settingsChanged();
  basicUI_->save->hide();
}


void Settings::on_close_clicked()
{
  printNormal(this, "Closing. Gettings recorded settings");

  if (checkMissingValues() && checkUserSettings())
  {
    // discard UI values and restore the settings from file
    getSettings(false);
    hide();
  }
}


void Settings::on_sip_settings_button_clicked()
{
  saveSettings();
  hide();
  sipSettings_.show();
}


void Settings::on_video_settings_button_clicked()
{
  saveSettings();
  hide();
  videoSettings_.show();
}


void Settings::on_audio_settings_button_clicked()
{
  saveSettings();
  hide();
  audioSettings_.show();
}

// records the settings
void Settings::saveSettings()
{
  printNormal(this, "Recording settings");

  // Local settings
  saveTextValue("local/Name", basicUI_->name_edit->text(), settings_);
  saveTextValue("local/Username", basicUI_->username_edit->text(), settings_);
  saveTextValue("sip/ServerAddress", basicUI_->serverAddress_edit->text(), settings_);

  saveCheckBox("sip/AutoConnect", basicUI_->auto_connect_box, settings_);

  saveDevice(basicUI_->videoDevice_combo, "video/DeviceID", "video/Device", true);
  saveDevice(basicUI_->audioDevice_combo, "audio/DeviceID", "audio/Device", false);
  saveDevice(basicUI_->screenDevice_combo, "user/ScreenID", "user/Screen", false);
}


// restores recorded settings
void Settings::getSettings(bool changedDevice)
{
  initDeviceSelector(basicUI_->videoDevice_combo, "video/DeviceID", "video/Device", cam_);
  initDeviceSelector(basicUI_->audioDevice_combo, "audio/DeviceID", "audio/Device", mic_);
  initDeviceSelector(basicUI_->screenDevice_combo, "user/ScreenID", "user/Screen", screen_);

  //get values from QSettings
  if(checkMissingValues() && checkUserSettings())
  {
    printNormal(this, "Loading settings from file", {"File"}, {settings_.fileName()});

    basicUI_->name_edit->setText      (settings_.value("local/Name").toString());
    basicUI_->username_edit->setText  (settings_.value("local/Username").toString());

    basicUI_->serverAddress_edit->setText(settings_.value("sip/ServerAddress").toString());

    restoreCheckBox("sip/AutoConnect", basicUI_->auto_connect_box, settings_);

    // updates the sip text label
    changedSIPText("");

    // set index for camera
    int videoIndex = getDeviceID(basicUI_->videoDevice_combo, "video/DeviceID", "video/Device");
    if(changedDevice)
    {
      videoSettings_.changedDevice(videoIndex);
    }
    basicUI_->videoDevice_combo->setCurrentIndex(videoIndex);

    // set correct entry for microphone selector
    int audioIndex = getDeviceID(basicUI_->audioDevice_combo, "audio/DeviceID", "audio/Device");
    if (basicUI_->audioDevice_combo->count() != 0)
    {
      if (audioIndex != -1)
      {
        basicUI_->audioDevice_combo->setCurrentIndex(audioIndex);
      }
      else
      {
        basicUI_->audioDevice_combo->setCurrentIndex(0);
      }
    }

    // set index for screen
    int screenIndex = getDeviceID(basicUI_->screenDevice_combo, "user/ScreenID", "user/Screen");
    if (basicUI_->screenDevice_combo->count() != 0)
    {
      if (screenIndex != -1)
      {
        basicUI_->screenDevice_combo->setCurrentIndex(screenIndex);
      }
      else
      {
        basicUI_->screenDevice_combo->setCurrentIndex(0);
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
  printWarning(this, "Could not restore settings! Resetting settings from defaults.");

  // record GUI settings in hope that they are correct ( is case by default )
  saveSettings();
  videoSettings_.resetSettings(getDeviceID(basicUI_->videoDevice_combo, "video/DeviceID", "video/Device"));
  audioSettings_.resetSettings(getDeviceID(basicUI_->audioDevice_combo, "audio/DeviceID", "audio/Device"));

  // we set the connecting to true at this point because we want two things:
  // 1) that Kvazzup doesn't connect to any server without user permission
  // 2) that connecting to server is default since it is the easiest way to use Kvazzup
  // These two conditions can only be achieved by modifying UI after settings have been saved
  basicUI_->auto_connect_box->setChecked(true);
  
  // Show resetted settings to user so she can fix them manually
  show();
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

  printNormal(this, "Added " + QString::number(deviceSelector->count()) + " devices to selector",
      {"SettingsID"}, {settingID});
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


void Settings::saveDevice(QComboBox* deviceSelector, QString settingsID,
                          QString settingsDevice, bool video)
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
        videoSettings_.changedDevice(currentIndex);
      }
      else
      {
        audioSettings_.changedDevice(currentIndex);
      }
    }
    else if(basicUI_->videoDevice_combo->currentIndex() != settings_.value(settingsID))
    {
      if (video)
      {
        videoSettings_.changedDevice(currentIndex);
      }
      else
      {
        audioSettings_.changedDevice(currentIndex);
      }
    }

    // record index in all cases
    settings_.setValue(settingsID,      currentIndex);
  }
}


void Settings::changedSIPText(const QString &text)
{
  Q_UNUSED(text);
  basicUI_->sipAddress->setText("sip:" + basicUI_->username_edit->text()
                                + "@" + basicUI_->serverAddress_edit->text());
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
