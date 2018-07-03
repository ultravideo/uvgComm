#include "customsettings.h"

#include "ui_advancedsettings.h"

#include <video/camerainfo.h>

#include <QDebug>

CustomSettings::CustomSettings(std::shared_ptr<CameraInfo> info)
  :
  currentDevice_(0),
  advancedUI_(new Ui::AdvancedSettings),
  cam_(info),
  settings_("kvazzup.ini", QSettings::IniFormat)
{
  advancedUI_->setupUi(this);

  restoreAdvancedSettings();

  QObject::connect(advancedUI_->ok, SIGNAL(clicked()), this, SLOT(on_advanced_ok_clicked()));
  QObject::connect(advancedUI_->cancel, SIGNAL(clicked()), this, SLOT(on_advanced_cancel_clicked()));
}

void CustomSettings::changedDevice(uint16_t deviceIndex)
{
  saveCameraCapabilities(deviceIndex, 0);
  advancedUI_->format_box->setCurrentIndex(0);
  advancedUI_->resolution->setCurrentIndex(0);
  currentDevice_ = deviceIndex;
}

void CustomSettings::resetSettings()
{
  saveAdvancedSettings();
}

void CustomSettings::on_advanced_ok_clicked()
{
  qDebug() << "Saving advanced settings";
  saveAdvancedSettings();
  emit customSettingsChanged();
  hide();
}

void CustomSettings::on_advanced_cancel_clicked()
{
  qDebug() << "Getting advanced settings from system";
  restoreAdvancedSettings();
  hide();
}

void CustomSettings::saveAdvancedSettings()
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
  cam_->getVideoCapabilities(currentDevice_, formats, resolutions);

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

void CustomSettings::saveCameraCapabilities(int deviceIndex, int capabilityIndex)
{
  qDebug() << "Recording capability settings for deviceIndex:" << deviceIndex;
  QSize resolution = QSize(0,0);
  double fps = 0.0f;
  QString format = "";
  cam_->getCapability(deviceIndex, capabilityIndex, resolution, fps, format);
  int32_t fps_int = static_cast<int>(fps);

  settings_.setValue("video/ResolutionID",         capabilityIndex);

  // since kvazaar requires resolution to be divisible by eight
  // TODO: Use QSize to record resolution
  settings_.setValue("video/ResolutionWidth",      resolution.width() - resolution.width()%8);
  settings_.setValue("video/ResolutionHeight",     resolution.height() - resolution.height()%8);
  settings_.setValue("video/Framerate",            fps_int);
  settings_.setValue("video/InputFormat",          format);

  qDebug() << "Recorded the following video settings: Resolution:"
           << resolution.width() - resolution.width()%8 << "x" << resolution.height() - resolution.height()%8
           << "fps:" << fps_int << "resolution index:" << capabilityIndex << "format" << format;
}

void CustomSettings::restoreAdvancedSettings()
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
    resetSettings();
  }
}

void CustomSettings::showAdvancedSettings()
{
  advancedUI_->resolution->clear();

  QStringList formats;
  QList<QStringList> resolutions;
  cam_->getVideoCapabilities(currentDevice_, formats, resolutions);

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
  show();
}

void CustomSettings::restoreCheckBox(const QString settingValue, QCheckBox* box)
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

void CustomSettings::saveCheckBox(const QString settingValue, QCheckBox* box)
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

void CustomSettings::saveTextValue(const QString settingValue, const QString &text)
{
  if(text != "")
  {
    settings_.setValue(settingValue,  text);
  }
}

bool CustomSettings::checkMissingValues()
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

bool CustomSettings::checkVideoSettings()
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

