#include "openhevcfilter.h"

#include "statisticsinterface.h"

#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QSettings>

enum OHThreadType {OH_THREAD_FRAME  = 1, OH_THREAD_SLICE  = 2, OH_THREAD_FRAMESLICE  = 3};

OpenHEVCFilter::OpenHEVCFilter(uint32_t sessionID, StatisticsInterface *stats,
                               std::shared_ptr<ResourceAllocator> hwResources):
  Filter(QString::number(sessionID), "OpenHEVC", stats, hwResources, DT_HEVCVIDEO, DT_YUV420VIDEO),
  handle_(),
  vpsReceived_(false),
  spsReceived_(false),
  ppsReceived_(false),
  sessionID_(sessionID),
  threads_(-1)
{}


bool OpenHEVCFilter::init()
{
  Logger::getLogger()->printNormal(this, "Starting to initiate OpenHEVC");
  QSettings settings(settingsFile, settingsFileFormat);

  threads_ = settings.value(SettingsKey::videoOpenHEVCThreads).toInt();
  handle_ = libOpenHevcInit(threads_, OH_THREAD_FRAME);

  //libOpenHevcSetDebugMode(handle_, 0);
  if(libOpenHevcStartDecoder(handle_) == -1)
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Failed to start decoder.");
    return false;
  }
  libOpenHevcSetTemporalLayer_id(handle_, 0);
  libOpenHevcSetActiveDecoders(handle_, 0);
  libOpenHevcSetViewLayers(handle_, 0);
  Logger::getLogger()->printNormal(this, "OpenHEVC initiation successful.", 
                                   {"Version"}, {libOpenHevcVersion(handle_)});

  // This is because we don't know anything about the incoming stream
  maxBufferSize_ = -1; // no buffer limit

  return true;
}


void OpenHEVCFilter::uninit()
{
  Logger::getLogger()->printNormal(this, "Uniniating.");
  libOpenHevcFlush(handle_);
  libOpenHevcClose(handle_);
}


void OpenHEVCFilter::updateSettings()
{
  QSettings settings(settingsFile, settingsFileFormat);
  if (settings.value(SettingsKey::videoOpenHEVCThreads).toInt() != threads_)
  {
    uninit();
    init();
  }
  Filter::updateSettings();
}


void OpenHEVCFilter::process()
{
  std::unique_ptr<Data> input = getInput();
  while(input)
  {
    getStats()->addReceivePacket(sessionID_, "Video", input->data_size);

    const unsigned char *buff = input->data.get();

    bool vps = (buff[4] >> 1) == 32;
    if (!vpsReceived_ && vps)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this,  "VPS found");
      vpsReceived_ = true;
    }

    bool sps = (buff[4] >> 1) == 33;
    if (!spsReceived_ && sps)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this,  "SPS found");
      spsReceived_ = true;
    }

    bool pps = (buff[4] >> 1) == 34;
    if (!ppsReceived_ && pps)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this,  "PPS found");
      ppsReceived_ = true;
    }

    if((vpsReceived_ && spsReceived_ && ppsReceived_) || vps || sps || pps)
    {
      int gotPicture = libOpenHevcDecode(handle_, input->data.get(), input->data_size, input->presentationTime);

      if (!vps && !sps && !pps)
      {
        decodingFrames_.push_front(std::move(input));
      }

      // TODO: Should figure out a way to get more output to reduce latency
      OpenHevc_Frame openHevcFrame;
      if( gotPicture == -1)
      {
        Logger::getLogger()->printDebug(DEBUG_ERROR, this,  "Error while decoding.");
      }
      else if (libOpenHevcGetOutput(handle_, gotPicture, &openHevcFrame) == -1)
      {
        Logger::getLogger()->printDebug(DEBUG_ERROR, this,  "Failed to get output.");
      }
      else if (gotPicture > 0)
      {
        std::unique_ptr<Data> decodedFrame = std::move(decodingFrames_.back());
        decodingFrames_.pop_back();

        libOpenHevcGetPictureInfo(handle_, &openHevcFrame.frameInfo);

        decodedFrame->vInfo->width = openHevcFrame.frameInfo.nWidth;
        decodedFrame->vInfo->height = openHevcFrame.frameInfo.nHeight;
        uint32_t finalDataSize = decodedFrame->vInfo->width*decodedFrame->vInfo->height +
            decodedFrame->vInfo->width*decodedFrame->vInfo->height/2;
        std::unique_ptr<uchar[]> yuv_frame(new uchar[finalDataSize]);

        uint8_t* pY = (uint8_t*)yuv_frame.get();
        uint8_t* pU = (uint8_t*)&(yuv_frame.get()[decodedFrame->vInfo->width*decodedFrame->vInfo->height]);
        uint8_t* pV = (uint8_t*)&(yuv_frame.get()[decodedFrame->vInfo->width*decodedFrame->vInfo->height +
            decodedFrame->vInfo->width*decodedFrame->vInfo->height/4]);

        uint32_t s_stride = openHevcFrame.frameInfo.nYPitch;
        uint32_t qs_stride = openHevcFrame.frameInfo.nUPitch/2;

        uint32_t d_stride = decodedFrame->vInfo->width/2;
        uint32_t dd_stride = decodedFrame->vInfo->width;

        for (int i=0; i<decodedFrame->vInfo->height; i++) {
          memcpy(pY,  (uint8_t *) openHevcFrame.pvY + i*s_stride, dd_stride);
          pY += dd_stride;

          if (! (i%2) ) {
            memcpy(pU,  (uint8_t *) openHevcFrame.pvU + i*qs_stride, d_stride);
            pU += d_stride;

            memcpy(pV,  (uint8_t *) openHevcFrame.pvV + i*qs_stride, d_stride);
            pV += d_stride;
          }
        }

        decodedFrame->type = DT_YUV420VIDEO;
        if (openHevcFrame.frameInfo.frameRate.den != 0)
        {
          decodedFrame->vInfo->framerate = openHevcFrame.frameInfo.frameRate.num/openHevcFrame.frameInfo.frameRate.den;
        }
        else
        {
          decodedFrame->vInfo->framerate = openHevcFrame.frameInfo.frameRate.num;
        }
        decodedFrame->data_size = finalDataSize;
        decodedFrame->data = std::move(yuv_frame);

        sendOutput(std::move(decodedFrame));
      }
    }
    else
    {
      Logger::getLogger()->printWarning(this, "Discarding frame until necessary structures have arrived");
    }

    input = getInput();
  }
}

