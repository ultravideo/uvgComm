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
      /*
    if (input->vInfo->width != 1920 || input->vInfo->height != 1080)
    {
      Logger::getLogger()->printError(this, "Wrong resolution");
      sendOutput(std::move(input));
      input = getInput();
      continue;
    }
*/

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

    size_t y_size = (input->vInfo->width - width_crop)*input->vInfo->height;
    size_t color_size = (input->vInfo->width - width_crop)*input->vInfo->height/4;

    size_t finalDataSize = y_size + 2*color_size;
    std::unique_ptr<uchar[]> yuv_data(new uchar[finalDataSize]);

    uint8_t* y = yuv_data.get();
    uint8_t* u = yuv_data.get() + y_size;
    uint8_t* v = yuv_data.get() + y_size + color_size;

    int y_stride = input->vInfo->width - width_crop;
    int u_stride = (input->vInfo->width - width_crop + 1)/2;
    int v_stride = (input->vInfo->width - width_crop + 1)/2;

    libyuv::ConvertToI420(input->data.get(), input->data_size,
                          y, y_stride,
                          u, u_stride,
                          v, v_stride,
                          width_crop/2, 0,
                          input->vInfo->width, input->vInfo->height,
                          input->vInfo->width - width_crop/2, input->vInfo->height,
                          libyuv::kRotate0, fourcc);

    input->vInfo->width = input->vInfo->width - width_crop;

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
