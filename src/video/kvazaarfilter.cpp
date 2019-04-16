#include "kvazaarfilter.h"

#include "statisticsinterface.h"

#include <kvazaar.h>
#include <common.h>

#include <QtDebug>
#include <QTime>
#include <QSize>

enum RETURN_STATUS {C_SUCCESS = 0, C_FAILURE = -1};

KvazaarFilter::KvazaarFilter(QString id, StatisticsInterface *stats):
  Filter(id, "Kvazaar", stats, YUVVIDEO, HEVCVIDEO),
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
  qDebug() << "Updating kvazaar settings";
  close();
  if(init())
  {
    qDebug() << getName() << "Kvazaar resolution change successful";
  }
  else
  {
    qDebug() << "Failed to change resolution";
  }

  Filter::updateSettings();
}

bool KvazaarFilter::init()
{
  qDebug() << getName() << "iniating";

  // input picture should not exist at this point
  Q_ASSERT(!input_pic_ && !api_);

  api_ = kvz_api_get(8);
  if(!api_)
  {
    qCritical() << getName() << "failed to retrieve Kvazaar API";
    return false;
  }
  config_ = api_->config_alloc();
  enc_ = nullptr;

  if(!config_)
  {
    qCritical() << getName() << "failed to allocate Kvazaar config";
    return false;
  }
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  api_->config_init(config_);

#ifdef __linux__
  api_->config_parse(config_, "preset", "ultrafast");

  config_->width = 640;
  config_->height = 480;
  config_->framerate_num = 30;
#else
  api_->config_parse(config_, "preset", settings.value("video/Preset").toString().toUtf8());

  config_->width = settings.value("video/ResolutionWidth").toInt();
  config_->height = settings.value("video/ResolutionHeight").toInt();
  config_->threads = settings.value("video/kvzThreads").toInt();
  config_->qp = settings.value("video/QP").toInt();
  config_->wpp = settings.value("video/WPP").toInt();
  config_->vps_period = settings.value("video/VPS").toInt();
  config_->intra_period = settings.value("video/Intra").toInt();
  config_->framerate_num = settings.value("video/Framerate").toInt();
  config_->framerate_denom = framerate_denom_;
  config_->hash = KVZ_HASH_NONE;
#endif

  //config_->fme_level = 0;

  if(settings.value("video/Slices").toInt() == 1)
  {
    if(config_->wpp == 0)
    {
      api_->config_parse(config_, "tiles", "2x2");
      config_->slices = KVZ_SLICES_TILES;
    }
    else
    {
      config_->slices = KVZ_SLICES_WPP;
    }
  }

  // TODO Maybe send parameter sets only when needed
  //config_->target_bitrate = target_bitrate;

  customParameters(settings);

  enc_ = api_->encoder_open(config_);

  if(!enc_)
  {
    qCritical() << getName() << "failed to open Kvazaar encoder";
    return false;
  }

  input_pic_ = api_->picture_alloc(config_->width, config_->height);

  if(!input_pic_)
  {
    qCritical() << getName() << "could not allocate input picture";
    return false;
  }

  qDebug() << getName() << "iniation success";
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
  qDebug() << getName() << "Kvazaar closed";

  pts_ = 0;
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
      qCritical() << getName() << "input picture was not allocated correctly";
      break;
    }

    feedInput(std::move(input));
    // TODO: decrease latency by polling at least once more.
/*
    while(data_out == nullptr && encodingPresentationTimes_.size() != 0)
    {
      qSleep(3);
      api_->encoder_encode(enc_, nullptr,
                           &data_out, &len_out,
                           &recon_pic, nullptr,
                           &frame_info );
    }
*/

    input = getInput();
  }
}

void KvazaarFilter::customParameters(QSettings& settings)
{


  int size = settings.beginReadArray("parameters");

  qDebug() << "Initialization," << metaObject()->className()
           << "Getting custom Kvazaar options:" << size;

  for(int i = 0; i < size; ++i)
  {
    settings.setArrayIndex(i);
    QString name = settings.value("Name").toString();
    QString value = settings.value("Value").toString();
    if (api_->config_parse(config_, name.toStdString().c_str(), value.toStdString().c_str()) != 1)
    {
      qDebug() << "Initialization," << metaObject()->className() << ": Invalid custom parameter for kvazaar";
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

  if(config_->width != input->width
     || config_->height != input->height
     || config_->framerate_num != input->framerate)
  {
    // This should not happen.
    qDebug() << getName() << "WARNING: Input resolution or framerate differs:"
             << config_->width << "x" << config_->height << "input:"
             << input->width << "x" << input->height;

    qDebug() << getName() << "Framerate:" << config_->framerate_num << "input:" << input->framerate;

    QSettings settings("kvazzup.ini", QSettings::IniFormat);
    settings.setValue("video/ResolutionWidth", input->width);
    settings.setValue("video/ResolutionHeight", input->height);
    settings.setValue("video/Framerate", input->framerate);
    updateSettings();
  }


  // copy input to kvazaar picture
  memcpy(input_pic_->y,
         input->data.get(),
         input->width*input->height);
  memcpy(input_pic_->u,
         &(input->data.get()[input->width*input->height]),
         input->width*input->height/4);
  memcpy(input_pic_->v,
         &(input->data.get()[input->width*input->height + input->width*input->height/4]),
         input->width*input->height/4);

  input_pic_->pts = pts_;
  ++pts_;

  encodingFrames_.push_front(std::move(input));

  api_->encoder_encode(enc_, input_pic_,
                       &data_out, &len_out,
                       &recon_pic, nullptr,
                       &frame_info );

  if(data_out != nullptr)
  {
    parseEncodedFrame(data_out, len_out, recon_pic);
  }
}

void KvazaarFilter::parseEncodedFrame(kvz_data_chunk *data_out, uint32_t len_out, kvz_picture *recon_pic)
{
  std::unique_ptr<Data> encodedFrame = std::move(encodingFrames_.back());
  encodingFrames_.pop_back();

  uint32_t delay = QDateTime::currentMSecsSinceEpoch() -
      (encodedFrame->presentationTime.tv_sec * 1000 + encodedFrame->presentationTime.tv_usec/1000);
  getStats()->sendDelay("video", delay);
  getStats()->addEncodedPacket("video", len_out);

  std::unique_ptr<uchar[]> hevc_frame(new uchar[len_out]);
  uint8_t* writer = hevc_frame.get();
  uint32_t dataWritten = 0;

  for (kvz_data_chunk *chunk = data_out; chunk != nullptr; chunk = chunk->next)
  {
    if(chunk->len > 3 &&
       chunk->data[0] == 0 && chunk->data[1] == 0  &&( chunk->data[2] == 1 || (chunk->data[2] == 0 && chunk->data[3] == 1 ))
       && dataWritten != 0 && config_->slices != KVZ_SLICES_NONE)
    {
      // send previous packet if this is not the first

      // TODO: put delayes into deque, and set timestamp accordingly to get more accurate latency.
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

  // send last packet reusing input structure
  sendEncodedFrame(std::move(encodedFrame), std::move(hevc_frame), dataWritten);
}

void KvazaarFilter::sendEncodedFrame(std::unique_ptr<Data> input, std::unique_ptr<uchar[]> hevc_frame,
                                     uint32_t dataWritten)
{
  input->type = HEVCVIDEO;
  input->data_size = dataWritten;
  input->data = std::move(hevc_frame);
  sendOutput(std::move(input));
}
