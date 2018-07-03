#include "settings.h"

#include "ui_settings.h"

#include <video/camerainfo.h>

#include <QDebug>

Settings::Settings(QWidget *parent) :
  QDialog(parent),
  basicUI_(new Ui::BasicSettings),
  cam_(std::shared_ptr<CameraInfo> (new CameraInfo())),
  custom_(cam_),
  settings_("kvazzup.ini", QSettings::IniFormat)
{
  basicUI_->setupUi(this);

  // initializes the GUI with values or initialize them in case they don't exist
  restoreBasicSettings();

  QObject::connect(basicUI_->ok, SIGNAL(clicked()), this, SLOT(on_ok_clicked()));
  QObject::connect(basicUI_->cancel, SIGNAL(clicked()), this, SLOT(on_cancel_clicked()));
  QObject::connect(basicUI_->advanced_settings_button, SIGNAL(clicked()),
                   this, SLOT(on_advanced_settings_clicked()));

  QObject::connect(&custom_, &CustomSettings::customSettingsChanged, this, &Settings::settingsChanged);
}

Settings::~Settings()
{
  // I believe the UI:s are destroyed when parents are destroyed
}

void Settings::on_ok_clicked()
{
  qDebug() << "Saving basic settings";
  saveBasicSettings();
  emit settingsChanged();
  hide();
}

void Settings::on_cancel_clicked()
{
  qDebug() << "Getting basic settings from system";
  restoreBasicSettings();
  hide();
}

void Settings::on_advanced_settings_clicked()
{
  custom_.show();
  custom_.showAdvancedSettings();
}

// records the settings
void Settings::saveBasicSettings()
{
  qDebug() << "Saving basic Settings";

  // Local settings

  saveTextValue("local/Name", basicUI_->name_edit->text());
  saveTextValue("local/Username", basicUI_->username_edit->text());

  int currentIndex = basicUI_->videoDevice->currentIndex();
  if( currentIndex != -1)
  {
    settings_.setValue("video/DeviceID",      currentIndex);

    if(basicUI_->videoDevice->currentText() != settings_.value("video/Device"))
    {
      settings_.setValue("video/Device",        basicUI_->videoDevice->currentText());
      // set capability to first

      custom_.changedDevice(currentIndex);
    }

    qDebug() << "Recording following device:" << basicUI_->videoDevice->currentText();
  }
}

// restores temporarily recorded settings
void Settings::restoreBasicSettings()
{
  //get values from QSettings
  if(checkMissingValues() && checkUserSettings())
  {
    qDebug() << "Restoring previous Basic settings from file:" << settings_.fileName();
    basicUI_->name_edit->setText      (settings_.value("local/Name").toString());
    basicUI_->username_edit->setText  (settings_.value("local/Username").toString());

    basicUI_->videoDevice->setCurrentIndex(getVideoDeviceID());
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
  saveBasicSettings();
  custom_.resetSettings();
}

void Settings::showBasicSettings()
{
  initializeDeviceList();
  restoreBasicSettings();
  show();
}

void Settings::initializeDeviceList()
{
  qDebug() << "Initialize device list";
  basicUI_->videoDevice->clear();
  QStringList videoDevices = cam_->getVideoDevices();
  for(int i = 0; i < videoDevices.size(); ++i)
  {
    basicUI_->videoDevice->addItem( videoDevices[i]);
  }
}

QStringList Settings::getAudioDevices()
{
  //TODO
  return QStringList();
}

int Settings::getVideoDeviceID()
{
  initializeDeviceList();

  int deviceIndex = basicUI_->videoDevice->findText(settings_.value("video/Device").toString());
  int deviceID = settings_.value("video/DeviceID").toInt();

  qDebug() << "deviceIndex:" << deviceIndex << "deviceID:" << deviceID;
  qDebug() << "deviceName:" << settings_.value("video/Device").toString();
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
  }
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
      && settings_.contains("local/Username");
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
