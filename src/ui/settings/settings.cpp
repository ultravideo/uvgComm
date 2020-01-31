#include "settings.h"

#include "ui_settings.h"

#include <ui/settings/camerainfo.h>
#include "settingshelper.h"

#include <QDebug>

Settings::Settings(QWidget *parent) :
  QDialog(parent),
  basicUI_(new Ui::BasicSettings),
  cam_(std::shared_ptr<CameraInfo> (new CameraInfo())),
  advanced_(this),
  custom_(this, cam_),
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

  custom_.init(getVideoDeviceID());
  advanced_.init();

  //QObject::connect(basicUI_->save, &QPushButton::clicked, this, &Settings::on_ok_clicked);
  //QObject::connect(basicUI_->close, &QPushButton::clicked, this, &Settings::on_cancel_clicked);

  QObject::connect(&custom_, &CustomSettings::customSettingsChanged, this, &Settings::settingsChanged);
  QObject::connect(&custom_, &CustomSettings::hidden, this, &Settings::show);

  QObject::connect(&advanced_, &AdvancedSettings::advancedSettingsChanged, this, &Settings::settingsChanged);
  QObject::connect(&advanced_, &AdvancedSettings::hidden, this, &Settings::show);
}

void Settings::show()
{
  initializeUIDeviceList(); // initialize everytime in case they have changed
  QWidget::show();
}

void Settings::on_save_clicked()
{
  qDebug() << "Settings," << metaObject()->className() << ": Saving basic settings";
  // The UI values are saved to settings.
  saveSettings();
  emit settingsChanged(); // TODO: check have the settings actually been changed
}

void Settings::on_close_clicked()
{
  qDebug() << "Settings," << metaObject()->className()
           << ": Cancel clicked. Getting settings from system";

  // discard UI values and restore the settings from file
  getSettings(false);
  hide();
}

void Settings::on_advanced_settings_button_clicked()
{
  //on_ok_clicked(); // treat this the same as ok
  saveSettings();
  hide();
  advanced_.show();
}


void Settings::on_custom_settings_button_clicked()
{
  //on_ok_clicked(); // treat this the same as ok
  saveSettings();
  hide();
  custom_.show();
}


void Settings::initializeUIDeviceList()
{
  qDebug() << "Settings," << metaObject()->className() << ": Initialize device list";
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
  qDebug() << "Settings," << metaObject()->className() << ": Saving basic Settings";

  // Local settings
  saveTextValue("local/Name", basicUI_->name_edit->text(), settings_);
  saveTextValue("local/Username", basicUI_->username_edit->text(), settings_);


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

    qDebug() << "Settings," << metaObject()->className()
             << "Recording following device:" << basicUI_->videoDevice->currentText();
  }
}

// restores recorded settings
void Settings::getSettings(bool changedDevice)
{
  initializeUIDeviceList();

  //get values from QSettings
  if(checkMissingValues() && checkUserSettings())
  {
    qDebug() << "Settings," << metaObject()->className() << ": Restoring user settings from file:" << settings_.fileName();
    basicUI_->name_edit->setText      (settings_.value("local/Name").toString());
    basicUI_->username_edit->setText  (settings_.value("local/Username").toString());

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
  qDebug() << "WARNING," << metaObject()->className()
           << ": Could not restore settings because they were corrupted!";
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

  qDebug() << "Settings," << metaObject()->className()
           << "Get device id: Index:" << deviceIndex << "deviceID:"
           << deviceID << "Name:" << settings_.value("video/Device").toString();

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
      qDebug() << "WARNING," << metaObject()->className() << ": MISSING SETTING FOR:" << key;
      foundEverything = false;
    }
  }
  return foundEverything;
}
