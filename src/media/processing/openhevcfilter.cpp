#include "openhevcfilter.h"

#include "statisticsinterface.h"

#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QSettings>

enum OHThreadType {OH_THREAD_FRAME  = 1, OH_THREAD_SLICE = 2, OH_THREAD_FRAMESLICE  = 3};


OpenHEVCFilter::OpenHEVCFilter(uint32_t sessionID, StatisticsInterface *stats,
                               std::shared_ptr<ResourceAllocator> hwResources):
  Filter(QString::number(sessionID), "OpenHEVC", stats, hwResources, DT_HEVCVIDEO, DT_YUV420VIDEO),
  handle_(),
  vpsReceived_(false),
  spsReceived_(false),
  ppsReceived_(false),
  sessionID_(sessionID),
  threads_(-1),
  parallelizationMode_("Slice")
{}


bool OpenHEVCFilter::init()
{
  Logger::getLogger()->printNormal(this, "Starting to initiate OpenHEVC");
  QSettings settings(settingsFile, settingsFileFormat);

  threads_ = settings.value(SettingsKey::videoOpenHEVCThreads).toInt();
  parallelizationMode_ = settings.value(SettingsKey::videoOHParallelization).toString();

  if (parallelizationMode_ == "Slice")
  {
    handle_ = libOpenHevcInit(threads_, OH_THREAD_SLICE);
  }
  else if (parallelizationMode_ == "Frame")
  {
    handle_ = libOpenHevcInit(threads_, OH_THREAD_FRAME);
  }
  else
  {
    handle_ = libOpenHevcInit(threads_, OH_THREAD_FRAMESLICE);
  }

  if(libOpenHevcStartDecoder(handle_) == -1)
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Failed to start decoder.");
    return false;
  }
  libOpenHevcSetTemporalLayer_id(handle_, 0);
  libOpenHevcSetActiveDecoders(handle_, 0);
  libOpenHevcSetViewLayers(handle_, 0);

  decodingFrames_.clear();

  // libOpenHevcSetDebugMode(handle_, OHEVC_LOG_DEBUG);

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "OpenHEVC initiation successful.",
                                   {"Version", "Threads", "Parallelization"},
                                  {libOpenHevcVersion(handle_), QString::number(threads_),
                                  parallelizationMode_});

  // This is because we don't know anything about the incoming stream
  maxBufferSize_ = -1; // no buffer limit

  vpsReceived_ = false;
  spsReceived_ = false;
  ppsReceived_ = false;
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

  if (settings.value(SettingsKey::videoOpenHEVCThreads).toInt() != threads_ ||
      settings.value(SettingsKey::videoOHParallelization).toString() != parallelizationMode_)
  {
    settingsMutex_.lock();
    uninit();
    init();
    settingsMutex_.unlock();
  }

  Filter::updateSettings();
}


void OpenHEVCFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    getStats()->addReceivePacket(sessionID_, "Video", input->data_size);
    settingsMutex_.lock();

    const unsigned char *buff = input->data.get();

    uint8_t nalType = (buff[4] >> 1);

    if (!vpsReceived_ && nalType == VPS_NUT)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this,  "VPS found");
      vpsReceived_ = true;
    }

    if (!spsReceived_ && nalType == SPS_NUT)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this,  "SPS found");
      spsReceived_ = true;
    }

    if (!ppsReceived_ && nalType == PPS_NUT)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this,  "PPS found");
      ppsReceived_ = true;
    }

    bool vcl = nalType <= 31; // 31 is highest vlc nal_type

    if((vpsReceived_ && spsReceived_ && ppsReceived_) || !vcl)
    {
      int gotPicture = libOpenHevcDecode(handle_, input->data.get(),
                                         input->data_size, input->presentationTime);

      // only VCL frames result in output from decoder (at least I think so)
      if (vcl)
      {
        decodingFrames_.push_front(std::move(input));
      }

      if (gotPicture <= -1)
      {
        Logger::getLogger()->printError(this,  "Error while decoding!");
      }
      else if (gotPicture == 0)
      {
        /*
        if (vcl)
        {
          // IDR can print this often, which is too much
          Logger::getLogger()->printWarning(this,  "No output from decoder for VCL frame",
                                            "NAL Type", QString::number(nalType));
        }
        */
      }
      else
      {
        sendDecodedOutput(gotPicture);
      }
    }
    else
    {
      Logger::getLogger()->printWarning(this, "Discarding frame until necessary structures have arrived");
    }

    settingsMutex_.unlock();

    input = getInput();
  }

}


void OpenHEVCFilter::sendDecodedOutput(int& gotPicture)
{
  OpenHevc_Frame openHevcFrame;
  if ((gotPicture = libOpenHevcGetOutput(handle_, gotPicture, &openHevcFrame)) > 0)
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
    decodedFrame->vInfo->framerateNumerator = openHevcFrame.frameInfo.frameRate.num;
    decodedFrame->vInfo->framerateDenominator = openHevcFrame.frameInfo.frameRate.den;
    decodedFrame->data_size = finalDataSize;
    decodedFrame->data = std::move(yuv_frame);

    sendOutput(std::move(decodedFrame));
  }
}


