#include "kvazaarfilter.h"

#include "statisticsinterface.h"
#include "src/media/resourceallocator.h"

#include "settingskeys.h"
#include "logger.h"
#include "common.h"

#include <kvazaar.h>

#include <QtDebug>
#include <QTime>
#include <QSize>

enum RETURN_STATUS {C_SUCCESS = 0, C_FAILURE = -1};

const int CU_MIN_SIZE_PIXELS = 8;

unsigned get_padding(unsigned width_or_height)
{
  if (width_or_height % CU_MIN_SIZE_PIXELS)
  {
    return CU_MIN_SIZE_PIXELS - (width_or_height % CU_MIN_SIZE_PIXELS);
  }
  else
  {
    return 0;
  }
}


KvazaarFilter::KvazaarFilter(QString id, StatisticsInterface *stats,
                             std::shared_ptr<ResourceAllocator> hwResources,
                             std::pair<uint16_t, uint16_t> resolution):
  Filter(id, "Kvazaar", stats, hwResources, DT_YUV420VIDEO, DT_HEVCVIDEO),
  api_(nullptr),
  config_(nullptr),
  currentResolution_(),
  encoders_(),
  pts_(0),
  encodingFrames_(),
  inputPics_(),
  nextInputPic_(-1),
  timestampInterval_(0),
  currentFrame_(0),
  initialized_(false)
{
  maxBufferSize_ = 30;
}


void KvazaarFilter::restartEncoder()
{
  reInitializeKvazaar();
}


void KvazaarFilter::createInputVector(int size)
{
  cleanupInputVector();

  for (unsigned int i = 0; i < size; ++i)
  {
    addInputPic(inputPics_.size());
  }

  nextInputPic_ = 0;
}


void KvazaarFilter::cleanupInputVector()
{
  if (!api_)
  {
    Logger::getLogger()->printProgramError(this, "Make sure Kvazaar API exists when cleaning input vector");
    return;
  }
  for (auto pic : inputPics_)
  {
     pic->roi.roi_array = nullptr;
     api_->picture_free(pic);
  }

  inputPics_.clear();
  nextInputPic_ = -1;
}

void KvazaarFilter::addInputPic(int index)
{
  if (!api_ || !config_)
  {
    Logger::getLogger()->printProgramError(this, "Initilize API and config before creating input vector");
    return;
  }

  int padding_x = get_padding(config_->width);
  int padding_y = get_padding(config_->height);

  kvz_picture* pic = api_->picture_alloc(config_->width + padding_x, config_->height + padding_y);
  if (pic)
  {
    inputPics_.insert(inputPics_.begin() + index, pic);
  }
}

kvz_picture* KvazaarFilter::getNextPic()
{
  // if we have no free pics, add more slots
  if (encodingFrames_.size() == inputPics_.size())
  {
    Logger::getLogger()->printNormal(this, "Increasing Kvazaar input pic vector size");
    addInputPic(nextInputPic_);
  }

  kvz_picture* inputPic = inputPics_.at(nextInputPic_);
  nextInputPic_ = (nextInputPic_ + 1)%inputPics_.size();
  return inputPic;
}


void KvazaarFilter::updateSettings()
{
  QSettings settings(getSettingsFile(), settingsFileFormat);
  timestampInterval_ = settings.value(SettingsKey::sipTimestampInterval).toInt();

  Logger::getLogger()->printNormal(this, "Updating kvazaar settings",
                                   "Timestamp Interval", QString::number(timestampInterval_));

  reInitializeKvazaar();

  Filter::updateSettings();
}


void KvazaarFilter::reInitializeKvazaar()
{
  stop();
  initialized_ = false;
  while(isRunning())
  {
    Logger::getLogger()->printNormal(this, "Waiting for Kvazaar to stop...");
    sleep(1);
  }

  settingsMutex_.lock();
  for (auto encoder : encoders_)
  {
    close(encoder.first);
  }
  encoders_.clear();

  if(init())
  {
    Logger::getLogger()->printNormal(this, "Resolution change successful");
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Failed to change resolution");
  }
  encodingFrames_.clear();
  settingsMutex_.unlock();

  start();
}


bool KvazaarFilter::init()
{
  QSettings settings(getSettingsFile(), settingsFileFormat);
  timestampInterval_ = settings.value(SettingsKey::sipTimestampInterval).toInt();
  int enumerator = settings.value(SettingsKey::videoFramerateNumerator).toInt();
  int denominator = settings.value(SettingsKey::videoFramerateDenominator).toInt();

  Logger::getLogger()->printNormal(this, "Iniating Kvazaar",
                                   "Timestamp interval", QString::number(timestampInterval_));


  if (!api_)
  {
    api_ = kvz_api_get(8);

    if(!api_)
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Failed to retrieve Kvazaar API.");
      return false;
    }
  }

  config_ = api_->config_alloc();

  if(!config_)
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Failed to allocate Kvazaar config.");
    return false;
  }

  api_->config_init(config_);

  QString preset = settings.value(SettingsKey::videoPreset).toString().toUtf8();

  config_->target_bitrate = getHWManager()->getEncoderBitrate(DT_HEVCVIDEO);
  QSize partResolution = getHWManager()->getVideoResolution();

  if (partResolution.width() <= 0 || partResolution.height() <= 0)
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "Invalid resolution settings",
                                    {"Width", "Height"},
                                    {QString::number(partResolution.width()),
                                     QString::number(partResolution.height())});
    return false;
  }

  QString resolutionStr = QString::number(partResolution.width()) + "x" + QString::number(partResolution.height());
  QString framerateStr = QString::number(enumerator) + "/" +  QString::number(denominator);

  if (settingEnabled(SettingsKey::videoFileEnabled))
  {
    framerateStr = QString::number(settings.value(SettingsKey::videoFileFramerate).toInt()) + "/" +
                   QString::number(1);
    if (settings.value(SettingsKey::videoFileFramerate).toInt() <= 0)
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                      "Invalid framerate settings",
                                      {"Framerate"},
                                      {framerateStr});
      return false;
    }
  }
  else if (enumerator <= 0 || denominator <= 0)
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "Invalid framerate settings",
                                    {"Numerator", "Denominator"},
                                    {QString::number(enumerator),
                                     QString::number(denominator)});
    return false;
  }

  // Input
  api_->config_parse(config_, "preset",    preset.toLocal8Bit());
  api_->config_parse(config_, "input-res", resolutionStr.toLocal8Bit());
  api_->config_parse(config_, "input-fps", framerateStr.toLocal8Bit());

  QString threads = "0";

  // parallelization
  if (settings.value(SettingsKey::videoKvzThreads) == "auto")
  {
    threads = QString::number(QThread::idealThreadCount());
  }
  else if (settings.value(SettingsKey::videoKvzThreads) == "Main")
  {
    threads = QString::number(0);
  }
  else
  {
    threads = settings.value(SettingsKey::videoKvzThreads).toString();
  }

  api_->config_parse(config_, "threads", threads.toLocal8Bit());
  api_->config_parse(config_, "owf", settings.value(SettingsKey::videoOWF).toString().toLocal8Bit());
  api_->config_parse(config_, "wpp", settings.value(SettingsKey::videoWPP).toString().toLocal8Bit());

  bool tiles = settings.value(SettingsKey::videoTiles).toBool();

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
      api_->config_parse(config_, "slices", "wpp");
    }
    else if (tiles)
    {
      api_->config_parse(config_, "slices", "tiles");
    }
  }

  // Video structure

  api_->config_parse(config_, "qp",         settings.value(SettingsKey::videoQP).toString().toLocal8Bit());
  api_->config_parse(config_, "period",     settings.value(SettingsKey::videoIntra).toString().toLocal8Bit());
  api_->config_parse(config_, "vps-period", settings.value(SettingsKey::videoVPS).toString().toLocal8Bit());

  if (config_->target_bitrate != 0)
  {
    api_->config_parse(config_, "rc-algorithm",    settings.value(SettingsKey::videoRCAlgorithm).toString().toLocal8Bit());
  }

  api_->config_parse(config_, "intra-bits", "1");

  // TODO: Move to settings
  api_->config_parse(config_, "gop", "lp-g4d3t1");

  if (settings.value(SettingsKey::videoScalingList).toInt() == 0)
  {
    api_->config_parse(config_, "scaling-list", "off");
  }
  else
  {
    api_->config_parse(config_, "scaling-list", "default");
  }

  config_->lossless = settings.value(SettingsKey::videoLossless).toInt();

  QString constraint = settings.value(SettingsKey::videoMVConstraint).toString();

  if (constraint == "frame" || constraint == "frametile" || constraint == "frametilemargin")
  {
    api_->config_parse(config_, "mv-constraint", "");
  }
  else
  {
    api_->config_parse(config_, "mv-constraint", "none");
  }

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

  int vaq = settings.value(SettingsKey::videoVAQ).toInt();
  if (vaq > 0 && vaq <= 20)
  {
    api_->config_parse(config_, "vaq", settings.value(SettingsKey::videoVAQ).toString().toLocal8Bit());
  }

  // compression-tab
  customParameters(settings);

  config_->hash = KVZ_HASH_NONE;

  // This is partial code for a system that is not working. A system was planned that would hot swap encoders with
  // different resolutions, but since resetting worked well enough, I gave up on that idea
  currentResolution_ = std::make_pair(partResolution.width(), partResolution.height());
  encoders_[currentResolution_] = api_->encoder_open(config_);

  if(!encoders_[currentResolution_])
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Failed to open Kvazaar encoder.");
    return false;
  }

  createInputVector(config_->owf + 1);

  if(inputPics_.empty())
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Could not allocate input picture vector!");
    return false;
  }

  Logger::getLogger()->printNormal(this, "Kvazaar iniation succeeded", "Resolution", resolutionStr);
  initialized_ = true;
  return true;
}


void KvazaarFilter::close(std::pair<int, int> resolution)
{
  if(api_ && encoders_.find(resolution) != encoders_.end())
  {
    api_->encoder_close(encoders_[resolution]);
    api_->config_destroy(config_);
    config_ = nullptr;

    cleanupInputVector();
    api_ = nullptr;
  }

  pts_ = 0;

  Logger::getLogger()->printNormal(this, "Closed Kvazaar");
}


void KvazaarFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input && initialized_)
  {
    if(inputPics_.empty())
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                      "Input pictures have not been allocated");
      break;
    }
    settingsMutex_.lock();
    feedInput(std::move(input));
    settingsMutex_.unlock();

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
      || config_->framerate_num != input->vInfo->framerateNumerator
      || config_->framerate_denom != input->vInfo->framerateDenominator)
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
                                     QString::number(input->vInfo->framerateNumerator) + "/" +
                                     QString::number(input->vInfo->framerateDenominator)});

    return;
  }

  if (nextInputPic_ == -1 || nextInputPic_ >= inputPics_.size())
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Input vec initilized incorrectly");
    return;
  }

  kvz_picture* inputPic = getNextPic();

  // copy input to kvazaar picture
  memcpy(inputPic->y,
         input->data.get(),
         input->vInfo->width*input->vInfo->height);
  memcpy(inputPic->u,
         &(input->data.get()[input->vInfo->width*input->vInfo->height]),
         input->vInfo->width*input->vInfo->height/4);
  memcpy(inputPic->v,
         &(input->data.get()[input->vInfo->width*input->vInfo->height + input->vInfo->width*input->vInfo->height/4]),
         input->vInfo->width*input->vInfo->height/4);

  inputPic->pts = pts_;
  ++pts_;

  if (config_->target_bitrate == 0)
  {
    // can also be empty by default
    inputPic->roi.width = input->vInfo->roi.width;
    inputPic->roi.height = input->vInfo->roi.height;

    // needs to be deleted later
    inputPic->roi.roi_array = input->vInfo->roi.data.release();
  }

  encodingFrames_.push_front({std::move(input), inputPic, inputPic->roi.roi_array});

  api_->encoder_encode(encoders_[currentResolution_], inputPic,
                       &data_out, &len_out,
                       &recon_pic, nullptr,
                       &frame_info );

  while(data_out != nullptr)
  {
    parseEncodedFrame(data_out, len_out, recon_pic);

    // see if there is more output ready
    api_->encoder_encode(encoders_[currentResolution_], nullptr,
                         &data_out, &len_out,
                         &recon_pic, nullptr,
                         &frame_info );
  }
}


void KvazaarFilter::parseEncodedFrame(kvz_data_chunk *data_out,
                                      uint32_t len_out, kvz_picture *recon_pic)
{
  FrameInfo info = std::move(encodingFrames_.back());
  encodingFrames_.pop_back();

  if (info.roi_array)
  {
    delete info.roi_array;
    info.roi_array = nullptr;
  }

  ++currentFrame_;

  bool addTimestamp = (timestampInterval_ > 0 &&
                          (currentFrame_ % timestampInterval_) == 0);

  uint32_t timestampSize = 0;

  if (addTimestamp)
  {
    timestampSize += sizeof(uint32_t); // start code
    timestampSize += sizeof(uint16_t); // NAL header
    timestampSize += sizeof(uint8_t); // Payload type
    timestampSize += sizeof(uint16_t); // Payload size
    timestampSize += sizeof(uint64_t)*2; // UUID
    timestampSize += sizeof(int64_t);  // timestamp
  }

  std::unique_ptr<uchar[]> hevc_frame(new uchar[len_out + timestampSize]);
  uint8_t* writer = hevc_frame.get();
  uint32_t dataWritten = 0;


  if (addTimestamp)
  {
    // start code
    writer[0] = 0x00;
    writer[1] = 0x00;
    writer[2] = 0x00;
    writer[3] = 0x01;
    writer += 4;

    // HEVC NAL header
    writer[0] = KVZ_NAL_PREFIX_SEI_NUT << 1;
    writer[1] = 1;
    writer += 2;

    // Payload type
    writer [0] = 5;
    writer += 1;

    // Payload size
    writer[0] = 0;
    writer[1] = sizeof(int64_t);
    writer += 2;

    // UUID
    for (int i = 0; i < 16; ++i)
    {
      writer[i] = 1;
    }
    writer += 16;

    int64_t timestamp = info.data->creationTimestamp;
    memcpy(writer, &timestamp, sizeof(int64_t));
    writer += sizeof(int64_t);

    dataWritten += timestampSize;
  }

  for (kvz_data_chunk *chunk = data_out; chunk != nullptr; chunk = chunk->next)
  {
    memcpy(writer, chunk->data, chunk->len);
    writer += chunk->len;
    dataWritten += chunk->len;
  }

  double psnr_y  = -1.0;
  double psnr_u  = -1.0;
  double psnr_v  = -1.0;

  if (info.inputPic && recon_pic)
  {
    calculate_psnr(info.inputPic, recon_pic, psnr_y, psnr_u, psnr_v);
  }

  api_->chunk_free(data_out);
  api_->picture_free(recon_pic);
  
  auto now = std::chrono::system_clock::now();
  int64_t since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  uint32_t delay = since_epoch - info.data->creationTimestamp;

  getStats()->encodedVideoFrame(len_out, delay, QSize(currentResolution_.first, currentResolution_.second),
                                psnr_y, psnr_u, psnr_v, info.data->creationTimestamp);

  // send last packet reusing input structure
  sendEncodedFrame(std::move(info.data), std::move(hevc_frame), dataWritten);
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


void KvazaarFilter::calculate_psnr(const kvz_picture *orig, const kvz_picture *recon,
                                   double& psnr_y, double& psnr_u, double& psnr_v)
{
  const int max_val = 255;
  double mse_y = 0.0, mse_u = 0.0, mse_v = 0.0;

  int width = orig->width;
  int height = orig->height;

  // Luma (Y)
  int y_size = width * height;
  for (int i = 0; i < y_size; ++i) {
    int diff = orig->y[i] - recon->y[i];
    mse_y += diff * diff;
  }
  mse_y /= y_size;

  // Chroma (U and V), subsampled (4:2:0 assumed)
  int uv_width = width / 2;
  int uv_height = height / 2;
  int uv_size = uv_width * uv_height;

  for (int i = 0; i < uv_size; ++i) {
    int diff_u = orig->u[i] - recon->u[i];
    int diff_v = orig->v[i] - recon->v[i];
    mse_u += diff_u * diff_u;
    mse_v += diff_v * diff_v;
  }
  mse_u /= uv_size;
  mse_v /= uv_size;

  psnr_y = (mse_y == 0) ? INFINITY : 10.0 * log10((max_val * max_val) / mse_y);
  psnr_u = (mse_u == 0) ? INFINITY : 10.0 * log10((max_val * max_val) / mse_u);
  psnr_v = (mse_v == 0) ? INFINITY : 10.0 * log10((max_val * max_val) / mse_v);

/*
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "PSNR",
                                   {"Y", "U", "V"},
                                   {QString::number(psnr_y, 'f', 2),
                                    QString::number(psnr_u, 'f', 2),
                                    QString::number(psnr_v, 'f', 2)});
*/
}
