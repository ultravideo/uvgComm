#include "kvazaarfilter.h"

#include "statisticsinterface.h"

#include <kvazaar.h>
#include <common.h>

#include <QtDebug>
#include <QTime>
#include <QSize>

enum RETURN_STATUS {C_SUCCESS = 0, C_FAILURE = -1};

KvazaarFilter::KvazaarFilter(QString id, StatisticsInterface *stats):
  Filter(id, "Kvazaar", stats, YUV420VIDEO, HEVCVIDEO),
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
  encodingFrames_.clear();

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
  if(!input_pic_ && !api_)
  {

    api_ = kvz_api_get(8);
    if(!api_)
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, "Failed to retrieve Kvazaar API.");
      return false;
    }
    config_ = api_->config_alloc();
    enc_ = nullptr;

    if(!config_)
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, "Failed to allocate Kvazaar config.");
      return false;
    }
    QSettings settings("kvazzup.ini", QSettings::IniFormat);

    api_->config_init(config_);
    api_->config_parse(config_, "preset", settings.value("video/Preset").toString().toUtf8());

    // input

#ifdef __linux__
    // On Linux the Camerafilter seems to have a Qt bug that causes not being able to set resolution
    config_->width = 640;
    config_->height = 480;
    config_->framerate_num = 30;
#else
    config_->width = settings.value("video/ResolutionWidth").toInt();
    config_->height = settings.value("video/ResolutionHeight").toInt();
    config_->framerate_num = settings.value("video/Framerate").toInt();
#endif
    config_->framerate_denom = framerate_denom_;

    // parallelization

    if (settings.value("video/kvzThreads") == "auto")
    {
      config_->threads = QThread::idealThreadCount();
    }
    else if (settings.value("video/kvzThreads") == "Main")
    {
      config_->threads = 0;
    }
    else
    {
      config_->threads = settings.value("video/kvzThreads").toInt();
    }

    config_->owf = settings.value("video/OWF").toInt();
    config_->wpp = settings.value("video/WPP").toInt();

    bool tiles = false; //settings.value("video/WPP").toBool();

    if (tiles)
    {
      std::string dimensions = settings.value("video/tileDimensions").toString().toStdString();
      api_->config_parse(config_, "tiles", dimensions.c_str());
    }

    // this does not work with uvgRTP at the moment. Avoid using slices.
    if(settings.value("video/Slices").toInt() == 1)
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

    config_->qp = settings.value("video/QP").toInt();
    config_->intra_period = settings.value("video/Intra").toInt();
    config_->vps_period = settings.value("video/VPS").toInt();

    config_->target_bitrate = settings.value("video/bitrate").toInt();

    if (config_->target_bitrate != 0)
    {
      QString rcAlgo = settings.value("video/rcAlgorithm").toString();

      if (rcAlgo == "lambda")
      {
        config_->rc_algorithm = KVZ_LAMBDA;
      }
      else if (rcAlgo == "oba")
      {
        config_->rc_algorithm = KVZ_OBA;
        config_->clip_neighbour = settings.value("video/obaClipNeighbours").toInt();
      }
      else
      {
        printWarning(this, "Some carbage in rc algorithm setting");
        config_->rc_algorithm = KVZ_NO_RC;
      }
    }
    else
    {
      config_->rc_algorithm = KVZ_NO_RC;
    }

    config_->gop_lowdelay = 1;

    if (settings.value("video/scalingList").toInt() == 0)
    {
      config_->scaling_list = KVZ_SCALING_LIST_OFF;
    }
    else
    {
      config_->scaling_list = KVZ_SCALING_LIST_DEFAULT;
    }

    config_->lossless = settings.value("video/lossless").toInt();

    QString constraint = settings.value("video/mvConstraint").toString();

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

    config_->set_qp_in_cu = settings.value("video/qpInCU").toInt();

    config_->vaq = settings.value("video/vaq").toInt();


    // compression-tab
    customParameters(settings);

    config_->hash = KVZ_HASH_NONE;

    enc_ = api_->encoder_open(config_);

    if(!enc_)
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, "Failed to open Kvazaar encoder.");
      return false;
    }

    input_pic_ = api_->picture_alloc(config_->width, config_->height);

    if(!input_pic_)
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, "Could not allocate input picture.");
      return false;
    }

    qDebug() << getName() << "iniation succeeded.";
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
      printDebug(DEBUG_PROGRAM_ERROR, this,  "Input picture was not allocated correctly.");
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
    if (api_->config_parse(config_, name.toStdString().c_str(),
                           value.toStdString().c_str()) != 1)
    {
      qDebug() << "Initialization," << metaObject()->className()
               << ": Invalid custom parameter for kvazaar";
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
  input->type = HEVCVIDEO;
  input->data_size = dataWritten;
  input->data = std::move(hevc_frame);
  sendOutput(std::move(input));
}
