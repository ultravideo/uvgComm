#include "openhevcfilter.h"

#include <QDebug>
#include <QSettings>

OpenHEVCFilter::OpenHEVCFilter(QString id, StatisticsInterface *stats):
  Filter(id, "OpenHEVC", stats, HEVCVIDEO, YUVVIDEO),
  handle_(),
  parameterSets_(false),
  waitFrames_(0),
  slices_(true)
{}

void OpenHEVCFilter::init()
{
  qDebug() << name_ << "iniating";
  handle_ = libOpenHevcInit(1, 1);

  libOpenHevcSetDebugMode(handle_, 0);
  if(libOpenHevcStartDecoder(handle_) == -1)
  {
    qCritical() << name_ << "ERROR: failed to start decoder.";
    return;
  }
  libOpenHevcSetTemporalLayer_id(handle_, 0);
  libOpenHevcSetActiveDecoders(handle_, 0);
  libOpenHevcSetViewLayers(handle_, 0);
  qDebug() << name_ << "initiation success. Version: " << libOpenHevcVersion(handle_);

  // This is because we don't know anything about the incoming stream
  maxBufferSize_ = -1; // no buffer limit
}

void OpenHEVCFilter::uninit()
{
  qDebug() << name_ << "uniniating.";
  libOpenHevcFlush(handle_);
  libOpenHevcClose(handle_);
}

void OpenHEVCFilter::run()
{
  setPriority(QThread::HighPriority);
  Filter::run();
}

std::unique_ptr<Data> OpenHEVCFilter::combineFrame()
{
  if(sliceBuffer_.size() == 0)
  {
    qDebug() << "No previous slices";
    return std::unique_ptr<Data>();
  }

  std::unique_ptr<Data> combinedFrame(shallowDataCopy(sliceBuffer_.at(0).get()));
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
    qDebug() << name_ << "Detected no slices in incoming stream.";
    uninit();
    init();
  }

  sliceBuffer_.clear();

  return std::move(combinedFrame);
}

void OpenHEVCFilter::process()
{
  std::unique_ptr<Data> input = getInput();
  while(input)
  {
    const unsigned char *buff = input->data.get();

    bool nextSlice = buff[0] == 0
        && buff[1] == 0
        && buff[2] == 0;

    if(!slices_ && buff[0] == 0
       && buff[1] == 0
       && buff[2] == 1)
    {
      slices_ = true;
      qDebug() << "Detected slices in incoming stream";
      uninit();
      init();
    }

    if(!parameterSets_ && (buff[4] >> 1) == 32)
    {
      parameterSets_ = true;
      qDebug() << name_ << ": Parameter set found after" << waitFrames_ << "frames";
    }

    if(parameterSets_)
    {
      if(nextSlice && sliceBuffer_.size() != 0)
      {
        std::unique_ptr<Data> frame = combineFrame();

        int64_t pts = frame->presentationTime.tv_sec*90000 + frame->presentationTime.tv_usec*90000/1000000;
        int gotPicture = libOpenHevcDecode(handle_, frame->data.get(), frame->data_size, pts);

        OpenHevc_Frame openHevcFrame;
        if( gotPicture == -1)
        {
          qCritical() << name_ << " error while decoding.";
        }
        else if(!gotPicture && frame->data_size >= 2)
        {
          const unsigned char *buff2 = frame->data.get();
          qWarning() << "Warning: Could not decode video frame. NAL type:" <<
                        buff2[0] << buff2[1] << buff2[2] << buff2[3] << (buff2[4] >> 1);
        }
        else if( libOpenHevcGetOutput(handle_, gotPicture, &openHevcFrame) == -1 )
        {
          qCritical() << name_ << " failed to get output.";
        }
        else
        {
          frame->width = openHevcFrame.frameInfo.nWidth;
          frame->height = openHevcFrame.frameInfo.nHeight;
          uint32_t finalDataSize = frame->width*frame->height + frame->width*frame->height/2;
          std::unique_ptr<uchar[]> yuv_frame(new uchar[finalDataSize]);

          uint8_t* pY = (uint8_t*)yuv_frame.get();
          uint8_t* pU = (uint8_t*)&(yuv_frame.get()[frame->width*frame->height]);
          uint8_t* pV = (uint8_t*)&(yuv_frame.get()[frame->width*frame->height + frame->width*frame->height/4]);

          uint32_t s_stride = openHevcFrame.frameInfo.nYPitch;
          uint32_t qs_stride = openHevcFrame.frameInfo.nUPitch/2;

          uint32_t d_stride = frame->width/2;
          uint32_t dd_stride = frame->width;

          for (int i=0; i<frame->height; i++) {
            memcpy(pY,  (uint8_t *) openHevcFrame.pvY + i*s_stride, dd_stride);
            pY += dd_stride;

            if (! (i%2) ) {
              memcpy(pU,  (uint8_t *) openHevcFrame.pvU + i*qs_stride, d_stride);
              pU += d_stride;

              memcpy(pV,  (uint8_t *) openHevcFrame.pvV + i*qs_stride, d_stride);
              pV += d_stride;
            }
          }
          frame->type = YUVVIDEO;
          frame->data_size = finalDataSize;
          frame->data = std::move(yuv_frame);

          sendOutput(std::move(frame));
        }
      }
      sliceBuffer_.push_back(std::move(input));
    }
    else
    {
      ++waitFrames_;
    }

    input = getInput();
  }
}
