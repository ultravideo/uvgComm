#include "settings.h"

#include "ui_basicsettings.h"
#include "ui_advancedsettings.h"

#include "video/dshow/capture_interface.h"

#include <QDebug>

Settings::Settings(QWidget *parent) :
  QObject(parent),
  basicUI_(new Ui::BasicSettings),
  advancedUI_(new Ui::AdvancedSettings),
  settings_("kvazzup.ini", QSettings::IniFormat)
{
  basicUI_->setupUi(&basicParent_);
  advancedUI_->setupUi(&advancedParent_);

  dshow_initCapture();

  // initializes the GUI with values or initialize them in case they don't exist
  restoreBasicSettings();
  restoreAdvancedSettings();

  QObject::connect(basicUI_->ok, SIGNAL(clicked()), this, SLOT(on_ok_clicked()));
  QObject::connect(basicUI_->cancel, SIGNAL(clicked()), this, SLOT(on_cancel_clicked()));
  QObject::connect(advancedUI_->ok, SIGNAL(clicked()), this, SLOT(on_advanced_ok_clicked()));
  QObject::connect(advancedUI_->cancel, SIGNAL(clicked()), this, SLOT(on_advanced_cancel_clicked()));
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
  basicParent_.hide();
}

void Settings::on_cancel_clicked()
{
  qDebug() << "Getting basic settings from system";
  restoreBasicSettings();
  basicParent_.hide();
}

void Settings::on_advanced_ok_clicked()
{
  qDebug() << "Saving advanced settings";
  saveAdvancedSettings();
  emit settingsChanged();
  advancedParent_.hide();
}

void Settings::on_advanced_cancel_clicked()
{
  qDebug() << "Getting advanced settings from system";
  restoreAdvancedSettings();
  advancedParent_.hide();
}

// records the settings
void Settings::saveBasicSettings()
{
  qDebug() << "Saving basic Settings";

  // Local settings
  if(basicUI_->name_edit->text() != "")
    settings_.setValue("local/Name",         basicUI_->name_edit->text());
  if(basicUI_->username_edit->text() != "")
    settings_.setValue("local/Username",     basicUI_->username_edit->text());
  int currentIndex = basicUI_->videoDevice->currentIndex();
  if( currentIndex != -1)
  {
    settings_.setValue("video/DeviceID",      currentIndex);

    if(basicUI_->videoDevice->currentText() != settings_.value("video/Device") )
    {
      settings_.setValue("video/Device",        basicUI_->videoDevice->currentText());
      // set capability to first
      saveCameraCapabilities(currentIndex, 0);
      advancedUI_->resolution->setCurrentIndex(0);
    }

    qDebug() << "Recording following device:" << basicUI_->videoDevice->currentText();
  }
}

void Settings::saveAdvancedSettings()
{
  qDebug() << "Saving advanced Settings";

  // Video settings
  settings_.setValue("video/Preset",       advancedUI_->preset->currentText());

  if(advancedUI_->threads->text() != "")
    settings_.setValue("video/Threads",      advancedUI_->threads->text());

  if(advancedUI_->openhevc_threads->text() != "")
    settings_.setValue("video/OPENHEVC_threads",      advancedUI_->openhevc_threads->text());

  settings_.setValue("video/QP",             QString::number(advancedUI_->qp->value()));

  if(advancedUI_->wpp->isChecked())
    settings_.setValue("video/WPP",          "1");
  else
    settings_.setValue("video/WPP",          "0");

  if(advancedUI_->vps->text() != "")
    settings_.setValue("video/VPS",          advancedUI_->vps->text());
  if(advancedUI_->intra->text() != "")
    settings_.setValue("video/Intra",        advancedUI_->intra->text());

  if(advancedUI_->slices->isChecked())
    settings_.setValue("video/Slices",          "1");
  else
    settings_.setValue("video/Slices",          "0");

  int currentIndex = advancedUI_->resolution->currentIndex();
  if( currentIndex != -1)
  {
    qDebug() << "Saving resolution:" << advancedUI_->resolution->currentText();
  }
  else
  {
    qDebug() << "No current index set for resolution. Using 0";
    currentIndex = 0;
  }
  saveCameraCapabilities(settings_.value("video/DeviceID").toInt(), currentIndex);
  //settings.sync(); // TODO is this needed?
}

void Settings::saveCameraCapabilities(int deviceIndex, int capabilityIndex)
{
  qDebug() << "Recording capability settings for deviceIndex:" << deviceIndex;
  QSize resolution = QSize(0,0);
  double fps = 0.0f;
  QString format = "";
  getCapability(deviceIndex, capabilityIndex, resolution, fps, format);
  int32_t fps_int = static_cast<int>(fps);

  settings_.setValue("video/ResolutionID",         capabilityIndex);

  // since kvazaar requires resolution to be divisible by eight
  settings_.setValue("video/ResolutionWidth",      resolution.width() - resolution.width()%8);
  settings_.setValue("video/ResolutionHeight",     resolution.height() - resolution.height()%8);
  settings_.setValue("video/Framerate",            fps_int);
  settings_.setValue("video/InputFormat",          format);

  qDebug() << "Recorded the following video settings: Resolution:"
           << resolution.width() - resolution.width()%8 << "x" << resolution.height() - resolution.height()%8
           << "fps:" << fps_int << "resolution index:" << capabilityIndex << "format" << format;
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

void Settings::restoreAdvancedSettings()
{
  if(checkVideoSettings())
  {
    qDebug() << "Restoring previous Advanced settings from file:" << settings_.fileName();
    int index = advancedUI_->preset->findText(settings_.value("video/Preset").toString());
    if(index != -1)
      advancedUI_->preset->setCurrentIndex(index);
    advancedUI_->threads->setText        (settings_.value("video/Threads").toString());
    advancedUI_->qp->setValue            (settings_.value("video/QP").toInt());
    advancedUI_->openhevc_threads->setText        (settings_.value("video/OPENHEVC_threads").toString());

    if(settings_.value("video/WPP").toString() == "1")
      advancedUI_->wpp->setChecked(true);
    else if(settings_.value("video/WPP").toString() == "0")
      advancedUI_->wpp->setChecked(false);
    advancedUI_->vps->setText            (settings_.value("video/VPS").toString());
    advancedUI_->intra->setText          (settings_.value("video/Intra").toString());

    if(settings_.value("video/Slices").toString() == "1")
      advancedUI_->slices->setChecked(true);
    else if(settings_.value("video/Slices").toString() == "0")
      advancedUI_->slices->setChecked(false);

    int capabilityID = settings_.value("video/ResolutionID").toInt();
    if(advancedUI_->resolution->count() < capabilityID)
    {
      capabilityID = 0;
    }
    advancedUI_->resolution->setCurrentIndex(capabilityID);
  }
  else
  {
    resetFaultySettings();
  }
}

void Settings::resetFaultySettings()
{
  qDebug() << "Could not restore settings because they were corrupted";
  // record GUI settings in hope that they are correct ( is case by default )
  saveBasicSettings();
  saveAdvancedSettings();
}

void Settings::showBasicSettings()
{
  initializeDeviceList();
  restoreBasicSettings();
  basicParent_.show();
}

void Settings::showAdvancedSettings()
{
  advancedUI_->resolution->clear();
  QStringList capabilities = getVideoCapabilities(getVideoDeviceID());
  qDebug() << "Showing advanced settings";
  for(int i = 0; i < capabilities.size(); ++i)
  {
    advancedUI_->resolution->addItem( capabilities[i]);
  }
  restoreAdvancedSettings();
  advancedParent_.show();
}

void Settings::initializeDeviceList()
{
  qDebug() << "Initialize device list";
  basicUI_->videoDevice->clear();
  QStringList videoDevices = getVideoDevices();
  for(int i = 0; i < videoDevices.size(); ++i)
  {
    basicUI_->videoDevice->addItem( videoDevices[i]);
  }
}

QStringList Settings::getVideoDevices()
{
  char** devices;
  int8_t count = 0;
  dshow_queryDevices(&devices, &count);

  QStringList list;

  qDebug() << "Found " << (int)count << " devices: ";
  for(int i = 0; i < count; ++i)
  {
    qDebug() << "[" << i << "] " << devices[i];
    list.push_back(devices[i]);
  }
  return list;
}

QStringList Settings::getAudioDevices()
{
  //TODO
  return QStringList();
}

QStringList Settings::getVideoCapabilities(int deviceID)
{
  QStringList list;
  if (dshow_selectDevice(deviceID) || dshow_selectDevice(0))
  {
    int8_t count = 0;
    deviceCapability *capList;
    dshow_getDeviceCapabilities(&capList, &count);

    qDebug() << "Found" << (int)count << "capabilities";
    for(int i = 0; i < count; ++i)
    {
      list.push_back(QString(capList[i].format) + " " + QString::number(capList[i].width) + "x" +
                     QString::number(capList[i].height) + " " +
                     QString::number(capList[i].fps) + " fps");
    }
  }
  return list;
}

void Settings::getCapability(int deviceIndex,
                             int capabilityIndex,
                             QSize& resolution,
                             double& framerate,
                             QString& format)
{
  char **devices;
  int8_t count;
  dshow_queryDevices(&devices, &count); // this has to be done because of the api

  if (dshow_selectDevice(deviceIndex) || dshow_selectDevice(0))
  {
    int8_t count;
    deviceCapability *capList;
    dshow_getDeviceCapabilities(&capList, &count);

    if(count == 0)
    {
      qDebug() << "No capabilites found";
      return;
    }
    if(count < capabilityIndex)
    {
      capabilityIndex = 0;
      qDebug() << "Capability id not found";
    }

    resolution = QSize(capList[capabilityIndex].width,capList[capabilityIndex].height);
    framerate = capList[capabilityIndex].fps;
    format = capList[capabilityIndex].format;
  }
  else
  {
    qWarning() << "WARNING: Failed to select device for capacity information.";
  }
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

bool Settings::checkVideoSettings()
{
  return checkMissingValues()
      && settings_.contains("video/DeviceID")
      && settings_.contains("video/Device")
      && settings_.contains("video/ResolutionWidth")
      && settings_.contains("video/ResolutionHeight")
      && settings_.contains("video/WPP")
      && settings_.contains("video/Framerate")
      && settings_.contains("video/InputFormat")
      && settings_.contains("video/Slices");
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
