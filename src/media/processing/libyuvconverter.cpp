#include "libyuvconverter.h"

#include "logger.h"
#include "common.h"


#include <libyuv.h>

LibYUVConverter::LibYUVConverter(QString id, StatisticsInterface* stats,
                                 std::shared_ptr<ResourceAllocator> hwResources,
                                 DataType input):
Filter(id, "libyuv", stats, hwResources, input, DT_YUV420VIDEO),
resolution_(1920, 1080)
{}


void LibYUVConverter::setConferenceSize(uint32_t otherParticipants)
{
  resolution_ = participantsToResolution(QSize(1920, 1080), otherParticipants);
}


void LibYUVConverter::updateSettings()
{
  Filter::updateSettings();
}


void LibYUVConverter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    if (input->vInfo->width != 1920 || input->vInfo->height != 1080)
    {
      Logger::getLogger()->printError(this, "Wrong resolution");
      sendOutput(std::move(input));
      input = getInput();
      continue;
    }

    uint32_t fourcc = getFourCC(inputType());

    if (fourcc == 0 || fourcc == libyuv::FOURCC_I420)
    {
      sendOutput(std::move(input));
      input = getInput();
      continue;
    }

    float heightScaling = float(resolution_.height())/float(input->vInfo->height);
    float widthScaling = float(resolution_.width())/float(input->vInfo->width);

    size_t width_crop = 0;

    if (widthScaling < heightScaling)
    {
      size_t cropped_width = resolution_.width()*heightScaling;
      width_crop = input->vInfo->width - cropped_width;
    }

    // how much each region of UUV takes space
    size_t y_size = (input->vInfo->width - width_crop)*input->vInfo->height;
    size_t color_size = (input->vInfo->width - width_crop)*input->vInfo->height/4;

    // reserve memory for converted YUV
    size_t finalDataSize = y_size + 2*color_size;
    std::unique_ptr<uchar[]> yuv_data(new uchar[finalDataSize]);

    // where each region begins
    uint8_t* y = yuv_data.get();
    uint8_t* u = yuv_data.get() + y_size;
    uint8_t* v = yuv_data.get() + y_size + color_size;

    int y_stride = input->vInfo->width - width_crop;
    int u_stride = (input->vInfo->width - width_crop + 1)/2;
    int v_stride = (input->vInfo->width - width_crop + 1)/2;

    // convert and possibly crop
    libyuv::ConvertToI420(input->data.get(), input->data_size,
                          y, y_stride,
                          u, u_stride,
                          v, v_stride,
                          width_crop/2, 0,
                          input->vInfo->width, input->vInfo->height,
                          input->vInfo->width - width_crop/2, input->vInfo->height,
                          libyuv::kRotate0, fourcc);

    // update the possible cropping to width
    input->vInfo->width = input->vInfo->width - width_crop;

    // downscale the YUV if needed
    if (heightScaling < 1)
    {
      // scaled resolution
      int scaledWidth = input->vInfo->width*heightScaling;
      int scaledHeight = input->vInfo->height*heightScaling;

      // size of scaled YUV
      size_t scaled_y_size = (scaledWidth)*scaledHeight;
      size_t scaled_color_size = (scaledWidth)*scaledHeight/4;

      // reserve memory for scaled YUV
      finalDataSize = scaled_y_size + 2*scaled_color_size;
      std::unique_ptr<uchar[]> scaled_yuv_data(new uchar[finalDataSize]);

      // get YUV regions
      uint8_t* sy = scaled_yuv_data.get();
      uint8_t* su = scaled_yuv_data.get() + scaled_y_size;
      uint8_t* sv = scaled_yuv_data.get() + scaled_y_size + scaled_color_size;

      int sy_stride = scaledWidth;
      int su_stride = (scaledWidth + 1)/2;
      int sv_stride = (scaledWidth + 1)/2;

      // scale the YUV
      libyuv::I420Scale(y, y_stride,
                        u, u_stride,
                        v, v_stride,
                        input->vInfo->width, input->vInfo->height,
                        sy, sy_stride,
                        su, su_stride,
                        sv, sv_stride,
                        scaledWidth, scaledHeight,
                        libyuv::kFilterNone);

      // use the scaled YUV when sending data forward
      yuv_data = std::move(scaled_yuv_data);
      input->vInfo->width = scaledWidth;
      input->vInfo->height = scaledHeight;
    }

    input->type = DT_YUV420VIDEO;
    input->data = std::move(yuv_data);
    input->data_size = finalDataSize;
    sendOutput(std::move(input));

    input = getInput();
  }
}


uint32_t LibYUVConverter::getFourCC(DataType type) const
{
  uint32_t fourcc = 0;

  switch(type)
  {
    case DT_YUV420VIDEO:
    {
      Logger::getLogger()->printProgramWarning(this, "Already in YUV 420 format");
      fourcc = libyuv::FOURCC_I420;
      break;
    }
    case DT_YUV422VIDEO:
    {
      fourcc = libyuv::FOURCC_I422;
      break;
    }
    case DT_NV12VIDEO:
    {
      fourcc = libyuv::FOURCC_NV12;
      break;
    }
    case DT_NV21VIDEO:
    {
      fourcc = libyuv::FOURCC_NV21;
      break;
    }
    case DT_YUYVVIDEO:
    {
      fourcc = libyuv::FOURCC_YUYV;
      break;
    }
    case DT_UYVYVIDEO:
    {
      fourcc = libyuv::FOURCC_UYVY;
      break;
    }
    case DT_ARGBVIDEO:
    {
      fourcc = libyuv::FOURCC_ARGB;
      break;
    }
    case DT_BGRAVIDEO:
    {
      fourcc = libyuv::FOURCC_BGRA;
      break;
    }
    case DT_ABGRVIDEO:
    {
      fourcc = libyuv::FOURCC_ABGR;
      break;
    }
    case DT_RGB32VIDEO:
    {
      fourcc = libyuv::FOURCC_RGBA;
      break;
    }
    case DT_RGB24VIDEO:
    {
      fourcc = 2; // copied from libyuv tests
      break;
    }
    case DT_BGRXVIDEO:
    {
      fourcc = libyuv::FOURCC_24BG;
      break;
    }
    case DT_MJPEGVIDEO:
    {
      fourcc = libyuv::FOURCC_MJPG;
      break;
    }
    default:
    {
      Logger::getLogger()->printWarning(this, "Unsupported conversion requested");
      fourcc = 0;
    }
  }

  return fourcc;
}
