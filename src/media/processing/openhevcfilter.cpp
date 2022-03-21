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
  slices_(true),
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

  setPriority(QThread::HighPriority);

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


void OpenHEVCFilter::combineFrame(std::unique_ptr<Data>& combinedFrame)
{
  if(sliceBuffer_.size() == 0)
  {
    Logger::getLogger()->printWarning(this, "No previous slices");
    return;
  }

  combinedFrame = std::unique_ptr<Data>(shallowDataCopy(sliceBuffer_.at(0).get()));
  combinedFrame->data_size = 0;

  for(unsigned int i = 0; i < sliceBuffer_.size(); ++i)
  {
    combinedFrame->data_size += sliceBuffer_.at(i)->data_size;
  }

  combinedFrame->data = std::unique_ptr<uchar[]>(new uchar[combinedFrame->data_size]);

  uint32_t dataWritten = 0;
  for(unsigned int i = 0; i < sliceBuffer_.size(); ++i)
  {
    memcpy(combinedFrame->data.get() + dataWritten,
           sliceBuffer_.at(i)->data.get(), sliceBuffer_.at(i)->data_size );
    dataWritten += sliceBuffer_.at(i)->data_size;
  }

  if(slices_ && sliceBuffer_.size() == 1)
  {
    slices_ = false;
    Logger::getLogger()->printPeerError(this, "Detected no slices in incoming stream.");
    uninit();
    init();
  }

  sliceBuffer_.clear();

  return;
}

void OpenHEVCFilter::process()
{
  std::unique_ptr<Data> input = getInput();
  while(input)
  {
    getStats()->addReceivePacket(sessionID_, "Video", input->data_size);

    const unsigned char *buff = input->data.get();

    bool nextSlice = buff[0] == 0
        && buff[1] == 0
        && buff[2] == 0;

    if(!slices_ && buff[0] == 0
       && buff[1] == 0
       && buff[2] == 1)
    {
      slices_ = true;
      Logger::getLogger()->printNormal(this, "Detected slices in incoming stream");
      uninit();
      init();
    }

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
      if(nextSlice && sliceBuffer_.size() != 0)
      {
        std::unique_ptr<Data> frame;
        combineFrame(frame);

        if (frame == nullptr)
        {
          break;
        }

        int gotPicture = libOpenHevcDecode(handle_, frame->data.get(), frame->data_size, frame->presentationTime);

        OpenHevc_Frame openHevcFrame;
        if( gotPicture == -1)
        {
          Logger::getLogger()->printDebug(DEBUG_ERROR, this,  "Error while decoding.");
        }
        else if( libOpenHevcGetOutput(handle_, gotPicture, &openHevcFrame) == -1 )
        {
          Logger::getLogger()->printDebug(DEBUG_ERROR, this,  "Failed to get output.");
        }
        else if(gotPicture)
        {
          libOpenHevcGetPictureInfo(handle_, &openHevcFrame.frameInfo);


          frame->vInfo->width = openHevcFrame.frameInfo.nWidth;
          frame->vInfo->height = openHevcFrame.frameInfo.nHeight;
          uint32_t finalDataSize = frame->vInfo->width*frame->vInfo->height +
              frame->vInfo->width*frame->vInfo->height/2;
          std::unique_ptr<uchar[]> yuv_frame(new uchar[finalDataSize]);

          uint8_t* pY = (uint8_t*)yuv_frame.get();
          uint8_t* pU = (uint8_t*)&(yuv_frame.get()[frame->vInfo->width*frame->vInfo->height]);
          uint8_t* pV = (uint8_t*)&(yuv_frame.get()[frame->vInfo->width*frame->vInfo->height +
              frame->vInfo->width*frame->vInfo->height/4]);

          uint32_t s_stride = openHevcFrame.frameInfo.nYPitch;
          uint32_t qs_stride = openHevcFrame.frameInfo.nUPitch/2;

          uint32_t d_stride = frame->vInfo->width/2;
          uint32_t dd_stride = frame->vInfo->width;

          for (int i=0; i<frame->vInfo->height; i++) {
            memcpy(pY,  (uint8_t *) openHevcFrame.pvY + i*s_stride, dd_stride);
            pY += dd_stride;

            if (! (i%2) ) {
              memcpy(pU,  (uint8_t *) openHevcFrame.pvU + i*qs_stride, d_stride);
              pU += d_stride;

              memcpy(pV,  (uint8_t *) openHevcFrame.pvV + i*qs_stride, d_stride);
              pV += d_stride;
            }
          }

          // TODO: put delay into deque, and set timestamp accordingly to get more accurate latency.

          frame->type = DT_YUV420VIDEO;
          if (openHevcFrame.frameInfo.frameRate.den != 0)
          {
            frame->vInfo->framerate = openHevcFrame.frameInfo.frameRate.num/openHevcFrame.frameInfo.frameRate.den;
          }
          else
          {
            frame->vInfo->framerate = openHevcFrame.frameInfo.frameRate.num;
          }
          frame->data_size = finalDataSize;
          frame->data = std::move(yuv_frame);

          sendOutput(std::move(frame));
        }
      }
      sliceBuffer_.push_back(std::move(input));
    }

    input = getInput();
  }
}
