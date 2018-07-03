#include "settings.h"

#include "ui_settings.h"
#include "ui_advancedsettings.h"


#include <QDebug>

Settings::Settings(QWidget *parent) :
  QDialog(parent),
  basicUI_(new Ui::BasicSettings),
  advancedUI_(new Ui::AdvancedSettings),
  settings_("kvazzup.ini", QSettings::IniFormat),
  cam_()
{
  basicUI_->setupUi(this);
  advancedUI_->setupUi(&advancedParent_);



  // initializes the GUI with values or initialize them in case they don't exist
  restoreBasicSettings();
  restoreAdvancedSettings();

  QObject::connect(basicUI_->ok, SIGNAL(clicked()), this, SLOT(on_ok_clicked()));
  QObject::connect(basicUI_->cancel, SIGNAL(clicked()), this, SLOT(on_cancel_clicked()));
  QObject::connect(advancedUI_->ok, SIGNAL(clicked()), this, SLOT(on_advanced_ok_clicked()));
  QObject::connect(advancedUI_->cancel, SIGNAL(clicked()), this, SLOT(on_advanced_cancel_clicked()));
  QObject::connect(basicUI_->advanced_settings_button, SIGNAL(clicked()),
                   this, SLOT(on_advanced_settings_clicked()));
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
  showAdvancedSettings();
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
      saveCameraCapabilities(currentIndex, 0);
      advancedUI_->format_box->setCurrentIndex(0);
      advancedUI_->resolution->setCurrentIndex(0);
    }

    qDebug() << "Recording following device:" << basicUI_->videoDevice->currentText();
  }
}

void Settings::saveAdvancedSettings()
{
  qDebug() << "Saving advanced Settings";

  // Video settings
  saveTextValue("video/Threads",               advancedUI_->threads->text());
  saveTextValue("video/OPENHEVC_threads",      advancedUI_->openhevc_threads->text());
  saveTextValue("video/VPS",                   advancedUI_->vps->text());
  saveTextValue("video/Intra",                 advancedUI_->intra->text());
  saveCheckBox("video/WPP",                    advancedUI_->wpp);
  saveCheckBox("video/Slices",                 advancedUI_->slices);

  settings_.setValue("video/QP",               QString::number(advancedUI_->qp->value()));
  settings_.setValue("video/Preset",           advancedUI_->preset->currentText());

  QStringList formats;
  QList<QStringList> resolutions;
  cam_.getVideoCapabilities(getVideoDeviceID(), formats, resolutions);

  int resolutionIndex = advancedUI_->resolution->currentIndex();
  if(resolutionIndex == -1)
  {
    qDebug() << "No current index set for resolution. Using first";
    resolutionIndex = 0;
  }

  int formatIndex = advancedUI_->resolution->currentIndex();
  if(formatIndex == -1)
  {
    qDebug() << "No current index set for format. Using first";
    formatIndex = 0;
  }

  int currentIndex = 0;

  // add all other resolutions to form currentindex
  for(unsigned int i = 0; i <= formatIndex; ++i)
  {
    currentIndex += resolutions.at(i).size();
  }

  currentIndex += resolutionIndex;
  qDebug() << "Saving format:" << advancedUI_->format_box->currentText()
           << " and resolution:" << advancedUI_->resolution->currentText();

  saveCameraCapabilities(settings_.value("video/DeviceID").toInt(), currentIndex);
  //settings.sync(); // TODO is this needed?
}

void Settings::saveCameraCapabilities(int deviceIndex, int capabilityIndex)
{
  qDebug() << "Recording capability settings for deviceIndex:" << deviceIndex;
  QSize resolution = QSize(0,0);
  double fps = 0.0f;
  QString format = "";
  cam_.getCapability(deviceIndex, capabilityIndex, resolution, fps, format);
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

    restoreCheckBox("video/WPP", advancedUI_->wpp);

    advancedUI_->vps->setText            (settings_.value("video/VPS").toString());
    advancedUI_->intra->setText          (settings_.value("video/Intra").toString());

    restoreCheckBox("video/Slices", advancedUI_->slices);

    int resolutionID = settings_.value("video/ResolutionID").toInt();
    if(advancedUI_->resolution->count() < resolutionID)
    {
      resolutionID = 0;
    }
    advancedUI_->resolution->setCurrentIndex(resolutionID);

    int formatIndex = settings_.value("video/ResolutionID").toInt();
    if(advancedUI_->resolution->count() < formatIndex)
    {
      formatIndex = 0;
    }
    advancedUI_->resolution->setCurrentIndex(formatIndex);
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
  show();
}

void Settings::showAdvancedSettings()
{
  advancedUI_->resolution->clear();

  QStringList formats;
  QList<QStringList> resolutions;
  cam_.getVideoCapabilities(getVideoDeviceID(), formats, resolutions);

  for(int i = 0; i < formats.size(); ++i)
  {
    advancedUI_->format_box->addItem( formats.at(i));
  }

  qDebug() << "Showing advanced settings";
  for(int i = 0; i < resolutions.size(); ++i)
  {
    advancedUI_->resolution->addItem( resolutions[0].at(i));
  }
  restoreAdvancedSettings();
  advancedParent_.show();
}

void Settings::initializeDeviceList()
{
  qDebug() << "Initialize device list";
  basicUI_->videoDevice->clear();
  QStringList videoDevices = cam_.getVideoDevices();
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


void Settings::saveTextValue(const QString settingValue, const QString &text)
{
  if(text != "")
  {
    settings_.setValue(settingValue,  text);
  }
}
