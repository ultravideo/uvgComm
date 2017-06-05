#include "settings.h"
#include "ui_basicsettings.h"
#include "ui_advancedsettings.h"

#include "dshow/capture_interface.h"

#include <QDebug>


const int SETTINGCOUNT = 10;

Settings::Settings(QWidget *parent) :
  QObject(parent),
  basicUI_(new Ui::BasicSettings),
  advancedUI_(new Ui::AdvancedSettings)
{
  basicUI_->setupUi(&basicParent_);
  advancedUI_->setupUi(&advancedParent_);

  // initializes the GUI with values
  //restoreBasicSettings();
  //restoreAdvancedSettings();

  QObject::connect(basicUI_->ok, SIGNAL(clicked()), this, SLOT(on_ok_clicked()));
  QObject::connect(basicUI_->cancel, SIGNAL(clicked()), this, SLOT(on_cancel_clicked()));
  QObject::connect(advancedUI_->ok, SIGNAL(clicked()), this, SLOT(on_ok_clicked()));
  QObject::connect(advancedUI_->cancel, SIGNAL(clicked()), this, SLOT(on_cancel_clicked()));
}

Settings::~Settings()
{
  // I think the UI:s are destroyed when parents are destroyed
  //delete basicUI_;
  //delete advancedUI_;
}

void Settings::on_ok_clicked()
{
  qDebug() << "Saving settings";
  saveBasicSettings();// because I am lazy, record both
  saveAdvancedSettings();
  emit settingsChanged();
  basicParent_.hide();
  advancedParent_.hide();
}

void Settings::on_cancel_clicked()
{
  qDebug() << "Getting settings from system";
  restoreBasicSettings(); // because I am lazy, restore both
  restoreAdvancedSettings();
  basicParent_.hide();
  advancedParent_.hide();
}

// records the settings
void Settings::saveBasicSettings()
{
  qDebug() << "Saving basic Settings";

  // Local settings
  QSettings settings;
  if(basicUI_->name_edit->text() != "")
    settings.setValue("local/Name",         basicUI_->name_edit->text());
  if(basicUI_->username_edit->text() != "")
    settings.setValue("local/Username",     basicUI_->username_edit->text());
  int currentIndex = basicUI_->videoDevice->currentIndex();
  if( currentIndex != -1)
  {
    settings.setValue("video/DeviceID",      currentIndex);

    if(basicUI_->videoDevice->currentText() != settings.value("video/Device") )
    {
      settings.setValue("video/Device",        basicUI_->videoDevice->currentText());
      // set capability to first
      saveCameraCapabilities(settings, currentIndex, 0);
      advancedUI_->resolution->setCurrentIndex(0);
    }

    qDebug() << "Recording following device:" << basicUI_->videoDevice->currentText();
  }
}

void Settings::saveAdvancedSettings()
{
  qDebug() << "Saving advanced Settings";
  QSettings settings;

  // Video settings
  settings.setValue("video/Preset",       advancedUI_->preset->currentText());

  if(advancedUI_->threads->text() != "")
    settings.setValue("video/Threads",      advancedUI_->threads->text());

  settings.setValue("video/QP",             QString::number(advancedUI_->qp->value()));

  if(advancedUI_->wpp->isChecked())
    settings.setValue("video/WPP",          "1");
  else
    settings.setValue("video/WPP",          "0");

  if(advancedUI_->vps->text() != "")
    settings.setValue("video/VPS",          advancedUI_->vps->text());
  if(advancedUI_->intra->text() != "")
    settings.setValue("video/Intra",        advancedUI_->intra->text());

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

  saveCameraCapabilities(settings, settings.value("video/DeviceID").toInt(), currentIndex);
  //settings.sync(); // TODO is this needed?
}

void Settings::saveCameraCapabilities(QSettings& settings, int deviceIndex, int capabilityIndex)
{
  QSize resolution = QSize(0,0);
  double fps = 0.0f;
  getCapability(deviceIndex, capabilityIndex, resolution, fps);
  int32_t fps_int = static_cast<int>(fps);

  settings.setValue("video/ResolutionID",         capabilityIndex);
  settings.setValue("video/ResolutionWidth",      resolution.width());
  settings.setValue("video/ResolutionHeight",     resolution.height());
  settings.setValue("video/Framerate",            fps_int);

  qDebug() << "Recorded the following video settings: Resolution:" << resolution
           << "fps:" << fps_int << "resolution index:" << capabilityIndex;
}

// restores temporarily recorded settings
void Settings::restoreBasicSettings()
{
  //get values from QSettings
  if(checkMissingValues())
  {
    QSettings settings;
    qDebug() << "Restoring previous Basic settings";
    basicUI_->name_edit->setText      (settings.value("local/Name").toString());
    basicUI_->username_edit->setText  (settings.value("local/Username").toString());

    basicUI_->videoDevice->setCurrentIndex(getVideoDeviceID(settings));
  }
  else
  {
    resetFaultySettings();
  }
}

void Settings::restoreAdvancedSettings()
{
  if(checkUserSettings())
  {
    qDebug() << "Restoring previous Advanced settings";
    QSettings settings;
    int index = advancedUI_->preset->findText(settings.value("video/Preset").toString());
    if(index != -1)
      advancedUI_->preset->setCurrentIndex(index);
    advancedUI_->threads->setText        (settings.value("video/Threads").toString());
    advancedUI_->qp->setValue            (settings.value("video/QP").toInt());

    if(settings.value("video/WPP").toString() == "1")
      advancedUI_->wpp->setChecked(true);
    else if(settings.value("video/WPP").toString() == "0")
      advancedUI_->wpp->setChecked(false);
    advancedUI_->vps->setText            (settings.value("video/VPS").toString());
    advancedUI_->intra->setText          (settings.value("video/Intra").toString());

    int capabilityID = settings.value("video/ResolutionID").toInt();
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
  qDebug() << "Could not restore advanced settings because they were corrupted";
  // record GUI settings in hope that they are correct ( is case by default )
  saveBasicSettings();
  saveAdvancedSettings();
}


void Settings::showBasicSettings()
{
  basicUI_->videoDevice->clear();
  QStringList videoDevices = getVideoDevices();
  for(unsigned int i = 0; i < videoDevices.size(); ++i)
  {
    basicUI_->videoDevice->addItem( videoDevices[i]);
  }
  restoreBasicSettings();
  basicParent_.show();
}

void Settings::showAdvancedSettings()
{
  advancedUI_->resolution->clear();
  QSettings settings;
  QStringList capabilities = getVideoCapabilities(getVideoDeviceID(settings));
  for(unsigned int i = 0; i < capabilities.size(); ++i)
  {
    advancedUI_->resolution->addItem( capabilities[i]);
  }
  restoreAdvancedSettings();
  advancedParent_.show();
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
{}

QStringList Settings::getVideoCapabilities(int deviceID)
{
  QStringList list;
  if (dshow_selectDevice(deviceID) || dshow_selectDevice(0))
  {
    int8_t count = 0;
    deviceCapability *capList;
    dshow_getDeviceCapabilities(&capList, &count);

    qDebug() << "Found " << (int)count << " capabilities: ";
    for(int i = 0; i < count; ++i)
    {
      qDebug() << "[" << i << "] " << capList[i].width << "x" << capList[i].height;
      list.push_back(QString(capList[i].format) + " " + QString::number(capList[i].width) + "x" +
                     QString::number(capList[i].height) + " " +
                     QString::number(capList[i].fps) + " fps");
    }
  }

  return list;
}

void Settings::getCapability(int deviceIndex, int capabilityIndex, QSize& resolution, double& framerate)
{
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
  }
}


int Settings::getVideoDeviceID(QSettings &settings)
{
  int deviceIndex = basicUI_->videoDevice->findText(settings.value("video/Device").toString());
  int deviceID = settings.value("video/DeviceID").toInt();

  qDebug() << "deviceIndex:" << deviceIndex << "deviceID:" << deviceID;
  qDebug() << "deviceName:" << settings.value("video/Device").toString();
  if(deviceIndex != -1 && basicUI_->videoDevice->count() != 0)
  {
    // if we have multiple devices with same name we use id
    if(deviceID != deviceIndex
       && basicUI_->videoDevice->itemText(deviceID) == settings.value("video/Device").toString())
    {
      return deviceID;
    }
    else
    {
      return deviceIndex;
    }
  }
  else if(basicUI_->videoDevice->count() != 0)
  {
    return 0;
  }
  return -1;
}

bool Settings::checkUserSettings()
{
  QSettings settings;
  return settings.contains("local/Name")
      && settings.contains("local/Username");
}
bool Settings::checkVideoSettings()
{
  QSettings settings;
  return checkMissingValues()
      && settings.contains("video/DeviceID")
      && settings.contains("video/Device")
      && settings.contains("video/ResolutionWidth")
      && settings.contains("video/ResolutionHeight")
      && settings.contains("video/WPP")
      && settings.contains("video/Framerate");
}

bool Settings::checkMissingValues()
{
  QSettings settings;
  QStringList list = settings.allKeys();
  for(auto key : list)
  {
    if(settings.value(key).isNull())
    {
      qDebug() << "MISSING SETTING FOR:" << key;
      return false;
    }
  }
  return true;
}
