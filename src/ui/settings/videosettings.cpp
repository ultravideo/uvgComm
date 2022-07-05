#include "videosettings.h"

#include "ui_videosettings.h"

#include <ui/settings/camerainfo.h>
#include "settingshelper.h"
#include "settingskeys.h"

#include "common.h"
#include "logger.h"

#include <QTableWidgetItem>
#include <QThread>
#include <QComboBox>


VideoSettings::VideoSettings(QWidget* parent,
                             std::shared_ptr<CameraInfo> info)
  :
  QDialog(parent),
  currentDevice_(0),
  videoSettingsUI_(new Ui::VideoSettings),
  cam_(info),
  sharingScreen_(false),
  settings_(settingsFile, settingsFileFormat)
{
  videoSettingsUI_->setupUi(this);

  // the buttons are named so that the slots are called automatically
  // Overloads are needed, because QComboBox has overloaded the signal and
  // the connect can't figure out which one to use.
  QObject::connect(videoSettingsUI_->format_box, QOverload<int>::of(&QComboBox::activated),
                   this, &VideoSettings::refreshResolutions);
  QObject::connect(videoSettingsUI_->resolution, QOverload<int>::of(&QComboBox::activated),
                   this, &VideoSettings::refreshFramerates);

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
  videoSettingsUI_->custom_parameters->removeRow(
        videoSettingsUI_->custom_parameters->currentRow());
}


void VideoSettings::changedDevice(uint16_t deviceIndex)
{
  currentDevice_ = deviceIndex;
  restoreSettings();


  //saveCameraCapabilities(deviceIndex, !sharingScreen_); // record the new camerasettings.
}


void VideoSettings::on_video_ok_clicked()
{
  Logger::getLogger()->printNormal(this, "Saving video settings");
  saveSettings();
  emit updateVideoSettings();
  //emit hidden();
  //hide();
}


void VideoSettings::on_video_close_clicked()
{
  Logger::getLogger()->printNormal(this, "Cancelled modifying video settings. "
                                         "Getting settings from system.");
  restoreSettings();
  hide();
  emit hidden();
}


void VideoSettings::on_add_parameter_clicked()
{
  Logger::getLogger()->printNormal(this, "Adding a custom parameter for kvazaar.");

  if (videoSettingsUI_->parameter_name->text() == "")
  {
    Logger::getLogger()->printWarning(this, "Parameter name not set");
    return;
  }

  QStringList list = QStringList() << videoSettingsUI_->parameter_name->text()
                                   << videoSettingsUI_->parameter_value->text();
  addFieldsToTable(list, videoSettingsUI_->custom_parameters);
}


void VideoSettings::saveSettings()
{
  Logger::getLogger()->printNormal(this, "Saving video Settings");

  // Video settings
  // input-tab
  saveCameraCapabilities(settings_.value(SettingsKey::videoDeviceID).toInt(), !sharingScreen_);

  // Parallelization-tab
  saveTextValue(SettingsKey::videoKvzThreads,       videoSettingsUI_->kvazaar_threads->currentText(),
                settings_);
  settings_.setValue(SettingsKey::videoOWF,         videoSettingsUI_->owf->currentText());

  saveTextValue(SettingsKey::videoOHParallelization,  videoSettingsUI_->oh_parallelization_combo->currentText(),
                settings_);

  saveCheckBox(SettingsKey::videoWPP,               videoSettingsUI_->wpp, settings_);

  saveCheckBox(SettingsKey::videoTiles,             videoSettingsUI_->tiles_checkbox, settings_);
  saveCheckBox(SettingsKey::videoSlices,            videoSettingsUI_->slices, settings_);
  QString tile_dimension =                        QString::number(videoSettingsUI_->tile_x->value()) + "x" +
                                                  QString::number(videoSettingsUI_->tile_y->value());
  saveTextValue(SettingsKey::videoTileDimensions,   tile_dimension, settings_);

  saveTextValue(SettingsKey::videoOpenHEVCThreads,  videoSettingsUI_->openhevc_threads->text(),
                settings_);
  saveTextValue(SettingsKey::videoYUVThreads,       videoSettingsUI_->yuv_threads->text(),
                settings_);
  saveTextValue(SettingsKey::videoRGBThreads,       videoSettingsUI_->rgb32_threads->text(),
                settings_);

  // structure-tab
  settings_.setValue(SettingsKey::videoQP,          QString::number(videoSettingsUI_->qp->value()));
  saveTextValue(SettingsKey::videoIntra,            videoSettingsUI_->intra->text(),
                settings_);
  saveTextValue(SettingsKey::videoVPS,              videoSettingsUI_->vps->text(),
                settings_);

  saveTextValue(SettingsKey::videoBitrate,          QString::number(videoSettingsUI_->bitrate_slider->value()),
                settings_);
  saveTextValue(SettingsKey::videoRCAlgorithm,      videoSettingsUI_->rc_algorithm->currentText(),
                settings_);
  saveCheckBox(SettingsKey::videoOBAClipNeighbours, videoSettingsUI_->oba_clip_neighbours,
               settings_);

  saveCheckBox(SettingsKey::videoScalingList,       videoSettingsUI_->scaling_box,
               settings_);
  saveCheckBox(SettingsKey::videoLossless,          videoSettingsUI_->lossless_box,
               settings_);
  saveTextValue(SettingsKey::videoMVConstraint,     videoSettingsUI_->mv_constraint->currentText(),
                settings_);

  saveCheckBox(SettingsKey::videoQPInCU,            videoSettingsUI_->qp_in_cu_box, settings_);
  saveTextValue(SettingsKey::videoVAQ,              QString::number(videoSettingsUI_->vaq->currentIndex()),
                settings_);

  // compression-tab
  settings_.setValue(SettingsKey::videoPreset,      videoSettingsUI_->preset->currentText());
  listGUIToSettings(settingsFile, SettingsKey::videoCustomParameters,
                    QStringList() << "Name" << "Value", videoSettingsUI_->custom_parameters);

  // Other-tab
  saveCheckBox(SettingsKey::videoOpenGL,         videoSettingsUI_->opengl, settings_);
}


void VideoSettings::saveCameraCapabilities(int deviceIndex, bool cameraEnabled)
{
  if (cameraEnabled)
  {
    Logger::getLogger()->printNormal(this, "Recording capability settings for device",
                {"Device Index"}, {QString::number(deviceIndex)});

#ifdef __linux__
    // only these work on Linux with Qt 5
    settings_.setValue(SettingsKey::videoInputFormat,         "RGB32");
    settings_.setValue(SettingsKey::videoResultionWidth,      640);
    settings_.setValue(SettingsKey::videoResultionHeight,     480);
    settings_.setValue(SettingsKey::videoFramerate,           30);

#else
    int formatIndex = videoSettingsUI_->format_box->currentIndex();
    int resolutionIndex = videoSettingsUI_->resolution->currentIndex();

    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Box status", {"Format", "Resolution"},
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
    settings_.setValue(SettingsKey::videoResultionWidth,      res.width() - res.width()%8);
    settings_.setValue(SettingsKey::videoResultionHeight,     res.height() - res.height()%8);
    settings_.setValue(SettingsKey::videoResolutionID,         resolutionIndex);
    settings_.setValue(SettingsKey::videoFramerateID,
                       videoSettingsUI_->framerate_box->currentIndex());

    // TODO: does not work if minimum and maximum framerates differ or if framerate is fractional
    if (!videoSettingsUI_->framerate_box->currentText().isEmpty())
    {
      settings_.setValue(SettingsKey::videoFramerate,
                         videoSettingsUI_->framerate_box->currentText());
    }
    else {
      settings_.setValue(SettingsKey::videoFramerate,            0);
    }

    settings_.setValue(SettingsKey::videoInputFormat,          format);

    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Recorded following video settings.",
                                    {"Resolution", "Resolution Index", "Format"},
                                    {QString::number(res.width() - res.width()%8) + "x" +
                                     QString::number(res.height() - res.height()%8),
                                     QString::number(resolutionIndex), format});
#endif
  }
}


void VideoSettings::restoreSettings()
{
  initializeFormat();
  initializeThreads();
  initializeFramerates();

  Logger::getLogger()->printNormal(this, "Restoring previous video settings from file.",
              {"Filename"}, {settings_.fileName()});

  restoreComboBoxes();

  // input-tab
  videoSettingsUI_->format_box->setCurrentText
      (settings_.value(SettingsKey::videoInputFormat).toString());

  int resolutionID = settings_.value(SettingsKey::videoResolutionID).toInt();
  videoSettingsUI_->resolution->setCurrentIndex(resolutionID);


  // parallelization-tab
  restoreComboBoxValue(SettingsKey::videoKvzThreads, videoSettingsUI_->kvazaar_threads,
                       "auto", settings_);
  restoreComboBoxValue(SettingsKey::videoOWF, videoSettingsUI_->owf, "0", settings_);

  restoreComboBoxValue(SettingsKey::videoOHParallelization,
                       videoSettingsUI_->oh_parallelization_combo, "slice", settings_);

  restoreCheckBox(SettingsKey::videoWPP, videoSettingsUI_->wpp, settings_);

  restoreCheckBox(SettingsKey::videoTiles, videoSettingsUI_->tiles_checkbox, settings_);
  QString dimensions = settings_.value(SettingsKey::videoTileDimensions).toString();

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

  restoreCheckBox(SettingsKey::videoSlices, videoSettingsUI_->slices, settings_);

  videoSettingsUI_->openhevc_threads->setValue(
        settings_.value(SettingsKey::videoOpenHEVCThreads).toInt());
  videoSettingsUI_->yuv_threads->setValue(
        settings_.value(SettingsKey::videoYUVThreads).toInt());
  videoSettingsUI_->rgb32_threads->setValue(
        settings_.value(SettingsKey::videoRGBThreads).toInt());

  updateSliceBoxStatus();

  // structure-tab
  videoSettingsUI_->qp->setValue            (settings_.value(SettingsKey::videoQP).toInt());
  videoSettingsUI_->intra->setText          (settings_.value(SettingsKey::videoIntra).toString());
  videoSettingsUI_->vps->setText            (settings_.value(SettingsKey::videoVPS).toString());

  QString bitrate = settings_.value(SettingsKey::videoBitrate).toString();
  videoSettingsUI_->bitrate_slider->setValue(bitrate.toInt());

  restoreComboBoxValue(SettingsKey::videoRCAlgorithm, videoSettingsUI_->rc_algorithm,
                       "lambda", settings_);

  restoreCheckBox(SettingsKey::videoOBAClipNeighbours, videoSettingsUI_->oba_clip_neighbours,
                  settings_);
  restoreCheckBox(SettingsKey::videoScalingList, videoSettingsUI_->scaling_box,
                  settings_);
  restoreCheckBox(SettingsKey::videoLossless, videoSettingsUI_->lossless_box,
                  settings_);

  restoreComboBoxValue(SettingsKey::videoMVConstraint, videoSettingsUI_->mv_constraint,
                       "none", settings_);
  restoreCheckBox(SettingsKey::videoQPInCU, videoSettingsUI_->qp_in_cu_box,
                  settings_);

  videoSettingsUI_->vaq->setCurrentIndex( settings_.value(SettingsKey::videoVAQ).toInt());

  updateObaStatus(videoSettingsUI_->rc_algorithm->currentIndex());

  // tools-tab
  restoreComboBoxValue(SettingsKey::videoPreset, videoSettingsUI_->preset,
                       "ultrafast", settings_);

  listSettingsToGUI(settingsFile, SettingsKey::videoCustomParameters,
                    QStringList() << "Name" << "Value",
                    videoSettingsUI_->custom_parameters);

  // other-tab
  restoreCheckBox(SettingsKey::videoOpenGL, videoSettingsUI_->opengl, settings_);
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
    if(settings_.contains(SettingsKey::videoInputFormat))
    {
      format = settings_.value(SettingsKey::videoInputFormat).toString();
      int formatIndex = videoSettingsUI_->format_box->findText(format);

      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Trying to find format for camera",
                                      {"Format", "Format index"}, {format, QString::number(formatIndex)});

      if (0 <= formatIndex && formatIndex < videoSettingsUI_->format_box->count())
      {
        videoSettingsUI_->format_box->setCurrentIndex(formatIndex);
      }
      else
      {
        videoSettingsUI_->format_box->setCurrentIndex(0);
      }
    }
    else
    {
      videoSettingsUI_->format_box->setCurrentIndex(0);
    }

    initializeResolutions();
  }
}


void VideoSettings::restoreResolution()
{
  if (videoSettingsUI_->resolution->count() > 0)
  {
    int resolutionID = settings_.value(SettingsKey::videoResolutionID).toInt();

    if (0 <= resolutionID &&  resolutionID < videoSettingsUI_->resolution->count())
    {
      videoSettingsUI_->resolution->setCurrentIndex(resolutionID);
    }
    else
    {
      videoSettingsUI_->resolution->setCurrentIndex(0);
    }

    initializeFramerates();
  }
}


void VideoSettings::restoreFramerate()
{
  if(videoSettingsUI_->framerate_box->count() > 0)
  {
    int framerateID = settings_.value(SettingsKey::videoFramerateID).toInt();

    if (0 <= framerateID && framerateID < videoSettingsUI_->framerate_box->count())
    {
      videoSettingsUI_->framerate_box->setCurrentIndex(framerateID);
    }
    else
    {
      videoSettingsUI_->framerate_box->setCurrentIndex(videoSettingsUI_->framerate_box->count() - 1);
    }
  }
}


void VideoSettings::initializeThreads()
{
  int maxThreads = QThread::idealThreadCount();

  Logger::getLogger()->printNormal(this, "Max Threads", "Threads", QString::number(maxThreads));

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

  videoSettingsUI_->openhevc_threads->setMaximum(maxThreads);
  videoSettingsUI_->yuv_threads->setMaximum(maxThreads);
  videoSettingsUI_->rgb32_threads->setMaximum(maxThreads);
}


void VideoSettings::initializeFormat()
{
  Logger::getLogger()->printNormal(this, "Initializing formats");
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
    initializeResolutions();
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Couldn't find any camera formats");
  }

}


void VideoSettings::initializeResolutions()
{
  Logger::getLogger()->printNormal(this, "Initializing camera resolutions", {"Format"},
              videoSettingsUI_->format_box->currentText());
  videoSettingsUI_->resolution->clear();
  QStringList resolutions;

  cam_->getFormatResolutions(currentDevice_,
                             videoSettingsUI_->format_box->currentText(), resolutions);

  if(!resolutions.empty())
  {
    for(int i = 0; i < resolutions.size(); ++i)
    {
      videoSettingsUI_->resolution->addItem(resolutions.at(i));
    }
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Couldn't find any camera resolutions");
  }

  if(videoSettingsUI_->resolution->count() > 0)
  {
    videoSettingsUI_->resolution->setCurrentIndex(0);
    initializeFramerates();
  }
}


void VideoSettings::initializeFramerates()
{
  Logger::getLogger()->printNormal(this, "Initializing camera framerates", {"Resolution"},
              videoSettingsUI_->resolution->currentText());

  videoSettingsUI_->framerate_box->clear();
  QStringList rates;

  cam_->getFramerates(currentDevice_, videoSettingsUI_->format_box->currentText(),
                      videoSettingsUI_->resolution->currentIndex(), rates);

  if (!rates.empty())
  {
    for(int i = 0; i < rates.size(); ++i)
    {
      videoSettingsUI_->framerate_box->addItem(rates.at(i));
    }
    // use the highest framerate values as default selection.
    videoSettingsUI_->framerate_box->setCurrentIndex(videoSettingsUI_->framerate_box->count() - 1);
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Couldn't find any camera frame rates");
  }
}


void VideoSettings::refreshResolutions(int index)
{
  Q_UNUSED(index)
  initializeResolutions();
}


void VideoSettings::refreshFramerates(int index)
{
  Q_UNUSED(index)
  initializeFramerates();
}


void VideoSettings::updateBitrate(int value)
{
  if (value == 0)
  {
    videoSettingsUI_->bitrate->setText("disabled");
  }
  else
  {
    value = roundToNumber(value, 1000);

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


void VideoSettings::closeEvent(QCloseEvent *event)
{
  on_video_close_clicked();
  QDialog::closeEvent(event);
}
