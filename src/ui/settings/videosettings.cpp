#include "videosettings.h"

#include "ui_videosettings.h"

#include <ui/settings/camerainfo.h>
#include "settingshelper.h"

#include "common.h"

#include <QTableWidgetItem>
#include <QThread>
#include <QComboBox>

// video/DeviceID and video/Device are no checked in case we don't have a camera

const QStringList neededSettings = {"video/ResolutionWidth",
                                    "video/ResolutionHeight",
                                    "video/WPP",
                                    "video/OWF",
                                    "video/InputFormat",
                                    "video/Slices",
                                    "video/kvzThreads",
                                    "video/yuvThreads",
                                    "video/rgbThreads",
                                    "video/OPENHEVC_threads",
                                    "video/FramerateID"};


VideoSettings::VideoSettings(QWidget* parent,
                             std::shared_ptr<CameraInfo> info)
  :
  QDialog(parent),
  currentDevice_(0),
  videoSettingsUI_(new Ui::VideoSettings),
  cam_(info),
  settings_("kvazzup.ini", QSettings::IniFormat)
{
  videoSettingsUI_->setupUi(this);

  // the buttons are named so that the slots are called automatically
  QObject::connect(videoSettingsUI_->format_box, SIGNAL(activated(QString)),
                   this, SLOT(initializeResolutions(QString)));
  QObject::connect(videoSettingsUI_->resolution, SIGNAL(activated(QString)),
                   this, SLOT(initializeFramerates(QString)));

  videoSettingsUI_->custom_parameters->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(videoSettingsUI_->custom_parameters, &QWidget::customContextMenuRequested,
          this, &VideoSettings::showParameterContextMenu);

  connect(videoSettingsUI_->bitrate_slider, &QSlider::valueChanged,
          this, &VideoSettings::updateBitrate);

  connect(videoSettingsUI_->wpp, &QCheckBox::clicked,
          this, &VideoSettings::updateSliceBoxStatus);

  connect(videoSettingsUI_->tiles_checkbox, &QCheckBox::clicked,
          this, &VideoSettings::updateSliceBoxStatus);

  connect(videoSettingsUI_->rc_algorithm,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &VideoSettings::updateObaStatus);
}


void VideoSettings::init(int deviceID)
{
  currentDevice_ = deviceID;

  restoreSettings();
}


void VideoSettings::showParameterContextMenu(const QPoint& pos)
{
  if (videoSettingsUI_->custom_parameters->rowCount() != 0)
  {
    showContextMenu(pos, videoSettingsUI_->custom_parameters, this,
                    QStringList() << "Delete", QStringList()
                    << SLOT(deleteListParameter()));
  }
}


void VideoSettings::deleteListParameter()
{
  videoSettingsUI_->custom_parameters->removeRow(videoSettingsUI_->custom_parameters->currentRow());
}


void VideoSettings::changedDevice(uint16_t deviceIndex)
{
  currentDevice_ = deviceIndex;
  initializeFormat();
  saveCameraCapabilities(deviceIndex); // record the new camerasettings.
}


void VideoSettings::resetSettings(int deviceID)
{
  printNormal(this, "Resettings video settings fomr UI");

  currentDevice_ = deviceID;
  initializeFormat();
  saveSettings();
  updateSliceBoxStatus();
  updateTilesStatus();
}


void VideoSettings::on_video_ok_clicked()
{
  printNormal(this, "Saving video settings");
  saveSettings();
  emit settingsChanged();
  //emit hidden();
  //hide();
}


void VideoSettings::on_video_close_clicked()
{
  printNormal(this, "Cancelled modifying video settings. Getting settings from system.");
  restoreSettings();
  hide();
  emit hidden();
}


void VideoSettings::on_add_parameter_clicked()
{
  printNormal(this, "Adding a custom parameter for kvazaar.");

  if (videoSettingsUI_->parameter_name->text() == "")
  {
    printWarning(this, "Parameter name not set");
    return;
  }

  QStringList list = QStringList() << videoSettingsUI_->parameter_name->text()
                                   << videoSettingsUI_->parameter_value->text();
  addFieldsToTable(list, videoSettingsUI_->custom_parameters);
}

void VideoSettings::saveSettings()
{
  printNormal(this, "Saving video Settings");

  // Video settings
  // input-tab
  saveCameraCapabilities(settings_.value("video/DeviceID").toInt());


  // Parallelization-tab
  saveTextValue("video/kvzThreads",        videoSettingsUI_->kvazaar_threads->currentText(), settings_);
  settings_.setValue("video/OWF",          videoSettingsUI_->owf->currentText());
  saveCheckBox("video/WPP",                videoSettingsUI_->wpp, settings_);

  saveCheckBox("video/Tiles",              videoSettingsUI_->tiles_checkbox, settings_);
  saveCheckBox("video/Slices",             videoSettingsUI_->slices, settings_);
  QString tile_dimension =                 QString::number(videoSettingsUI_->tile_x->value()) + "x" +
                                           QString::number(videoSettingsUI_->tile_y->value());
  saveTextValue("video/tileDimensions",    tile_dimension, settings_);

  saveTextValue("video/OPENHEVC_threads",  videoSettingsUI_->openhevc_threads->text(), settings_);
  saveTextValue("video/yuvThreads",        videoSettingsUI_->yuv_threads->text(), settings_);
  saveTextValue("video/rgbThreads",        videoSettingsUI_->rgb32_threads->text(), settings_);

  // structure-tab
  settings_.setValue("video/QP",           QString::number(videoSettingsUI_->qp->value()));
  saveTextValue("video/Intra",             videoSettingsUI_->intra->text(), settings_);
  saveTextValue("video/VPS",               videoSettingsUI_->vps->text(), settings_);

  saveTextValue("video/bitrate",           QString::number(videoSettingsUI_->bitrate_slider->value()), settings_);
  saveTextValue("video/rcAlgorithm",       videoSettingsUI_->rc_algorithm->currentText(), settings_);
  saveCheckBox("video/obaClipNeighbours",  videoSettingsUI_->oba_clip_neighbours, settings_);

  saveCheckBox("video/scalingList",        videoSettingsUI_->scaling_box, settings_);
  saveCheckBox("video/lossless",           videoSettingsUI_->lossless_box, settings_);
  saveTextValue("video/mvConstraint",      videoSettingsUI_->mv_constraint->currentText(), settings_);

  saveCheckBox("video/qpInCU",             videoSettingsUI_->qp_in_cu_box, settings_);
  saveTextValue("video/vaq",               QString::number(videoSettingsUI_->vaq->currentIndex()), settings_);

  // compression-tab
  settings_.setValue("video/Preset",       videoSettingsUI_->preset->currentText());
  listGUIToSettings("kvazzup.ini", "parameters", QStringList() << "Name" << "Value", videoSettingsUI_->custom_parameters);

  // Other-tab
  saveCheckBox("video/opengl",             videoSettingsUI_->opengl, settings_);
  saveCheckBox("video/flipViews",          videoSettingsUI_->flip, settings_);
}


void VideoSettings::saveCameraCapabilities(int deviceIndex)
{
  printNormal(this, "Recording capability settings for device",
              {"Device Index"}, {QString::number(deviceIndex)});

  int formatIndex = videoSettingsUI_->format_box->currentIndex();
  int resolutionIndex = videoSettingsUI_->resolution->currentIndex();

  printDebug(DEBUG_NORMAL, this, "Box status", {"Format", "Resolution"},
             {QString::number(formatIndex), QString::number(resolutionIndex)});

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
  settings_.setValue("video/FramerateID",          videoSettingsUI_->framerate_box->currentIndex());

  // TODO: does not work if minimum and maximum framerates differ or if framerate is fractional
  if (!videoSettingsUI_->framerate_box->currentText().isEmpty())
  {
    settings_.setValue("video/Framerate",            videoSettingsUI_->framerate_box->currentText());
  }
  else {
    settings_.setValue("video/Framerate",            0);
  }

  settings_.setValue("video/InputFormat",          format);

  printDebug(DEBUG_NORMAL, this, "Recorded following video settings.",
             {"Resolution", "Resolution Index", "Format"},
             {QString::number(res.width() - res.width()%8) + "x" + QString::number(res.height() - res.height()%8),
             QString::number(resolutionIndex), format});
}


void VideoSettings::restoreSettings()
{
  initializeFormat();
  initializeThreads();

  bool validSettings = checkMissingValues(settings_);
  if(validSettings && checkSettings())
  {
    printNormal(this, "Restoring previous video settings from file.",
                {"Filename"}, {settings_.fileName()});

    restoreComboBoxes();

    // input-tab
    videoSettingsUI_->format_box->setCurrentText(settings_.value("video/InputFormat").toString());

    int resolutionID = settings_.value("video/ResolutionID").toInt();
    videoSettingsUI_->resolution->setCurrentIndex(resolutionID);


    // parallelization-tab
    restoreComboBoxValue("video/kvzThreads", videoSettingsUI_->kvazaar_threads, "auto", settings_);
    restoreComboBoxValue("video/OWF", videoSettingsUI_->owf, "0", settings_);

    restoreCheckBox("video/WPP", videoSettingsUI_->wpp, settings_);

    restoreCheckBox("video/Tiles", videoSettingsUI_->tiles_checkbox, settings_);
    QString dimensions = settings_.value("video/tileDimensions").toString();

    QRegularExpression re_dimension("(\\d*)x(\\d*)");
    QRegularExpressionMatch dimension_match = re_dimension.match(dimensions);

    if (dimension_match.hasMatch() &&
        dimension_match.lastCapturedIndex() == 2)
    {
      int tile_x = dimension_match.captured(1).toInt();
      int tile_y = dimension_match.captured(2).toInt();

      if (videoSettingsUI_->tile_x->maximum() >= tile_x)
      {
        videoSettingsUI_->tile_x->setValue(tile_x);
      }
      else
      {
        videoSettingsUI_->tile_x->setValue(2);
      }

      if (videoSettingsUI_->tile_y->maximum() >= tile_y)
      {
        videoSettingsUI_->tile_y->setValue(tile_y);
      }
      else
      {
        videoSettingsUI_->tile_y->setValue(2);
      }
    }

    updateTilesStatus();

    restoreCheckBox("video/Slices", videoSettingsUI_->slices, settings_);

    videoSettingsUI_->openhevc_threads->setValue(settings_.value("video/OPENHEVC_threads").toInt());
    videoSettingsUI_->yuv_threads->setValue(settings_.value("video/yuvThreads").toInt());
    videoSettingsUI_->rgb32_threads->setValue(settings_.value("video/rgbThreads").toInt());

    updateSliceBoxStatus();

    // structure-tab
    videoSettingsUI_->qp->setValue            (settings_.value("video/QP").toInt());
    videoSettingsUI_->intra->setText          (settings_.value("video/Intra").toString());
    videoSettingsUI_->vps->setText            (settings_.value("video/VPS").toString());

    QString bitrate = settings_.value("video/bitrate").toString();
    videoSettingsUI_->bitrate_slider->setValue(bitrate.toInt());

    restoreComboBoxValue("video/rcAlgorithm", videoSettingsUI_->rc_algorithm, "lambda", settings_);

    restoreCheckBox("video/obaClipNeighbours", videoSettingsUI_->oba_clip_neighbours, settings_);
    restoreCheckBox("video/scalingList", videoSettingsUI_->scaling_box, settings_);
    restoreCheckBox("video/lossless", videoSettingsUI_->lossless_box, settings_);

    restoreComboBoxValue("video/mvConstraint", videoSettingsUI_->mv_constraint, "none", settings_);
    restoreCheckBox("video/qpInCU", videoSettingsUI_->qp_in_cu_box, settings_);

    videoSettingsUI_->vaq->setCurrentIndex( settings_.value("video/vaq").toInt());

    updateObaStatus(videoSettingsUI_->rc_algorithm->currentIndex());

    // tools-tab
    restoreComboBoxValue("video/Preset", videoSettingsUI_->preset, "ultrafast", settings_);

    listSettingsToGUI("kvazzup.ini", "parameters", QStringList() << "Name" << "Value",
                      videoSettingsUI_->custom_parameters);

    // other-tab
    restoreCheckBox("video/opengl", videoSettingsUI_->opengl, settings_);
    restoreCheckBox("video/flipViews", videoSettingsUI_->flip, settings_);
  }
  else
  {
    resetSettings(currentDevice_);
  }
}


void VideoSettings::restoreComboBoxes()
{
  restoreFormat();
  restoreResolution();
  restoreFramerate();
}


void VideoSettings::restoreFormat()
{
  if(videoSettingsUI_->format_box->count() > 0)
  {
    // initialize right format
    QString format = "";
    if(settings_.contains("video/InputFormat"))
    {
      format = settings_.value("video/InputFormat").toString();
      int formatIndex = videoSettingsUI_->format_box->findText(format);

      printDebug(DEBUG_NORMAL, this, "Trying to find format for camera",
                 {"Format", "Format index"}, {format, QString::number(formatIndex)});

      if(formatIndex != -1)
      {
        videoSettingsUI_->format_box->setCurrentIndex(formatIndex);
      }
      else {
        videoSettingsUI_->format_box->setCurrentIndex(0);
      }
    }
    else
    {
      videoSettingsUI_->format_box->setCurrentIndex(0);
    }

    initializeResolutions(videoSettingsUI_->format_box->currentText());
  }
}


void VideoSettings::restoreResolution()
{
  if(videoSettingsUI_->resolution->count() > 0)
  {
    int resolutionID = settings_.value("video/ResolutionID").toInt();

    if(resolutionID >= 0)
    {
      videoSettingsUI_->resolution->setCurrentIndex(resolutionID);
    }
    else {
      videoSettingsUI_->resolution->setCurrentIndex(0);
    }
  }
}


void VideoSettings::restoreFramerate()
{
  if(videoSettingsUI_->framerate_box->count() > 0)
  {
    int framerateID = settings_.value("video/FramerateID").toInt();

    if(framerateID < videoSettingsUI_->framerate_box->count())
    {
      videoSettingsUI_->framerate_box->setCurrentIndex(framerateID);
    }
    else {
      videoSettingsUI_->framerate_box->setCurrentIndex(videoSettingsUI_->framerate_box->count() - 1);
    }
  }
}


void VideoSettings::initializeThreads()
{
  int maxThreads = QThread::idealThreadCount();

  printNormal(this, "Max Threads", "Threads", QString::number(maxThreads));

  // because I don't think the number of threads has changed if we have already
  // added them.
  if (videoSettingsUI_->kvazaar_threads->count() == 0 ||
      videoSettingsUI_->owf->count() == 0)
  {
    videoSettingsUI_->kvazaar_threads->clear();
    videoSettingsUI_->owf->clear();
    videoSettingsUI_->kvazaar_threads->addItem("auto");
    videoSettingsUI_->kvazaar_threads->addItem("Main");
    videoSettingsUI_->owf->addItem("0");

    for (int i = 1; i <= maxThreads; ++i)
    {
      videoSettingsUI_->kvazaar_threads->addItem(QString::number(i));
      videoSettingsUI_->owf->addItem(QString::number(i));
    }
  }
}


void VideoSettings::initializeFormat()
{
  printNormal(this, "Initializing formats");
  QStringList formats;

  cam_->getVideoFormats(currentDevice_, formats);

  videoSettingsUI_->format_box->clear();
  for(int i = 0; i < formats.size(); ++i)
  {
    videoSettingsUI_->format_box->addItem(formats.at(i));
  }

  if(videoSettingsUI_->format_box->count() > 0)
  {
    videoSettingsUI_->format_box->setCurrentIndex(0);
    initializeResolutions(videoSettingsUI_->format_box->currentText());
  }
}


void VideoSettings::initializeResolutions(QString format)
{
  printNormal(this, "Initializing camera resolutions", {"Format"}, format);
  videoSettingsUI_->resolution->clear();
  QStringList resolutions;

  cam_->getFormatResolutions(currentDevice_, format, resolutions);

  if(!resolutions.empty())
  {
    for(int i = 0; i < resolutions.size(); ++i)
    {
      videoSettingsUI_->resolution->addItem(resolutions.at(i));
    }
  }

  if(videoSettingsUI_->resolution->count() > 0)
  {
    videoSettingsUI_->resolution->setCurrentIndex(0);
    initializeFramerates(videoSettingsUI_->resolution->currentText());
  }
}


void VideoSettings::initializeFramerates(QString resolution)
{
  printNormal(this, "Initializing camera framerates", {"Resolution"}, resolution);
  videoSettingsUI_->framerate_box->clear();
  QStringList rates;

  cam_->getFramerates(currentDevice_, videoSettingsUI_->format_box->currentText(),
                      videoSettingsUI_->resolution->currentIndex(), rates);

  if(!rates.empty())
  {
    for(int i = 0; i < rates.size(); ++i)
    {
      videoSettingsUI_->framerate_box->addItem(rates.at(i));
    }
    // use the highest framerate values as default selection.
    videoSettingsUI_->framerate_box->setCurrentIndex(videoSettingsUI_->framerate_box->count() - 1);
  }
}


bool VideoSettings::checkSettings()
{
  bool everythingPresent = checkMissingValues(settings_);

  for(auto& need : neededSettings)
  {
    if(!settings_.contains(need))
    {
      printError(this, "Found missing setting. Resetting video settings", {"Missing key"}, need);
      everythingPresent = false;
    }
  }
  return everythingPresent;
}

void VideoSettings::updateBitrate(int value)
{
  if (value == 0)
  {
    videoSettingsUI_->bitrate->setText("disabled");
  }
  else
  {
    value = roundToThousands(value);

    videoSettingsUI_->bitrate->setText(getBitrateString(value));
    videoSettingsUI_->bitrate_slider->setValue(value);
  }
}


void VideoSettings::updateSliceBoxStatus()
{
  if (videoSettingsUI_->wpp->checkState() ||
      videoSettingsUI_->tiles_checkbox->checkState())
  {
    videoSettingsUI_->slices_label->setDisabled(false);
    videoSettingsUI_->slices->setDisabled(false);
  }
  else
  {
    videoSettingsUI_->slices_label->setDisabled(true);
    videoSettingsUI_->slices->setDisabled(true);
    videoSettingsUI_->slices->setCheckState(Qt::CheckState::Unchecked);
  }
}

void VideoSettings::updateTilesStatus()
{
  if (videoSettingsUI_->tiles_checkbox->checkState())
  {
    videoSettingsUI_->tile_frame->setHidden(false);
    videoSettingsUI_->tile_split_label->setHidden(false);
  }
  else
  {
    videoSettingsUI_->tile_frame->setHidden(true);
    videoSettingsUI_->tile_split_label->setHidden(true);
  }
}


void VideoSettings::updateObaStatus(int index)
{
  Q_UNUSED(index);

  if (videoSettingsUI_->rc_algorithm->currentText() == "oba")
  {
    videoSettingsUI_->oba_clip_neighbours->setDisabled(false);
    videoSettingsUI_->oba_clip_neighbour_label->setDisabled(false);
  }
  else
  {
    videoSettingsUI_->oba_clip_neighbour_label->setDisabled(true);
    videoSettingsUI_->oba_clip_neighbours->setDisabled(true);
    videoSettingsUI_->oba_clip_neighbours->setCheckState(Qt::CheckState::Unchecked);
  }
}
