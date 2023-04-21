#include "libyuvconverter.h"

#include "logger.h"

#include <libyuv.h>

LibYUVConverter::LibYUVConverter(QString id, StatisticsInterface* stats,
                                 std::shared_ptr<ResourceAllocator> hwResources,
                                 DataType input):
Filter(id, "libyuv", stats, hwResources, input, DT_YUV420VIDEO)
{}


void LibYUVConverter::updateSettings()
{
  Filter::updateSettings();
}


void LibYUVConverter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    uint32_t fourcc = 0;

    switch(inputType())
    {
        case DT_YUV420VIDEO:
        {
        Logger::getLogger()->printWarning(this, "Conversion without need for one");
        sendOutput(std::move(input));
        input = getInput();
        continue;
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
          fourcc = libyuv::FOURCC_NV21;
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
          sendOutput(std::move(input));
          input = getInput();
          continue;
        }
    }

    size_t y_size = input->vInfo->width*input->vInfo->height;
    size_t color_size = input->vInfo->width*input->vInfo->height/4;

    size_t finalDataSize = y_size + 2*color_size;
    std::unique_ptr<uchar[]> yuv_data(new uchar[finalDataSize]);

    uint8_t* y = yuv_data.get();
    uint8_t* u = yuv_data.get() + y_size;
    uint8_t* v = yuv_data.get() + y_size + color_size;

    int y_stride = input->vInfo->width;
    int u_stride = (input->vInfo->width + 1)/2;
    int v_stride = (input->vInfo->width + 1)/2;

    libyuv::ConvertToI420(input->data.get(), input->data_size,
                          y, y_stride,
                          u, u_stride,
                          v, v_stride,
                          0, 0,
                          input->vInfo->width, input->vInfo->height,
                          input->vInfo->width, input->vInfo->height,
                          libyuv::kRotate0, fourcc);

    input->type = DT_YUV420VIDEO;
    input->data = std::move(yuv_data);
    input->data_size = finalDataSize;
    sendOutput(std::move(input));

    input = getInput();
  }
}
