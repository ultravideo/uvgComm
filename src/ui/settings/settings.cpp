#include "settings.h"

#include "ui_settings.h"

#include <ui/settings/camerainfo.h>
#include <ui/settings/microphoneinfo.h>
#include <ui/settings/screeninfo.h>
#include "settingshelper.h"

#include "settingskeys.h"

#include "common.h"
#include "logger.h"

#include <QScreen>
#include <QCryptographicHash>
#include <QUuid>


const QStringList neededSettings = {SettingsKey::localRealname,
                                    SettingsKey::localUsername,
                                    SettingsKey::sipServerAddress,
                                    SettingsKey::sipAutoConnect,
                                    SettingsKey::manualSettings};

Settings::Settings(QWidget *parent) :
  QDialog(parent),
  basicUI_(new Ui::BasicSettings),
  cam_(std::shared_ptr<CameraInfo> (new CameraInfo())),
  mic_(std::shared_ptr<MicrophoneInfo> (new MicrophoneInfo())),
  screen_(std::shared_ptr<ScreenInfo> (new ScreenInfo())),
  sipSettings_(this),
  videoSettings_(this, cam_),
  audioSettings_(this, mic_),
  autoSettings_(this),
  settings_(settingsFile, settingsFileFormat)
{}


Settings::~Settings()
{
  // I believe the UI:s are destroyed when parents are destroyed
}


VideoWidget* Settings::getSelfViews()
{
  return autoSettings_.getRoiSelfView();
}


void Settings::init()
{ 
  basicUI_->setupUi(this);

  setWindowTitle("Kvazzup Settings");

  // makes sure the settings are valid and resets them to defaults if necessary
  defaults_.validateSettings(mic_, cam_);

  // Checks that settings values are correct for the program to start. Also sets GUI.
  getSettings(false);

  int videoID    = getDeviceID(basicUI_->videoDevice_combo, SettingsKey::videoDeviceID,
                               SettingsKey::videoDevice);
  int audioIndex = getDeviceID(basicUI_->audioDevice_combo, SettingsKey::audioDeviceID,
                               SettingsKey::audioDevice);

  audioSettings_.init(audioIndex);
  videoSettings_.init(videoID);
  sipSettings_.init();

  checkUUID(); // makes sure that we have an uuid and if not, creates one

  //QObject::connect(basicUI_->save, &QPushButton::clicked, this, &Settings::on_ok_clicked);
  //QObject::connect(basicUI_->close, &QPushButton::clicked, this, &Settings::on_cancel_clicked);

  // video settings
  QObject::connect(&videoSettings_, &VideoSettings::updateVideoSettings,
                   this, &Settings::updateVideoSettings);
  QObject::connect(&videoSettings_, &VideoSettings::hidden, this, &Settings::show);
  QObject::connect(basicUI_->video_settings_button, &QCheckBox::clicked,
                   this, &Settings::openVideoSettings);

  // audio settings
  QObject::connect(&audioSettings_, &AudioSettings::updateAudioSettings,
                   this,            &Settings::updateAudioSettings);
  QObject::connect(&audioSettings_, &AudioSettings::hidden,
                   this,            &Settings::show);
  QObject::connect(basicUI_->audio_settings_button, &QCheckBox::clicked,
                   this,                            &Settings::openAudioSettings);

  // call settings
  QObject::connect(&sipSettings_, &SIPSettings::updateCallSettings,
                   this,          &Settings::updateCallSettings);
  QObject::connect(&sipSettings_, &SIPSettings::hidden,
                   this,          &Settings::show);
  QObject::connect(basicUI_->sip_settings_button, &QCheckBox::clicked,
                   this,                          &Settings::openCallSettings);

  // automatic settings
  QObject::connect(&autoSettings_, &AutomaticSettings::updateAutomaticSettings,
                   this,          &Settings::updateAutomaticSettings);
  QObject::connect(basicUI_->media_settings_button, &QCheckBox::clicked,
                   this,                            &Settings::openAutomaticSettings);
  QObject::connect(&autoSettings_, &AutomaticSettings::hidden,
                   this,           &Settings::show);
  QObject::connect(&autoSettings_, &AutomaticSettings::updateVideoSettings,
                   this,           &Settings::updateVideoSettings);



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

  QObject::connect(basicUI_->passwd_edit, &QLineEdit::textChanged,
                   this, &Settings::uiChangedString);

  QObject::connect(basicUI_->auto_connect_box, &QCheckBox::stateChanged,
                   this,                       &Settings::uiChangedBool);

  QObject::connect(basicUI_->show_manual_box, &QCheckBox::stateChanged,
                   this,                       &Settings::uiChangedBool);

  QObject::connect(basicUI_->show_manual_box, &QCheckBox::stateChanged,
                   this,                      &Settings::showManual);

  QObject::connect(basicUI_->videoDevice_combo, &QComboBox::currentTextChanged,
                   this, &Settings::uiChangedString);
  QObject::connect(basicUI_->audioDevice_combo, &QComboBox::currentTextChanged,
                   this, &Settings::uiChangedString);
  QObject::connect(basicUI_->screenDevice_combo, &QComboBox::currentTextChanged,
                   this, &Settings::uiChangedString);

  // we must initialize the settings if they do not exist
  if (!settings_.value(SettingsKey::micStatus).isValid())
  {
    setMicState(true);
  }

  if (!settings_.value(SettingsKey::cameraStatus).isValid())
  {
    setCameraState(true);
  }

  // never start with screen sharing turned on
  setScreenShareState(false);

  // TODO: Also record the position of closed settings window and move the window there when shown again
}


void Settings::show()
{
  Logger::getLogger()->printNormal(this, "Opening settings");

  defaults_.validateSettings(mic_, cam_);

  // initialize everytime in case they have changed
  initDeviceSelector(basicUI_->videoDevice_combo, SettingsKey::videoDeviceID,
                     SettingsKey::videoDevice, cam_);
  initDeviceSelector(basicUI_->audioDevice_combo, SettingsKey::audioDeviceID,
                     SettingsKey::audioDevice, mic_);
  initDeviceSelector(basicUI_->screenDevice_combo, SettingsKey::userScreenID,
                     SettingsKey::userScreen, screen_);

  QWidget::show();
  basicUI_->save->hide();
}


void Settings::setMicState(bool enabled)
{
  if (enabled)
  {
    settings_.setValue(SettingsKey::micStatus, "1");
  }
  else
  {
    settings_.setValue(SettingsKey::micStatus, "0");
  }
}


void Settings::setCameraState(bool enabled)
{
  if (enabled)
  {
    settings_.setValue(SettingsKey::cameraStatus, "1");
  }
  else
  {
    settings_.setValue(SettingsKey::cameraStatus, "0");
  }
}


void Settings::setScreenShareState(bool enabled)
{
  if (enabled)
  {
    int screenIndex = getDeviceID(basicUI_->screenDevice_combo, SettingsKey::userScreenID,
                                  SettingsKey::userScreen);

    if (screenIndex < QGuiApplication::screens().size())
    {
      QScreen *screen = QGuiApplication::screens().at(screenIndex);

      if (screen != nullptr)
      {
        QSize resolution;
        resolution.setWidth(screen->size().width() - screen->size().width()%8);
        resolution.setHeight(screen->size().height() - screen->size().height()%8);

        settings_.setValue(SettingsKey::screenShareStatus, "1");
        settings_.setValue(SettingsKey::videoResultionWidth, resolution.width());
        settings_.setValue(SettingsKey::videoResultionHeight, resolution.height());

        settings_.setValue(SettingsKey::videoFramerateNumerator, "10");
        settings_.setValue(SettingsKey::videoFramerateDenominator, "1");
        videoSettings_.setScreenShareState(enabled);

        Logger::getLogger()->printNormal(this, "Enabled Screen sharing", "Screen resolution",
                                         QString::number(resolution.width()) + "x" + QString::number(resolution.height()));
      }
    }
  }
  else
  {
    settings_.setValue(SettingsKey::screenShareStatus, "0");
    videoSettings_.setScreenShareState(enabled);
    videoSettings_.saveCameraCapabilities(getDeviceID(basicUI_->videoDevice_combo,
                                                      SettingsKey::videoDeviceID,
                                                      SettingsKey::videoDevice), !enabled);
  }
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


void Settings::showManual(bool state)
{
  if (state)
  {
    basicUI_->video_settings_button->show();
    basicUI_->audio_settings_button->show();
  }
  else
  {
    basicUI_->video_settings_button->hide();
    basicUI_->audio_settings_button->hide();
  }
}

void Settings::closeEvent(QCloseEvent *event)
{
  on_close_clicked();
  QDialog::closeEvent(event);
}


void Settings::on_save_clicked()
{
  Logger::getLogger()->printNormal(this, "Saving settings");
  // The UI values are saved to settings.
  saveSettings();
  setScreenShareState(settingEnabled(SettingsKey::screenShareStatus));

  emit updateCallSettings();
  emit updateVideoSettings();
  emit updateAudioSettings();

  basicUI_->save->hide();
}


void Settings::on_close_clicked()
{
  Logger::getLogger()->printNormal(this, "Closing Settings. Gettings settings from file.");

  if (checkSettingsList(settings_, neededSettings))
  {
    // discard UI values and restore the settings from file
    getSettings(false);
    hide();
  }
}


void Settings::openCallSettings()
{
  saveSettings();
  hide();
  sipSettings_.show();
}


void Settings::openVideoSettings()
{
  saveSettings();
  hide();
  videoSettings_.show();
}


void Settings::openAudioSettings()
{
  saveSettings();
  hide();
  audioSettings_.show();
}


void Settings::openAutomaticSettings()
{
  saveSettings();
  hide();
  autoSettings_.show();
}


// records the settings
void Settings::saveSettings()
{
  Logger::getLogger()->printNormal(this, "Recording settings");

  // Local settings
  saveTextValue(SettingsKey::localRealname, basicUI_->name_edit->text(), settings_);
  saveTextValue(SettingsKey::localUsername, basicUI_->username_edit->text(), settings_);
  saveTextValue(SettingsKey::sipServerAddress, basicUI_->serverAddress_edit->text(), settings_);
  if (basicUI_->passwd_edit->text() != "")
  {
    QCryptographicHash hash(QCryptographicHash::Md5);
    QString qopString = basicUI_->username_edit->text() + ":" +
        basicUI_->serverAddress_edit->text() +  ":" + basicUI_->passwd_edit->text();
    hash.addData(qopString.toLatin1());

    QString credentials = hash.result().toHex();

    saveTextValue("local/Credentials", credentials, settings_);
  }

  saveCheckBox(SettingsKey::sipAutoConnect, basicUI_->auto_connect_box, settings_);
  saveCheckBox(SettingsKey::manualSettings, basicUI_->show_manual_box, settings_);

  saveDevice(basicUI_->videoDevice_combo, SettingsKey::videoDeviceID,
             SettingsKey::videoDevice, D_VIDEO);
  saveDevice(basicUI_->audioDevice_combo, SettingsKey::audioDeviceID,
             SettingsKey::audioDevice, D_AUDIO);
  saveDevice(basicUI_->screenDevice_combo, SettingsKey::userScreenID,
             SettingsKey::userScreen, D_SCREEN);
}


// restores recorded settings
void Settings::getSettings(bool changedDevice)
{
  initDeviceSelector(basicUI_->videoDevice_combo, SettingsKey::videoDeviceID,
                     SettingsKey::videoDevice, cam_);
  initDeviceSelector(basicUI_->audioDevice_combo, SettingsKey::audioDeviceID,
                     SettingsKey::audioDevice, mic_);
  initDeviceSelector(basicUI_->screenDevice_combo, SettingsKey::userScreenID,
                     SettingsKey::userScreen, screen_);

  //get values from QSettings
  if (checkSettingsList(settings_, neededSettings))
  {
    Logger::getLogger()->printNormal(this, "Loading settings from file", {"File"}, {settings_.fileName()});

    basicUI_->name_edit->setText      (settings_.value(SettingsKey::localRealname).toString());
    basicUI_->username_edit->setText  (settings_.value(SettingsKey::localUsername).toString());

    basicUI_->serverAddress_edit->setText(settings_.value(SettingsKey::sipServerAddress).toString());

    restoreCheckBox(SettingsKey::sipAutoConnect, basicUI_->auto_connect_box, settings_);
    restoreCheckBox(SettingsKey::manualSettings, basicUI_->show_manual_box, settings_);

    // set the UI in correct state
    showManual(basicUI_->show_manual_box->isChecked());

    // updates the sip text label
    changedSIPText("");

    // set index for camera
    int videoIndex = getDeviceID(basicUI_->videoDevice_combo, SettingsKey::videoDeviceID,
                                 SettingsKey::videoDevice);
    if(changedDevice)
    {
      defaults_.setDefaultVideoSettings(cam_);
      videoSettings_.changedDevice(videoIndex);
    }
    basicUI_->videoDevice_combo->setCurrentIndex(videoIndex);

    // set correct entry for microphone selector
    int audioIndex = getDeviceID(basicUI_->audioDevice_combo, SettingsKey::audioDeviceID,
                                 SettingsKey::audioDevice);
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
    int screenIndex = getDeviceID(basicUI_->screenDevice_combo, SettingsKey::userScreenID,
                                  SettingsKey::userScreen);
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
  Logger::getLogger()->printWarning(this, "Could not restore settings! "
                                          "Resetting settings from defaults.");

  // record GUI settings in hope that they are correct ( is case by default )
  saveSettings();

  // we set the connecting to true at this point because we want two things:
  // 1) that Kvazzup doesn't connect to any server without user permission
  // 2) that connecting to server is default since it is the easiest way to use Kvazzup
  // These two conditions can only be achieved by modifying UI after settings have been saved
  basicUI_->auto_connect_box->setChecked(true);

  sipSettings_.resetSettings();

  // Show resetted settings to user so she can fix them manually
  show();

  sipSettings_.showSTUNQuestion();
}


void Settings::initDeviceSelector(QComboBox* deviceSelector,
                                  QString settingID,
                                  QString settingsDevice,
                                  std::shared_ptr<DeviceInfoInterface> deviceInterface)
{
  deviceSelector->clear();
  QStringList devices = deviceInterface->getDeviceList();
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

  //Logger::getLogger()->printNormal(this, "Added " + 
  //                                 QString::number(deviceSelector->count()) + " devices to selector",
  //                                 {"SettingsID"}, {settingID});
}


int Settings::getDeviceID(QComboBox* deviceSelector, QString settingID,
                          QString settingsDevice)
{
  Q_ASSERT(deviceSelector);
  QString deviceName = settings_.value(settingsDevice).toString();

  int deviceIndex = deviceSelector->findText(deviceName);
  int deviceID = settings_.value(settingID).toInt();

//  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Getting device ID from selector list",
//      {"SettingsID", "DeviceName", "List Index", "Number of items"},
//      {settingID, deviceName, QString::number(deviceIndex),
//       QString::number(deviceSelector->count())});

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
                          QString settingsDevice, DeviceType type)
{
  int currentIndex = deviceSelector->currentIndex();
  if( currentIndex != -1)
  {
    if(deviceSelector->currentText() != settings_.value(settingsDevice).toString())
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "The device name has changed",
                                       {"Old name", "New name"},
                                       {settings_.value(settingsDevice).toString(), deviceSelector->currentText()});

      settings_.setValue(settingsDevice, deviceSelector->currentText());

      // set capability to first

      if (type == D_VIDEO)
      {
        defaults_.setDefaultVideoSettings(cam_);
        videoSettings_.changedDevice(currentIndex);
      }
      else if (type == D_AUDIO)
      {
        defaults_.setDefaultAudioSettings(mic_);
        audioSettings_.changedDevice(currentIndex);
      }
    }
    else if(deviceSelector->currentIndex() != settings_.value(settingsID).toInt())
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "The device ID has changed",
                                       {"Old ID", "New ID"},
                                       {settings_.value(settingsID).toString(),
                                        QString::number(deviceSelector->currentIndex())});

      if (type == D_VIDEO)
      {
        defaults_.setDefaultVideoSettings(cam_);
        videoSettings_.changedDevice(currentIndex);
      }
      else if (type == D_AUDIO)
      {
        defaults_.setDefaultAudioSettings(mic_);
        audioSettings_.changedDevice(currentIndex);
      }
    }

    // record index in all cases
    settings_.setValue(settingsID,      currentIndex);
  }
  else
  {
    // record something so it doesn't give error when checking for missing keys
    settings_.setValue(settingsID,      "-1");
    settings_.setValue(settingsDevice,  "none");
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


void Settings::checkUUID()
{
  if (!settings_.value(SettingsKey::sipUUID).isValid() ||
      settings_.value(SettingsKey::sipUUID).toString().isEmpty())
  {
    QUuid uuid = QUuid::createUuid();
    QString uuid_str = uuid.toString();

    uuid = uuid_str.remove("{");
    uuid = uuid_str.remove("}");

    settings_.setValue(SettingsKey::sipUUID, uuid_str);

    Logger::getLogger()->printNormal(this, "Generated new UUID.", "UUID", uuid_str);
  }
}
