#include "openhevcfilter.h"

#include "common.h"
#include "statisticsinterface.h"

#include "settingskeys.h"
#include "logger.h"

#include <QSettings>
#include <qdatetime.h>

enum OHThreadType {OH_THREAD_FRAME  = 1, OH_THREAD_SLICE = 2, OH_THREAD_FRAMESLICE  = 3};

OpenHEVCFilter::OpenHEVCFilter(uint32_t sessionID,
                               QString cname,
                               StatisticsInterface *stats,
                               std::shared_ptr<ResourceAllocator> hwResources)
    : Filter(QString::number(sessionID), "OpenHEVC", stats, hwResources, DT_HEVCVIDEO, DT_YUV420VIDEO)
    , handle_()
    , vpsReceived_(false)
    , spsReceived_(false)
    , ppsReceived_(false)
    , sessionID_(sessionID)
    , cname_(cname)
    , threads_(-1)
    , parallelizationMode_("Slice")
  , discardedFrames_(0)
  , pendingParamSetBytes_(0)
{}


bool OpenHEVCFilter::init()
{
  Logger::getLogger()->printNormal(this, "Starting to initiate OpenHEVC");
  QSettings settings(getSettingsFile(), settingsFileFormat);

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
    Logger::getLogger()->printProgramError(this, "Failed to start decoder.");
    return false;
  }
  libOpenHevcSetTemporalLayer_id(handle_, 0);
  libOpenHevcSetActiveDecoders(handle_, 0);
  libOpenHevcSetViewLayers(handle_, 0);

  decodingFrames_.clear();

  // libOpenHevcSetDebugMode(handle_, OHEVC_LOG_DEBUG);

  Logger::getLogger()->printNormal(this, "OpenHEVC initiation successful.",
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
  QSettings settings(getSettingsFile(), settingsFileFormat);

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
    getStats()->addReceivePacket(sessionID_, cname_, "Video", input->data_size);
    settingsMutex_.lock();

    const unsigned char *buff = input->data.get();

    uint8_t nalType = (buff[4] >> 1);

    if (!vpsReceived_ && nalType == VPS_NUT)
    {
      Logger::getLogger()->printNormal(this,  "VPS found");
      vpsReceived_ = true;
    }

    if (!spsReceived_ && nalType == SPS_NUT)
    {
      Logger::getLogger()->printNormal(this,  "SPS found");
      spsReceived_ = true;
    }

    if (!ppsReceived_ && nalType == PPS_NUT)
    {
      Logger::getLogger()->printNormal(this,  "PPS found");
      ppsReceived_ = true;
    }

    bool vcl = nalType <= 31; // 31 is highest vlc nal_type
    // HEVC SEI NAL unit types are typically 39 (prefix SEI) and 40 (suffix SEI).
    bool sei = (nalType == 39 || nalType == 40);

    if (!vcl && !sei)
    {
      // needed for accurate frame sizes
      pendingParamSetBytes_ += input->data_size;
    }

    //Logger::getLogger()->printNormal(this, cname_ + " RTP timestamp: " + QString::number(input->rtpTimestamp) + ", NAL type: " + QString::number(nalType));

    if((vpsReceived_ && spsReceived_ && ppsReceived_) || !vcl)
    {
      if (discardedFrames_ != 0)
      {
        Logger::getLogger()->printNormal(this, "Starting to decode HEVC video after discarding frames",
                                         "Discarded frames", QString::number(discardedFrames_));
        discardedFrames_ = 0;
      }

      int gotPicture = libOpenHevcDecode(handle_, input->data.get(),
                                         input->data_size, input->presentationTimestamp);

      // only VCL frames result in output from decoder (at least I think so)
      if (vcl)
      {
        // Push a pair of (Data, extraBytes) so the extra non-VCL bytes are
        // associated with this queued frame without modifying Data.
        decodingFrames_.push_front(std::make_pair(std::move(input), pendingParamSetBytes_));
        pendingParamSetBytes_ = 0;
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
      if (discardedFrames_ == 0)
      {
        Logger::getLogger()->printWarning(this, "Discarding frames until necessary structures have arrived");
      }

      ++discardedFrames_;
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
    // Pop the queued pair (Data, extraBytes) and separate them.
    auto queued = std::move(decodingFrames_.back());
    std::unique_ptr<Data> decodedFrame = std::move(queued.first);
    uint32_t extraBytesForStats = queued.second;
    decodingFrames_.pop_back();
    libOpenHevcGetPictureInfo(handle_, &openHevcFrame.frameInfo);

    // we take the size from compressed frame because that is more interesting.
    auto now = std::chrono::system_clock::now();
    int64_t since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    int64_t decoding_delay = since_epoch - decodedFrame->presentationTimestamp;

    // Report end-to-end video latency as early as possible (after decode) for reliability.
    // Mirrors the previous semantics from the render path:
    // - key by presentationTimestamp
    // - delay computed from creationTimestamp when sender-provided timing exists
    if (sessionID_ != 0 && decodedFrame->creationTimestamp > 0)
    {
      int64_t endToEndDelay = since_epoch - decodedFrame->creationTimestamp;
      getStats()->videoLatency(sessionID_, cname_, decodedFrame->presentationTimestamp, endToEndDelay);
    }

    uint32_t reportedCompressedSize = decodedFrame->data_size + extraBytesForStats;
    getStats()->decodedVideoFrame(cname_, since_epoch, reportedCompressedSize, decoding_delay,
                                  QSize(openHevcFrame.frameInfo.nWidth, openHevcFrame.frameInfo.nHeight));

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


