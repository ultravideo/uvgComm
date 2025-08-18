#include "libyuvconverter.h"

#include "common.h"
#include "logger.h"
#include "settingskeys.h"
#include "src/media/resourceallocator.h"


#include <libyuv.h>

LibYUVConverter::LibYUVConverter(QString id, StatisticsInterface* stats,
                                 std::shared_ptr<ResourceAllocator> hwResources,
                                 DataType input):
 Filter(id, "libyuv", stats, hwResources, input, DT_YUV420VIDEO),
  targetResolution_(0, 0),
  baseResolution_(0, 0)
{
  QSettings settings(getSettingsFile(), settingsFileFormat);
  baseResolution_ = QSize(settings.value(SettingsKey::videoResolutionWidth).toInt(),
                         settings.value(SettingsKey::videoResolutionHeight).toInt());

  // also sets target resolution
  changeResolution();
}



void LibYUVConverter::changeResolution()
{
  QSettings settings(getSettingsFile(), settingsFileFormat);

  resolutionMutex_.lock();

  QSize resolution = getHWManager()->getVideoResolution();

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Changing resolution for libyuv filter",
                                  {"Resolution"}, {QString::number(resolution.width()) + "x" + QString::number(resolution.height())});

  if (resolution.width() == 0 ||
      resolution.height() == 0)
  {
    Logger::getLogger()->printError(this, "Invalid libyuv filter resolution");
  }
  else
  {
    targetResolution_ = resolution;
  }

  resolutionMutex_.unlock();
}


void LibYUVConverter::updateSettings()
{
  QSettings settings(getSettingsFile(), settingsFileFormat);
  baseResolution_ = QSize(settings.value(SettingsKey::videoResolutionWidth).toInt(),
                          settings.value(SettingsKey::videoResolutionHeight).toInt());

  changeResolution();
  Filter::updateSettings();
}


void LibYUVConverter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    if (input->type != inputType() ||
        input->data_size == 0 ||
        input->data == nullptr ||
        input->data.get() == nullptr ||
        input->vInfo->width == 0 ||
        input->vInfo->height == 0)
    {
      Logger::getLogger()->printError(this, "Wrong resolution");
      sendOutput(std::move(input));
      input = getInput();
      continue;
    }

    if (targetResolution_.width() <= 0 ||
        targetResolution_.height() <= 0)
    {
      Logger::getLogger()->printError(this, "Invalid target resolution for libyuv filter");
      sendOutput(std::move(input));
      input = getInput();
      continue;
    }

    // needed by libyuv
    uint32_t fourcc = getFourCC(inputType());

    if (fourcc == 0)
    {
      Logger::getLogger()->printError(this, "Unsupported conversion requested");
      sendOutput(std::move(input));
      input = getInput();
      continue;
    }

    resolutionMutex_.lock();
    float heightScaling = float(targetResolution_.height())/float(input->vInfo->height);
    float widthScaling = float(targetResolution_.width())/float(input->vInfo->width);

    size_t width_crop = 0;
    size_t height_crop = 0;

    if (widthScaling < heightScaling)
    {
      // we reverse the coming scaling for the desired resolution to get how much we need to crop
      width_crop = input->vInfo->width - targetResolution_.width()/heightScaling;
    }
    else if (heightScaling < widthScaling)
    {
      // we reverse the coming scaling for the desired resolution to get how much we need to crop
      height_crop = input->vInfo->height - targetResolution_.height()/widthScaling;
    }
    resolutionMutex_.unlock();

    size_t newWidth = input->vInfo->width - width_crop;
    size_t newHeight = input->vInfo->height - height_crop;

    // how much each region of UUV takes space
    size_t y_size = newWidth*newHeight;
    size_t uv_size = ((newWidth + 1)/2)*((newHeight + 1)/2);

    // reserve memory for converted YUV
    size_t finalDataSize = y_size + 2*uv_size;
    std::unique_ptr<uchar[]> yuv_data(new uchar[finalDataSize]);

    int dst_y_stride = newWidth;
    int dst_u_stride = (newWidth + 1)/2; // +1 is for rounding up
    int dst_v_stride = (newWidth + 1)/2;

    // where each region begins
    uint8_t* dst_y = yuv_data.get();
    uint8_t* dst_u = yuv_data.get() + y_size;
    uint8_t* dst_v = yuv_data.get() + y_size + uv_size;

    // convert and possibly crop
    libyuv::ConvertToI420(input->data.get(), input->data_size,
                          dst_y, dst_y_stride,
                          dst_u, dst_u_stride,
                          dst_v, dst_v_stride,
                          width_crop/2, height_crop/2,
                          input->vInfo->width, input->vInfo->height,
                          newWidth, newHeight,
                          libyuv::kRotate0,
                          fourcc);

    bool needScaling = input->vInfo->width > targetResolution_.width() && input->vInfo->height > targetResolution_.height();

    // update the possible cropping to width
    input->vInfo->width = newWidth;
    input->vInfo->height = newHeight;

    // downscale the YUV if needed
    if (needScaling)
    {
      // scaled resolution
      int scaledWidth = std::round(input->vInfo->width*heightScaling);
      int scaledHeight = std::round(input->vInfo->height*heightScaling);

      if (width_crop == 0)
      {
        scaledWidth = std::round(input->vInfo->width*widthScaling);
      }

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

      libyuv::FilterMode mode = libyuv::kFilterBilinear;

      // the small resolutions can use the best filtering as it does not cost much
      // and looks much better
      if (scaledHeight <= 270)
      {
        mode = libyuv::kFilterBox;
      }

      // scale the YUV
      libyuv::I420Scale(dst_y, dst_y_stride,
                        dst_u, dst_u_stride,
                        dst_v, dst_v_stride,
                        input->vInfo->width, input->vInfo->height,
                        sy, sy_stride,
                        su, su_stride,
                        sv, sv_stride,
                        scaledWidth, scaledHeight,
                        mode);

      // use the scaled YUV when sending data forward
      yuv_data = std::move(scaled_yuv_data);
      input->vInfo->width = scaledWidth;
      input->vInfo->height = scaledHeight;
    }

    if(input->vInfo->width != targetResolution_.width() || input->vInfo->height != targetResolution_.height())
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Incorrect resolution conversion",
                                      {"Expected resolutions", "Converted resolution"},
                                       {QString::number(targetResolution_.width()) + "x" + QString::number(targetResolution_.height()),
                                        QString::number(input->vInfo->width) + "x" + QString::number(input->vInfo->height)});
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
