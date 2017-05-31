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
  restoreBasicSettings();
  restoreAdvancedSettings();

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
  saveBasicSettings();// because I am lazy record both
  saveAdvancedSettings();
  emit settingsChanged();
  basicParent_.hide();
  advancedParent_.hide();
}

void Settings::on_cancel_clicked()
{
  qDebug() << "Getting settings from system";
  restoreBasicSettings(); // because I am lazy restore both
  restoreAdvancedSettings();
  basicParent_.hide();
  advancedParent_.hide();
}

// records the settings
void Settings::saveBasicSettings()
{
  // Local settings
  QSettings settings;
  if(basicUI_->name_edit->text() != "")
    settings.setValue("local/Name",         basicUI_->name_edit->text());
  if(basicUI_->username_edit->text() != "")
    settings.setValue("local/Username",     basicUI_->username_edit->text());
  int currentIndex = basicUI_->videoDevice->currentIndex();
  if( currentIndex != -1)
  {
    settings.setValue("video/Device",        basicUI_->videoDevice->currentText());
    settings.setValue("video/DeviceID",      currentIndex);
  }
}

void Settings::saveAdvancedSettings()
{
  QSettings settings;

  // Video settings
  settings.setValue("video/Preset",       advancedUI_->preset->currentText());

  if(advancedUI_->scaled_height->text() != "")
    settings.setValue("video/ScaledHeight", advancedUI_->scaled_height->text());
  if(advancedUI_->scaled_width->text() != "")
    settings.setValue("video/ScaledWidth",  advancedUI_->scaled_width->text());
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

  //settings.sync(); // TODO is this needed?
}

// restores temporarily recorded settings
void Settings::restoreBasicSettings()
{
  //get values from QSettings
  if(checkSavedSettings())
  {
    QSettings settings;
    qDebug() << "Restoring previous Basic settings";
    basicUI_->name_edit->setText      (settings.value("local/Name").toString());
    basicUI_->username_edit->setText  (settings.value("local/Username").toString());
    if(basicUI_->videoDevice->currentIndex() != -1)
      settings.setValue("video/Device",        basicUI_->videoDevice->currentText());

    int deviceIndex = basicUI_->videoDevice->findText(settings.value("video/Device").toString());
    int deviceID = settings.value("video/Device").toInt();
    if(deviceIndex != -1)
    {
      // if we have multiple devices with same name we use id
      if(deviceID != deviceIndex
         && basicUI_->videoDevice->itemText(deviceID) == settings.value("video/Device").toString())
      {
        basicUI_->videoDevice->setCurrentIndex(deviceID);
      }
      else
      {
        basicUI_->videoDevice->setCurrentIndex(deviceIndex);
      }
    }
    else
      basicUI_->videoDevice->setCurrentIndex(0);
  }
  else
  {
    resetFaultySettings();
  }
}

void Settings::restoreAdvancedSettings()
{
  if(checkSavedSettings())
  {
    qDebug() << "Restoring previous Advanced settings";
    QSettings settings;
    int index = advancedUI_->preset->findText(settings.value("video/Preset").toString());
    if(index != -1)
      advancedUI_->preset->setCurrentIndex(index);
    advancedUI_->scaled_height->setText  (settings.value("video/ScaledHeight").toString());
    advancedUI_->scaled_width->setText   (settings.value("video/ScaledWidth").toString());
    advancedUI_->threads->setText        (settings.value("video/Threads").toString());
    advancedUI_->qp->setValue            (settings.value("video/QP").toInt());

    if(settings.value("video/WPP").toString() == "1")
      advancedUI_->wpp->setChecked(true);
    else if(settings.value("video/WPP").toString() == "0")
      advancedUI_->wpp->setChecked(false);
    advancedUI_->vps->setText            (settings.value("video/VPS").toString());
    advancedUI_->intra->setText          (settings.value("video/Intra").toString());
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
  basicParent_.show();
  for(unsigned int i = 0; i < videoDevices.size(); ++i)
  {
    basicUI_->videoDevice->addItem( videoDevices[i]);
  }
}

void Settings::showAdvancedSettings()
{
  advancedParent_.show();
}

QStringList Settings::getVideoDevices()
{
  char** devices;
  int8_t count;
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

QStringList Settings::getVideoCapabilities(QString device)
{}

int Settings::getSettingsDeviceID()
{

}

bool Settings::checkUIBasicSettings()
{
  return true;
}
bool Settings::checkUIAdvancedSettings()
{
  return true;
}

bool Settings::checkSavedSettings()
{
  QSettings settings;
  bool missingSettings = false;

  QStringList list = settings.allKeys();

  for(auto key : list)
  {
    if(!missingSettings && settings.value(key).isNull())
    {
      qDebug() << "MISSING SETTING FOR:" << key;
      missingSettings = true;
    }
  }

  if(list.size() < SETTINGCOUNT) // Remember to update this value
  {
    qDebug() << "Settings found:" << list.size() << "Expected:" << SETTINGCOUNT;
    missingSettings = true;
  }

  return !missingSettings;
}
