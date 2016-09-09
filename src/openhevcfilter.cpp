#include "openhevcfilter.h"


#include <QDebug>



OpenHEVCFilter::OpenHEVCFilter(StatisticsInterface *stats):
  Filter("OpenHEVC", stats, true, true),
  handle_()
{}

void OpenHEVCFilter::init()
{
  qDebug() << name_ << "iniating";
  handle_ = libOpenHevcInit(1, 1);

  libOpenHevcSetDebugMode(handle_, 0);
  if(libOpenHevcStartDecoder(handle_) == -1)
  {
    qCritical() << name_ << "failed to start decoder.";
    return;
  }
  libOpenHevcSetTemporalLayer_id(handle_, 0);
  libOpenHevcSetActiveDecoders(handle_, 0);
  libOpenHevcSetViewLayers(handle_, 0);
  qDebug() << name_ << "initiation success. Version: " << libOpenHevcVersion(handle_);
}


void OpenHEVCFilter::process()
{
  std::unique_ptr<Data> input = getInput();
  while(input)
  {

    OpenHevc_Frame openHevcFrame;
    const unsigned char *buff = input->data.get();

    int64_t pts = input->presentationTime.tv_sec*90000 + input->presentationTime.tv_usec*90000/1000000;
    int gotPicture = libOpenHevcDecode(handle_, buff, input->data_size, pts);

    if( gotPicture == -1)
    {
      qCritical() << name_ << " error while decoding.";
    }
    else if(!gotPicture)
    {
      qDebug() << name_ << " could not decode frame. NAL type:" <<
       buff[0] << buff[1] << buff[2] << buff[3] << (buff[4] >> 1);
    }
    else if( libOpenHevcGetOutput(handle_, gotPicture, &openHevcFrame) == -1 )
    {
      qCritical() << name_ << " failed to get output.";
    }
    else
    {
      input->width = openHevcFrame.frameInfo.nWidth;
      input->height = openHevcFrame.frameInfo.nHeight;
      uint32_t finalDataSize = input->width*input->height + input->width*input->height/2;
      std::unique_ptr<uchar[]> yuv_frame(new uchar[finalDataSize]);

      uint8_t* pY = (uint8_t*)yuv_frame.get();
      uint8_t* pU = (uint8_t*)&(yuv_frame.get()[input->width*input->height]);
      uint8_t* pV = (uint8_t*)&(yuv_frame.get()[input->width*input->height + input->width*input->height/4]);

      uint32_t s_stride = openHevcFrame.frameInfo.nYPitch;
      uint32_t qs_stride = openHevcFrame.frameInfo.nUPitch/2;

      uint32_t d_stride = input->width/2;
      uint32_t dd_stride = input->width;

      for (int i=0; i<input->height; i++) {
        memcpy(pY,  (uint8_t *) openHevcFrame.pvY + i*s_stride, dd_stride);
        pY += dd_stride;

        if (! (i%2) ) {
          memcpy(pU,  (uint8_t *) openHevcFrame.pvU + i*qs_stride, d_stride);
          pU += d_stride;

          memcpy(pV,  (uint8_t *) openHevcFrame.pvV + i*qs_stride, d_stride);
          pV += d_stride;
        }
      }

      input->type = YUVVIDEO;
      input->data_size = finalDataSize;
      input->data = std::move(yuv_frame);

      sendOutput(std::move(input));
    }

    input = getInput();
  }
}
