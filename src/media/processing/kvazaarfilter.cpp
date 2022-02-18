#include "kvazaarfilter.h"

#include "statisticsinterface.h"

#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <kvazaar.h>

#include <QtDebug>
#include <QTime>
#include <QSize>

enum RETURN_STATUS {C_SUCCESS = 0, C_FAILURE = -1};

KvazaarFilter::KvazaarFilter(QString id, StatisticsInterface *stats,
                             std::shared_ptr<HWResourceManager> hwResources):
  Filter(id, "Kvazaar", stats, hwResources, DT_YUV420VIDEO, DT_HEVCVIDEO),
  api_(nullptr),
  config_(nullptr),
  enc_(nullptr),
  pts_(0),
  input_pic_(nullptr),
  framerate_num_(30),
  framerate_denom_(1),
  encodingFrames_()
{
  maxBufferSize_ = 3;
}


void KvazaarFilter::updateSettings()
{
  Logger::getLogger()->printNormal(this, "Updating kvazaar settings");

  stop();

  while(isRunning())
  {
    sleep(1);
  }


  close();
  encodingFrames_.clear();

  if(init())
  {
    Logger::getLogger()->printNormal(this, "Resolution change successful");
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Failed to change resolution");
  }

  start();

  Filter::updateSettings();
}


bool KvazaarFilter::init()
{
  Logger::getLogger()->printNormal(this, "Iniating Kvazaar");

  // input picture should not exist at this point
  if(!input_pic_ && !api_)
  {

    api_ = kvz_api_get(8);
    if(!api_)
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Failed to retrieve Kvazaar API.");
      return false;
    }
    config_ = api_->config_alloc();
    enc_ = nullptr;

    if(!config_)
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Failed to allocate Kvazaar config.");
      return false;
    }
    QSettings settings(settingsFile, settingsFileFormat);

    api_->config_init(config_);
    api_->config_parse(config_, "preset", settings.value(SettingsKey::videoPreset).toString().toUtf8());

    // input

#ifdef __linux__

    if (settingEnabled(SettingsKey::screenShareStatus))
    {
      config_->width = settings.value(SettingsKey::videoResultionWidth).toInt();
      config_->height = settings.value(SettingsKey::videoResultionHeight).toInt();
      framerate_num_ = settings.value(SettingsKey::videoFramerate).toFloat();
      config_->framerate_num = framerate_num_;
    }
    else
    {
      // On Linux the Camerafilter seems to have a Qt bug that causes not being able to set resolution
      config_->width = 640;
      config_->height = 480;
      config_->framerate_num = 30;
    }
#else
    config_->width = settings.value(SettingsKey::videoResultionWidth).toInt();
    config_->height = settings.value(SettingsKey::videoResultionHeight).toInt();

    convertFramerate(settings.value(SettingsKey::videoFramerate).toReal());
    config_->framerate_num = framerate_num_;
#endif
    config_->framerate_denom = framerate_denom_;

    // parallelization

    if (settings.value(SettingsKey::videoKvzThreads) == "auto")
    {
      config_->threads = QThread::idealThreadCount();
    }
    else if (settings.value(SettingsKey::videoKvzThreads) == "Main")
    {
      config_->threads = 0;
    }
    else
    {
      config_->threads = settings.value(SettingsKey::videoKvzThreads).toInt();
    }

    config_->owf = settings.value(SettingsKey::videoOWF).toInt();
    config_->wpp = settings.value(SettingsKey::videoWPP).toInt();

    bool tiles = false;

    if (tiles)
    {
      std::string dimensions = settings.value(SettingsKey::videoTileDimensions).toString().toStdString();
      api_->config_parse(config_, "tiles", dimensions.c_str());
    }

    // this does not work with uvgRTP at the moment. Avoid using slices.
    if(settings.value(SettingsKey::videoSlices).toInt() == 1)
    {
      if(config_->wpp)
      {
        config_->slices = KVZ_SLICES_WPP;
      }
      else if (tiles)
      {
        config_->slices = KVZ_SLICES_TILES;
      }
    }

    // Structure

    config_->qp = settings.value(SettingsKey::videoQP).toInt();
    config_->intra_period = settings.value(SettingsKey::videoIntra).toInt();
    config_->vps_period = settings.value(SettingsKey::videoVPS).toInt();

    config_->target_bitrate = settings.value(SettingsKey::videoBitrate).toInt();

    if (config_->target_bitrate != 0)
    {
      QString rcAlgo = settings.value(SettingsKey::videoRCAlgorithm).toString();

      if (rcAlgo == "lambda")
      {
        config_->rc_algorithm = KVZ_LAMBDA;
      }
      else if (rcAlgo == "oba")
      {
        config_->rc_algorithm = KVZ_OBA;
        config_->clip_neighbour = settings.value(SettingsKey::videoOBAClipNeighbours).toInt();
      }
      else
      {
        Logger::getLogger()->printWarning(this, "Some carbage in rc algorithm setting");
        config_->rc_algorithm = KVZ_NO_RC;
      }
    }
    else
    {
      config_->rc_algorithm = KVZ_NO_RC;
    }

    config_->gop_lowdelay = 1;

    if (settings.value(SettingsKey::videoScalingList).toInt() == 0)
    {
      config_->scaling_list = KVZ_SCALING_LIST_OFF;
    }
    else
    {
      config_->scaling_list = KVZ_SCALING_LIST_DEFAULT;
    }

    config_->lossless = settings.value(SettingsKey::videoLossless).toInt();

    QString constraint = settings.value(SettingsKey::videoMVConstraint).toString();

    if (constraint == "frame")
    {
      config_->mv_constraint = KVZ_MV_CONSTRAIN_FRAME;
    }
    else if (constraint == "tile")
    {
      config_->mv_constraint = KVZ_MV_CONSTRAIN_TILE;
    }
    else if (constraint == "frametile")
    {
      config_->mv_constraint = KVZ_MV_CONSTRAIN_FRAME_AND_TILE;
    }
    else if (constraint == "frametilemargin")
    {
      config_->mv_constraint = KVZ_MV_CONSTRAIN_FRAME_AND_TILE_MARGIN;
    }
    else
    {
      config_->mv_constraint = KVZ_MV_CONSTRAIN_NONE;
    }

    config_->set_qp_in_cu = settings.value(SettingsKey::videoQPInCU).toInt();
    config_->vaq = settings.value(SettingsKey::videoVAQ).toInt();


    // compression-tab
    customParameters(settings);

    config_->hash = KVZ_HASH_NONE;

    enc_ = api_->encoder_open(config_);

    if(!enc_)
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Failed to open Kvazaar encoder.");
      return false;
    }

    input_pic_ = api_->picture_alloc(config_->width, config_->height);

    if(!input_pic_)
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Could not allocate input picture.");
      return false;
    }

    Logger::getLogger()->printNormal(this, "Kvazaar iniation succeeded");
  }

  return true;
}

void KvazaarFilter::close()
{
  if(api_)
  {
    api_->encoder_close(enc_);
    api_->config_destroy(config_);
    enc_ = nullptr;
    config_ = nullptr;

    api_->picture_free(input_pic_);
    input_pic_ = nullptr;
    api_ = nullptr;
  }

  pts_ = 0;

  Logger::getLogger()->printNormal(this, "Closed Kvazaar");
}

void KvazaarFilter::process()
{
  Q_ASSERT(enc_);
  Q_ASSERT(config_);

  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    if(!input_pic_)
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,  
                                      "Input picture was not allocated correctly.");
      break;
    }

    feedInput(std::move(input));

    input = getInput();
  }
}

void KvazaarFilter::customParameters(QSettings& settings)
{
  int size = settings.beginReadArray(SettingsKey::videoCustomParameters);

  Logger::getLogger()->printNormal(this, "Getting custom Kvazaar parameters",
                                   "Amount", QString::number(size));

  for(int i = 0; i < size; ++i)
  {
    settings.setArrayIndex(i);
    QString name = settings.value("Name").toString();
    QString value = settings.value("Value").toString();
    if (api_->config_parse(config_, name.toStdString().c_str(),
                           value.toStdString().c_str()) != 1)
    {
      Logger::getLogger()->printWarning(this, "Invalid custom parameter for kvazaar",
                                        "Amount", QString::number(size));
    }
  }
  settings.endArray();
}


void KvazaarFilter::feedInput(std::unique_ptr<Data> input)
{
  kvz_picture *recon_pic = nullptr;
  kvz_frame_info frame_info;
  kvz_data_chunk *data_out = nullptr;
  uint32_t len_out = 0;

  if (config_->width != input->vInfo->width
      || config_->height != input->vInfo->height
      || (double)(config_->framerate_num/config_->framerate_denom) != input->vInfo->framerate)
  {
    // This should not happen.
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "Input resolution or framerate differs from settings",
                                    {"Settings", "Input"},
                                    {QString::number(config_->width) + "x" +
                                     QString::number(config_->height) + "p" +
                                     QString::number(config_->framerate_num),
                                     QString::number(input->vInfo->width) + "x" +
                                     QString::number(input->vInfo->height) + "p" +
                                     QString::number(input->vInfo->framerate)});

    return;
  }

  // copy input to kvazaar picture
  memcpy(input_pic_->y,
         input->data.get(),
         input->vInfo->width*input->vInfo->height);
  memcpy(input_pic_->u,
         &(input->data.get()[input->vInfo->width*input->vInfo->height]),
         input->vInfo->width*input->vInfo->height/4);
  memcpy(input_pic_->v,
         &(input->data.get()[input->vInfo->width*input->vInfo->height + input->vInfo->width*input->vInfo->height/4]),
         input->vInfo->width*input->vInfo->height/4);

  input_pic_->pts = pts_;
  ++pts_;

  encodingFrames_.push_front(std::move(input));

  api_->encoder_encode(enc_, input_pic_,
                       &data_out, &len_out,
                       &recon_pic, nullptr,
                       &frame_info );

  while(data_out != nullptr)
  {
    parseEncodedFrame(data_out, len_out, recon_pic);

    // see if there is more output ready
    api_->encoder_encode(enc_, nullptr,
                         &data_out, &len_out,
                         &recon_pic, nullptr,
                         &frame_info );
  }
}


void KvazaarFilter::parseEncodedFrame(kvz_data_chunk *data_out,
                                      uint32_t len_out, kvz_picture *recon_pic)
{
  std::unique_ptr<Data> encodedFrame = std::move(encodingFrames_.back());
  encodingFrames_.pop_back();

  std::unique_ptr<uchar[]> hevc_frame(new uchar[len_out]);
  uint8_t* writer = hevc_frame.get();
  uint32_t dataWritten = 0;

  for (kvz_data_chunk *chunk = data_out; chunk != nullptr; chunk = chunk->next)
  {
    if(chunk->len > 3 && chunk->data[0] == 0 && chunk->data[1] == 0
       && ( chunk->data[2] == 1 || (chunk->data[2] == 0 && chunk->data[3] == 1 ))
       && dataWritten != 0 && config_->slices != KVZ_SLICES_NONE)
    {
      // send previous packet if this is not the first
      std::unique_ptr<Data> slice(shallowDataCopy(encodedFrame.get()));

      sendEncodedFrame(std::move(slice), std::move(hevc_frame), dataWritten);

      hevc_frame = std::unique_ptr<uchar[]>(new uchar[len_out - dataWritten]);
      writer = hevc_frame.get();
      dataWritten = 0;
    }

    memcpy(writer, chunk->data, chunk->len);
    writer += chunk->len;
    dataWritten += chunk->len;
  }
  api_->chunk_free(data_out);
  api_->picture_free(recon_pic);

  uint32_t delay = QDateTime::currentMSecsSinceEpoch() - encodedFrame->presentationTime;
  getStats()->sendDelay("video", delay);
  getStats()->addEncodedPacket("video", len_out);

  // send last packet reusing input structure
  sendEncodedFrame(std::move(encodedFrame), std::move(hevc_frame), dataWritten);
}


void KvazaarFilter::sendEncodedFrame(std::unique_ptr<Data> input,
                                     std::unique_ptr<uchar[]> hevc_frame,
                                     uint32_t dataWritten)
{
  input->type = DT_HEVCVIDEO;
  input->data_size = dataWritten;
  input->data = std::move(hevc_frame);
  sendOutput(std::move(input));
}


void KvazaarFilter::convertFramerate(double framerate)
{
  uint32_t wholeNumber = (uint32_t)framerate;

  double remainder = framerate - wholeNumber;

  if (remainder > 0.0)
  {
    uint32_t multiplier = 1.0 /remainder;
    framerate_num_ = framerate*multiplier;
    framerate_denom_ = multiplier;
  }
  else
  {
    framerate_num_ = wholeNumber;
    framerate_denom_ = 1;
  }

  Logger::getLogger()->printNormal(this, "Got framerate num and denum", "Framerate",
                                   {QString::number(framerate_num_) + "/" +
                                    QString::number(framerate_denom_) });
}
