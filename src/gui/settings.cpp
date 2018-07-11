#include "settings.h"

#include "ui_settings.h"

#include <video/camerainfo.h>

#include <QDebug>

Settings::Settings(QWidget *parent) :
  QDialog(parent),
  basicUI_(new Ui::BasicSettings),
  cam_(std::shared_ptr<CameraInfo> (new CameraInfo())),
  custom_(this, cam_),
  settings_("kvazzup.ini", QSettings::IniFormat)
{
  basicUI_->setupUi(this);

  // Checks that settings values are correct for the program to start. Also sets GUI.
  getSettings(true);

  custom_.init(getVideoDeviceID());

  QObject::connect(basicUI_->ok, &QPushButton::clicked, this, &Settings::on_ok_clicked);
  QObject::connect(basicUI_->cancel, &QPushButton::clicked, this, &Settings::on_cancel_clicked);

  QObject::connect(&custom_, &CustomSettings::customSettingsChanged, this, &Settings::settingsChanged);
  QObject::connect(&custom_, &CustomSettings::hidden, this, &Settings::show);
}

Settings::~Settings()
{
  // I believe the UI:s are destroyed when parents are destroyed
}

void Settings::show()
{
  initializeUIDeviceList(); // initialize everytime in case they have changed
  QWidget::show();
}

void Settings::on_ok_clicked()
{
  qDebug() << "Saving basic settings";
  // The UI values are saved to settings.
  saveSettings();
  emit settingsChanged(); // TODO: check have the settings actually been changed
  hide();
}

void Settings::on_cancel_clicked()
{
  qDebug() << "Settings Cancel clicked. Getting settings from system";
  // discard UI values and restore the settings from file
  getSettings(false);
  hide();
}

void Settings::on_advanced_settings_button_clicked()
{
  on_ok_clicked(); // treat this the same as ok
  custom_.show();
}

void Settings::initializeUIDeviceList()
{
  qDebug() << "Initialize device list";
  basicUI_->videoDevice->clear();
  QStringList videoDevices = cam_->getVideoDevices();
  for(int i = 0; i < videoDevices.size(); ++i)
  {
    basicUI_->videoDevice->addItem( videoDevices[i]);
  }
  int deviceIndex = getVideoDeviceID();

  if(deviceIndex >= basicUI_->videoDevice->count())
  {
    basicUI_->videoDevice->setCurrentIndex(0);
  }
  else
  {
    basicUI_->videoDevice->setCurrentIndex(deviceIndex);
  }
}

// records the settings
void Settings::saveSettings()
{
  qDebug() << "Saving basic Settings";

  // Local settings
  saveTextValue("local/Name", basicUI_->name_edit->text());
  saveTextValue("local/Username", basicUI_->username_edit->text());
  saveCheckBox("local/Auto-Accept", basicUI_->auto_accept);

  int currentIndex = basicUI_->videoDevice->currentIndex();
  if( currentIndex != -1)
  {
    if(basicUI_->videoDevice->currentText() != settings_.value("video/Device"))
    {
      settings_.setValue("video/Device",        basicUI_->videoDevice->currentText());
      // set capability to first

      custom_.changedDevice(currentIndex);
    }
    else if(basicUI_->videoDevice->currentIndex() != settings_.value("video/DeviceID"))
    {
      custom_.changedDevice(currentIndex);
    }

    // record index in all cases
    settings_.setValue("video/DeviceID",      currentIndex);

    qDebug() << "Recording following device:" << basicUI_->videoDevice->currentText();
  }
}

// restores recorded settings
void Settings::getSettings(bool changedDevice)
{
  initializeUIDeviceList();

  //get values from QSettings
  if(checkMissingValues() && checkUserSettings())
  {
    qDebug() << "Restoring previous Basic settings from file:" << settings_.fileName();
    basicUI_->name_edit->setText      (settings_.value("local/Name").toString());
    basicUI_->username_edit->setText  (settings_.value("local/Username").toString());
    restoreCheckBox("local/Auto-Accept", basicUI_->auto_accept);

    int currentIndex = getVideoDeviceID();
    if(changedDevice)
    {
      custom_.changedDevice(currentIndex);
    }
    basicUI_->videoDevice->setCurrentIndex(currentIndex);
  }
  else
  {
    resetFaultySettings();
  }
}

void Settings::resetFaultySettings()
{
  qDebug() << "Could not restore settings because they were corrupted!";
  // record GUI settings in hope that they are correct ( is case by default )
  saveSettings();
  custom_.resetSettings(getVideoDeviceID());
}

QStringList Settings::getAudioDevices()
{
  //TODO
  return QStringList();
}

int Settings::getVideoDeviceID()
{
  int deviceIndex = basicUI_->videoDevice->findText(settings_.value("video/Device").toString());
  int deviceID = settings_.value("video/DeviceID").toInt();

  qDebug() << "deviceIndex:" << deviceIndex << "deviceID:" << deviceID;
  qDebug() << "deviceName:" << settings_.value("video/Device").toString();

  // if the device exists in list
  if(deviceIndex != -1 && basicUI_->videoDevice->count() != 0)
  {
    // if we have multiple devices with same name we use id
    if(deviceID != deviceIndex
       && basicUI_->videoDevice->itemText(deviceID) == settings_.value("video/Device").toString())
    {
      return deviceID;
    }
    else
    {
      // the recorded info was false and our found device is chosen
      settings_.setValue("video/DeviceID", deviceIndex);
      return deviceIndex;
    }
  } // if there are devices available, use first
  else if(basicUI_->videoDevice->count() != 0)
  {
    // could not find the device. Choosing first one
    settings_.setValue("video/DeviceID", 0);
    return 0;
  }

  // no devices attached
  return -1;
}

bool Settings::checkUserSettings()
{
  return settings_.contains("local/Name")
      && settings_.contains("local/Username")
      && settings_.contains("local/Auto-Accept");
}

bool Settings::checkMissingValues()
{
  QStringList list = settings_.allKeys();

  bool foundEverything = true;
  for(auto key : list)
  {
    if(settings_.value(key).isNull() || settings_.value(key) == "")
    {
      qDebug() << "MISSING SETTING FOR:" << key;
      foundEverything = false;
    }
  }
  return foundEverything;
}


void Settings::saveTextValue(const QString settingValue, const QString &text)
{
  if(text != "")
  {
    settings_.setValue(settingValue,  text);
  }
}

void Settings::restoreCheckBox(const QString settingValue, QCheckBox* box)
{
  if(settings_.value(settingValue).toString() == "1")
  {
    box->setChecked(true);
  }
  else if(settings_.value(settingValue).toString() == "0")
  {
    box->setChecked(false);
  }
  else
  {
    qDebug() << "Corrupted value for checkbox in settings file for:" << settingValue << "!!!";
  }
}

void Settings::saveCheckBox(const QString settingValue, QCheckBox* box)
{
  if(box->isChecked())
  {
    settings_.setValue(settingValue,          "1");
  }
  else
  {
    settings_.setValue(settingValue,          "0");
  }
}
