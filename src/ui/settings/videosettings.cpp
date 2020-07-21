#include "videosettings.h"

#include "ui_videosettings.h"

#include <ui/settings/camerainfo.h>
#include "settingshelper.h"

#include "common.h"

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
                                    "video/rgbThreads",
                                    "video/OPENHEVC_Threads",
                                    "video/FramerateID"};


VideoSettings::VideoSettings(QWidget* parent,
                               std::shared_ptr<CameraInfo> info)
  :
  QDialog(parent),
  currentDevice_(0),
  customUI_(new Ui::VideoSettings),
  cam_(info),
  settings_("kvazzup.ini", QSettings::IniFormat)
{
  customUI_->setupUi(this);

  // the buttons are named so that the slots are called automatically
  QObject::connect(customUI_->format_box, SIGNAL(activated(QString)),
                   this, SLOT(initializeResolutions(QString)));
  QObject::connect(customUI_->resolution, SIGNAL(activated(QString)),
                   this, SLOT(initializeFramerates(QString)));

  customUI_->custom_parameters->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(customUI_->custom_parameters, &QWidget::customContextMenuRequested,
          this, &VideoSettings::showParameterContextMenu);
}


void VideoSettings::init(int deviceID)
{
  currentDevice_ = deviceID;

  restoreCustomSettings();
}


void VideoSettings::showParameterContextMenu(const QPoint& pos)
{
  if (customUI_->custom_parameters->rowCount() != 0)
  {
    showContextMenu(pos, customUI_->custom_parameters, this,
                    QStringList() << "Delete", QStringList() << SLOT(deleteListParameter()));
  }
}


void VideoSettings::deleteListParameter()
{
  customUI_->custom_parameters->removeRow(customUI_->custom_parameters->currentRow());
}


void VideoSettings::changedDevice(uint16_t deviceIndex)
{
  currentDevice_ = deviceIndex;
  initializeFormat();
  saveCameraCapabilities(deviceIndex); // record the new camerasettings.
}


void VideoSettings::resetSettings(int deviceID)
{
  qDebug() << "Settings," << metaObject()->className()
           << "Resetting custom settings from UI";
  currentDevice_ = deviceID;
  initializeFormat();
  saveCustomSettings();
}


void VideoSettings::on_custom_ok_clicked()
{
  qDebug() << "Settings," << metaObject()->className() << ": Saving advanced settings";
  saveCustomSettings();
  emit customSettingsChanged();
  //emit hidden();
  //hide();
}


void VideoSettings::on_custom_close_clicked()
{
  qDebug() << "Settings," << metaObject()->className()
           << ": Cancelled modifying custom settings. Getting settings from system";
  restoreCustomSettings();
  hide();
  emit hidden();
}

void VideoSettings::on_add_parameter_clicked()
{
  qDebug() << "Settings," << metaObject()->className()
           << ": Adding a custom parameter for kvazaar.";

  if (customUI_->parameter_name->text() == "")
  {
    qDebug() << "Settings," << metaObject()->className() << ": parameter name not set";
    return;
  }

  QStringList list = QStringList() << customUI_->parameter_name->text()
                                   << customUI_->parameter_value->text();
  addFieldsToTable(list, customUI_->custom_parameters);
}

void VideoSettings::saveCustomSettings()
{
  qDebug() << "Settings," << metaObject()->className() << ": Saving advanced Settings";

  // Video settings
  saveTextValue("video/kvzThreads",            customUI_->kvz_threads->text(), settings_);
  saveTextValue("video/OPENHEVC_threads",      customUI_->openhevc_threads->text(), settings_);
  saveTextValue("video/yuvThreads",            customUI_->yuv_threads->text(), settings_);
  saveTextValue("video/rgbThreads",            customUI_->rgb32_threads->text(), settings_);
  saveTextValue("video/VPS",                   customUI_->vps->text(), settings_);
  saveTextValue("video/Intra",                 customUI_->intra->text(), settings_);
  saveCheckBox("video/WPP",                    customUI_->wpp, settings_);
  saveCheckBox("video/Slices",                 customUI_->slices, settings_);

  settings_.setValue("video/QP",               QString::number(customUI_->qp->value()));
  settings_.setValue("video/Preset",           customUI_->preset->currentText());

  saveCheckBox("video/opengl",                 customUI_->opengl, settings_);
  saveCheckBox("video/flipViews",              customUI_->flip, settings_);

  //settings_.setValue("audio/Channels",         QString::number(customUI_->channels->value()));

  listGUIToSettings("kvazzup.ini", "parameters", QStringList() << "Name" << "Value", customUI_->custom_parameters);

  saveCameraCapabilities(settings_.value("video/DeviceID").toInt());
}


void VideoSettings::saveCameraCapabilities(int deviceIndex)
{
  qDebug() << "Settings," << metaObject()->className()
           << ": Recording capability settings for deviceIndex:" << deviceIndex;

  int formatIndex = customUI_->format_box->currentIndex();
  int resolutionIndex = customUI_->resolution->currentIndex();

  qDebug() << "Settings," << metaObject()->className()
           << ": Boxes in following positions: Format:" << formatIndex << "Resolution:" << resolutionIndex;
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

  // TODO: does not work if minimum and maximum framerates differ or if framerate is fractional
  if (!customUI_->framerate_box->currentText().isEmpty())
  {
    settings_.setValue("video/Framerate",            customUI_->framerate_box->currentText());
  }
  else {
    settings_.setValue("video/Framerate",            0);
  }

  settings_.setValue("video/InputFormat",          format);

  qDebug() << "Settings," << metaObject()->className() << ": Recorded the following video settings: Resolution:"
           << res.width() - res.width()%8 << "x" << res.height() - res.height()%8
           << "resolution index:" << resolutionIndex << "format" << format;
}


void VideoSettings::restoreCustomSettings()
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

    customUI_->yuv_threads->setValue(settings_.value("video/yuvThreads").toInt());
    customUI_->rgb32_threads->setValue(settings_.value("video/rgbThreads").toInt());

    restoreCheckBox("video/WPP", customUI_->wpp, settings_);

    customUI_->vps->setText            (settings_.value("video/VPS").toString());
    customUI_->intra->setText          (settings_.value("video/Intra").toString());

    restoreCheckBox("video/Slices", customUI_->slices, settings_);

    customUI_->format_box->setCurrentText(settings_.value("video/InputFormat").toString());

    int resolutionID = settings_.value("video/ResolutionID").toInt();
    customUI_->resolution->setCurrentIndex(resolutionID);

    restoreCheckBox("video/opengl", customUI_->opengl, settings_);
    restoreCheckBox("video/flipViews", customUI_->flip, settings_);

    listSettingsToGUI("kvazzup.ini", "parameters", QStringList() << "Name" << "Value",
                      customUI_->custom_parameters);
  }
  else
  {
    resetSettings(currentDevice_);
  }

  if(validSettings && checkAudioSettings())
  {
    // TODO: implement audio settings.
    //customUI_->channels->setValue(settings_.value("audio/Channels").toInt());
  }
  else
  {
    // TODO: reset only audio settings.
    resetSettings(currentDevice_);
  }
}

void VideoSettings::restoreFormat()
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

void VideoSettings::restoreResolution()
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


void VideoSettings::restoreFramerate()
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


void VideoSettings::initializeFormat()
{
  printNormal(this, "Initializing formats");
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


void VideoSettings::initializeResolutions(QString format)
{
  qDebug() << "Settings," << metaObject()->className()
           << ": Initializing resolutions for format:" << format;
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
    initializeFramerates(customUI_->resolution->currentText());
  }
}


void VideoSettings::initializeFramerates(QString resolution)
{
  qDebug() << "Settings,"  << metaObject()->className() << ": Initializing framerates for resolution:"
           << resolution;
  customUI_->framerate_box->clear();
  QStringList rates;

  cam_->getFramerates(currentDevice_, customUI_->format_box->currentText(),
                      customUI_->resolution->currentIndex(), rates);

  if(!rates.empty())
  {
    for(int i = 0; i < rates.size(); ++i)
    {
      customUI_->framerate_box->addItem(rates.at(i));
    }
    // use the highest framerate values as default selection.
    customUI_->framerate_box->setCurrentIndex(customUI_->framerate_box->count() - 1);
  }
}


bool VideoSettings::checkVideoSettings()
{
  bool everythingPresent = checkMissingValues(settings_);

  for(auto& need : neededSettings)
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

bool VideoSettings::checkAudioSettings()
{
  return true;
}
