#include "openhevcfilter.h"


#include <QDebug>



OpenHEVCFilter::OpenHEVCFilter()
{
  name_ = "OHEVCF";
  pts_ = 1;


}

void OpenHEVCFilter::init()
{
  qDebug() << name_ << " iniating";
  handle_ = libOpenHevcInit(1, 1);

  libOpenHevcSetDebugMode(handle_, 0);
  if(libOpenHevcStartDecoder(handle_) == -1)
  {
    qCritical() << name_ << " failed to start decoder.";
  }
  libOpenHevcSetTemporalLayer_id(handle_, 0);
  libOpenHevcSetActiveDecoders(handle_, 0);
  libOpenHevcSetViewLayers(handle_, 0);

  f = fopen("decoded.265", "ab+");
}


void OpenHEVCFilter::process()
{
  std::unique_ptr<Data> input = getInput();
  while(input)
  {

    OpenHevc_Frame openHevcFrame;
    const unsigned char *buff = input->data.get();

    int gotPicture = libOpenHevcDecode(handle_, buff, input->data_size, pts_);


    if( gotPicture == -1)
    {
      qCritical() << name_ << " error while decoding.";
    }
    else if(!gotPicture)
    {
      qDebug() << name_ << " could not decompress frame";
    }
    else if( libOpenHevcGetOutput(handle_, gotPicture, &openHevcFrame) == -1 )
    {
      qCritical() << name_ << " failed to get output.";
    }
    else
    {
      qDebug() << name_ << " decoded one frame";

      Data *yuv_frame = new Data;
      yuv_frame->data_size = input->width*input->height + input->width*input->height/2;
      yuv_frame->data = std::unique_ptr<uchar[]>(new uchar[yuv_frame->data_size]);
      yuv_frame->width = input->width;
      yuv_frame->height = input->height;
      yuv_frame->type = YUVVIDEO;

      uint8_t* pY = (uint8_t*)yuv_frame->data.get();
      uint8_t* pU = (uint8_t*)&(yuv_frame->data.get()[input->width*input->height]);
      uint8_t* pV = (uint8_t*)&(yuv_frame->data.get()[input->width*input->height + input->width*input->height/4]);

      uint32_t s_stride = openHevcFrame.frameInfo.nYPitch;
      uint32_t qs_stride = openHevcFrame.frameInfo.nUPitch/2;

      uint32_t d_stride = input->width/2;
      uint32_t dd_stride = input->width;

      for (uint32_t i=0; i<input->height; i++) {
        memcpy(pY,  (uint8_t *) openHevcFrame.pvY + i*s_stride, dd_stride);
        pY += dd_stride;

        if (! (i%2) ) {
          memcpy(pU,  (uint8_t *) openHevcFrame.pvU + i*qs_stride, d_stride);
          pU += d_stride;

          memcpy(pV,  (uint8_t *) openHevcFrame.pvV + i*qs_stride, d_stride);
          pV += d_stride;
        }
      }
      //fwrite(yuv_frame->data.get(), 1, yuv_frame->data_size, f);

      std::unique_ptr<Data> u_yuv_data( yuv_frame );
      sendOutput(std::move(u_yuv_data));
      ++pts_;

    }
/*


    memcpy(yuv_frame->data.get(),
           (openHevcFrame.pvY),
           input->width*input->height);

    memcpy(&(yuv_frame->data.get()[input->width*input->height]),
           (openHevcFrame.pvU),
           input->width*input->height/4);

    memcpy(&(yuv_frame->data.get()[input->width*input->height + input->width*input->height/4]),
           (openHevcFrame.pvV),
           input->width*input->height/4);
*/

    input = getInput();
  }

}
