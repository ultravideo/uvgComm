#include "libyuvconverter.h"

#include "logger.h"
#include "common.h"
#include "settingskeys.h"


#include <libyuv.h>

LibYUVConverter::LibYUVConverter(QString id, StatisticsInterface* stats,
                                 std::shared_ptr<ResourceAllocator> hwResources,
                                 DataType input):
 Filter(id, "libyuv", stats, hwResources, input, DT_YUV420VIDEO),
  resolution_(1920, 1080),
  participants_(1)
{
  setConferenceSize(participants_);
}


void LibYUVConverter::setConferenceSize(uint32_t otherParticipants)
{
  QSettings settings(settingsFile, settingsFileFormat);
  QSize resolution = QSize(settings.value(SettingsKey::videoResolutionWidth).toInt(),
                           settings.value(SettingsKey::videoResolutionHeight).toInt());

  participants_ = otherParticipants;
  resolution_ = participantsToResolution(resolution, participants_);
}


void LibYUVConverter::updateSettings()
{
  setConferenceSize(participants_);
  Filter::updateSettings();
}


void LibYUVConverter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    if (input->vInfo->width != 1920 ||
        input->vInfo->height != 1080 ||
        input->type != inputType() ||
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
      // we reverse the coming scaling for the desired resolution to get how much we need to crop
      width_crop = input->vInfo->width - resolution_.width()/heightScaling;
    }

    size_t newWidth = input->vInfo->width - width_crop;

    // how much each region of UUV takes space
    size_t y_size = (newWidth)*input->vInfo->height;
    size_t uv_size = ((newWidth + 1)/2)*((input->vInfo->height + 1)/2);

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
                          width_crop/2, 0,
                          input->vInfo->width, input->vInfo->height,
                          newWidth, input->vInfo->height,
                          libyuv::kRotate0,
                          fourcc);

    // update the possible cropping to width
    input->vInfo->width = newWidth;

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
