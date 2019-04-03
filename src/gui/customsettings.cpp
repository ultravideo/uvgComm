#include "customsettings.h"

#include "ui_customsettings.h"

#include <video/camerainfo.h>
#include "settingshelper.h"

#include <QTableWidgetItem>


#include <QDebug>


const QStringList neededSettings = {"video/DeviceID",
                                    "video/Device",
                                    "video/ResolutionWidth",
                                    "video/ResolutionHeight",
                                    "video/WPP",
                                    "video/InputFormat",
                                    "video/Slices",
                                    "video/kvzThreads",
                                    "video/yuvThreads",
                                    "video/OPENHEVC_Threads",
                                    "video/FramerateID"};


CustomSettings::CustomSettings(QWidget* parent,
                               std::shared_ptr<CameraInfo> info)
  :
  QDialog(parent),
  currentDevice_(0),
  customUI_(new Ui::CustomSettings),
  cam_(info),
  settings_("kvazzup.ini", QSettings::IniFormat)
{
  customUI_->setupUi(this);

  // the buttons are named so that the slots are called automatically
  QObject::connect(customUI_->format_box, SIGNAL(activated(QString)), this, SLOT(initializeResolutions(QString)));
  QObject::connect(customUI_->resolution, SIGNAL(activated(QString)), this, SLOT(initializeFramerates(QString)));
}


void CustomSettings::init(int deviceID)
{
  currentDevice_ = deviceID;

  restoreCustomSettings();
}


void CustomSettings::changedDevice(uint16_t deviceIndex)
{
  currentDevice_ = deviceIndex;
  initializeFormat();
  saveCameraCapabilities(deviceIndex); // record the new camerasettings.
}


void CustomSettings::resetSettings(int deviceID)
{
  qDebug() << "Settings," << metaObject()->className()
           << "Resetting custom settings from UI";
  currentDevice_ = deviceID;
  initializeFormat();
  saveCustomSettings();
}


void CustomSettings::on_custom_ok_clicked()
{
  qDebug() << "Settings," << metaObject()->className() << ": Saving advanced settings";
  saveCustomSettings();
  emit customSettingsChanged();
  emit hidden();
  hide();
}


void CustomSettings::on_custom_cancel_clicked()
{
  qDebug() << "Settings," << metaObject()->className() << ": Cancelled modifying custom settings. Getting settings from system";
  restoreCustomSettings();
  hide();
  emit hidden();
}


void CustomSettings::saveCustomSettings()
{
  qDebug() << "Settings," << metaObject()->className() << ": Saving advanced Settings";

  // Video settings
  saveTextValue("video/kvzThreads",            customUI_->kvz_threads->text(), settings_);
  saveTextValue("video/OPENHEVC_threads",      customUI_->openhevc_threads->text(), settings_);
  saveTextValue("video/yuvThreads",            customUI_->yuv_threads->text(), settings_);
  saveTextValue("video/VPS",                   customUI_->vps->text(), settings_);
  saveTextValue("video/Intra",                 customUI_->intra->text(), settings_);
  saveCheckBox("video/WPP",                    customUI_->wpp, settings_);
  saveCheckBox("video/Slices",                 customUI_->slices, settings_);

  settings_.setValue("video/QP",               QString::number(customUI_->qp->value()));
  settings_.setValue("video/Preset",           customUI_->preset->currentText());

  saveCheckBox("video/opengl",                 customUI_->opengl, settings_);
  saveCheckBox("video/flipViews",              customUI_->flip, settings_);
  saveCheckBox("video/liveCopying",            customUI_->live555Copy, settings_);

  settings_.setValue("audio/Channels",         QString::number(customUI_->channels->value()));

  saveCameraCapabilities(settings_.value("video/DeviceID").toInt());
}


void CustomSettings::saveCameraCapabilities(int deviceIndex)
{
  qDebug() << "Settings," << metaObject()->className() << ": Recording capability settings for deviceIndex:" << deviceIndex;

  int formatIndex = customUI_->format_box->currentIndex();
  int resolutionIndex = customUI_->resolution->currentIndex();

  qDebug() << "Settings," << metaObject()->className() << ": Boxes in following positions: Format:" << formatIndex << "Resolution:" << resolutionIndex;
  if(formatIndex == -1)
  {
    formatIndex = 0;
    resolutionIndex = 0;
  }

  if(resolutionIndex == -1)
  {
    resolutionIndex = 0;
  }

  QString format = cam_->getFormat(currentDevice_, formatIndex);
  QSize res = cam_->getResolution(currentDevice_, formatIndex, resolutionIndex);

  // since kvazaar requires resolution to be divisible by eight
  // TODO: Use QSize to record resolution
  settings_.setValue("video/ResolutionWidth",      res.width() - res.width()%8);
  settings_.setValue("video/ResolutionHeight",     res.height() - res.height()%8);
  settings_.setValue("video/ResolutionID",         resolutionIndex);
  settings_.setValue("video/FramerateID",          customUI_->framerate_box->currentIndex());
  settings_.setValue("video/InputFormat",          format);

  qDebug() << "Settings," << metaObject()->className() << ": Recorded the following video settings: Resolution:"
           << res.width() - res.width()%8 << "x" << res.height() - res.height()%8
           << "resolution index:" << resolutionIndex << "format" << format;
}


void CustomSettings::restoreCustomSettings()
{
  initializeFormat();

  bool validSettings = checkMissingValues(settings_);
  if(validSettings && checkVideoSettings())
  {
    restoreFormat();
    restoreResolution();
    restoreFramerate();

    qDebug() << "Settings," << metaObject()->className() << ": Restoring previous Advanced settings from file:" << settings_.fileName();
    int index = customUI_->preset->findText(settings_.value("video/Preset").toString());
    if(index != -1)
      customUI_->preset->setCurrentIndex(index);
    customUI_->kvz_threads->setText        (settings_.value("video/kvzThreads").toString());
    customUI_->qp->setValue            (settings_.value("video/QP").toInt());
    customUI_->openhevc_threads->setText        (settings_.value("video/OPENHEVC_threads").toString());

    customUI_->yuv_threads->setText        (settings_.value("video/yuvThreads").toString());

    restoreCheckBox("video/WPP", customUI_->wpp, settings_);

    customUI_->vps->setText            (settings_.value("video/VPS").toString());
    customUI_->intra->setText          (settings_.value("video/Intra").toString());

    restoreCheckBox("video/Slices", customUI_->slices, settings_);

    customUI_->format_box->setCurrentText(settings_.value("video/InputFormat").toString());

    int resolutionID = settings_.value("video/ResolutionID").toInt();
    customUI_->resolution->setCurrentIndex(resolutionID);

    restoreCheckBox("video/opengl", customUI_->opengl, settings_);
    restoreCheckBox("video/flipViews", customUI_->flip, settings_);
    restoreCheckBox("video/liveCopying", customUI_->live555Copy, settings_);

  }
  else
  {
    resetSettings(currentDevice_);
  }

  if(validSettings && checkAudioSettings())
  {
    // TODO: implement audio settings.
    customUI_->channels->setValue(settings_.value("audio/Channels").toInt());
  }
  else
  {
    // TODO: reset only audio settings.
    resetSettings(currentDevice_);
  }
}

void CustomSettings::restoreFormat()
{
  if(customUI_->format_box->count() > 0)
  {
    // initialize right format
    QString format = "";
    if(settings_.contains("video/InputFormat"))
    {
      format = settings_.value("video/InputFormat").toString();
      int formatIndex = customUI_->format_box->findText(format);
      qDebug() << "Settings," << metaObject()->className() << ": Trying to find format in camera:" << format << "Result index:" << formatIndex;

      if(formatIndex != -1)
      {
        customUI_->format_box->setCurrentIndex(formatIndex);
      }
      else {
        customUI_->format_box->setCurrentIndex(0);
      }

    }
    else
    {
      customUI_->format_box->setCurrentIndex(0);
    }

    initializeResolutions(customUI_->format_box->currentText());
  }
}

void CustomSettings::restoreResolution()
{
  if(customUI_->resolution->count() > 0)
  {
    int resolutionID = settings_.value("video/ResolutionID").toInt();

    if(resolutionID >= 0)
    {
      customUI_->resolution->setCurrentIndex(resolutionID);
    }
    else {
      customUI_->resolution->setCurrentIndex(0);
    }
  }
}


void CustomSettings::restoreFramerate()
{
  if(customUI_->framerate_box->count() > 0)
  {
    int framerateID = settings_.value("video/FramerateID").toInt();

    if(framerateID < customUI_->framerate_box->count())
    {
      customUI_->framerate_box->setCurrentIndex(framerateID);
    }
    else {
      customUI_->framerate_box->setCurrentIndex(customUI_->framerate_box->count() - 1);
    }
  }
}


void CustomSettings::initializeFormat()
{
  qDebug() << "Settings," << metaObject()->className() << "Initializing formats";
  QStringList formats;

  cam_->getVideoFormats(currentDevice_, formats);

  customUI_->format_box->clear();
  for(int i = 0; i < formats.size(); ++i)
  {
    customUI_->format_box->addItem(formats.at(i));
  }

  if(customUI_->format_box->count() > 0)
  {
    customUI_->format_box->setCurrentIndex(0);
    initializeResolutions(customUI_->format_box->currentText());
  }
}


void CustomSettings::initializeResolutions(QString format)
{
  qDebug() << "Settings," << metaObject()->className() << ": Initializing resolutions for format:" << format;
  customUI_->resolution->clear();
  QStringList resolutions;

  cam_->getFormatResolutions(currentDevice_, format, resolutions);

  if(!resolutions.empty())
  {
    for(int i = 0; i < resolutions.size(); ++i)
    {
      customUI_->resolution->addItem(resolutions.at(i));
    }
  }

  if(customUI_->resolution->count() > 0)
  {
    customUI_->resolution->setCurrentIndex(0);
    initializeFramerates(format, 0);
  }
}

void CustomSettings::initializeFramerates(QString format, int resolutionID)
{
  qDebug() << "Settings,"  << metaObject()->className() << ": Initializing resolutions for format:" << format;
  customUI_->framerate_box->clear();
  QStringList rates;

  cam_->getFramerates(currentDevice_, format, resolutionID, rates);

  if(!rates.empty())
  {
    for(int i = 0; i < rates.size(); ++i)
    {
      customUI_->framerate_box->addItem(rates.at(i));
    }
  }
}


bool CustomSettings::checkVideoSettings()
{
  bool everythingPresent = checkMissingValues(settings_);

  for(auto need : neededSettings)
  {
    if(!settings_.contains(need))
    {
      qDebug() << "Settings," << metaObject()->className()
               << "Missing setting for:" << need << "Resetting custom settings";
      everythingPresent = false;
    }
  }
  return everythingPresent;
}

bool CustomSettings::checkAudioSettings()
{
  return true;
}
