#include "yuvtorgb32.h"

#include "optimized/yuv2rgb.h"

#include "common.h"

#include <QSettings>
#include <QDebug>

#include <algorithm>    // std::min and std:max

uint8_t clamp(int32_t input)
{
  if(input & ~255)
  {
    return (-input) >> 31;
  }
  return input;
}

YUVtoRGB32::YUVtoRGB32(QString id, StatisticsInterface *stats, uint32_t peer) :
  Filter(id, "YUVtoRGB32_" + QString::number(peer), stats, YUV420VIDEO, RGB32VIDEO),
  sse_(true),
  avx2_(true),
  threadCount_(0)
{
  updateSettings();
}

void YUVtoRGB32::updateSettings()
{
  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  if(settings.value("video/yuvThreads").isValid())
  {
    threadCount_ = settings.value("video/yuvThreads").toInt();

    qDebug() << "Settings for YUV to RGB32 threads:" << threadCount_;
  }
  else
  {
    printDebug(DEBUG_ERROR, "CameraInfo", DC_SETTINGS,
               "Missing settings value YUV threads.");
  }

  Filter::updateSettings();
}

// also flips input
void YUVtoRGB32::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    uint32_t finalDataSize = input->width*input->height*4;
    std::unique_ptr<uchar[]> rgb32_frame(new uchar[finalDataSize]);


    // TODO: Select thread count based on input resolution. Anything above fullhd should be around 2
    if(threadCount_ == 1 && input->width % 16 == 0)
    {
      yuv2rgb_i_avx2_single(input->data.get(), rgb32_frame.get(), input->width, input->height);
    }
    else if(avx2_ && input->width % 16 == 0)
    {
      yuv2rgb_i_avx2(input->data.get(), rgb32_frame.get(), input->width, input->height, threadCount_);
    }
    else if(sse_ && input->width % 16 == 0)
    {
      yuv2rgb_i_sse41(input->data.get(), rgb32_frame.get(), input->width, input->height);
    }
    else
    {
      // Luma pixels
      for(int i = 0; i < input->width*input->height; ++i)
      {
        rgb32_frame[i*4] = input->data[i];
        rgb32_frame[i*4+1] = input->data[i];
        rgb32_frame[i*4+2] = input->data[i];
      }

      uint32_t u_offset = input->width*input->height;
      uint32_t v_offset = input->width*input->height + input->height*input->width/4;

      for(int y = 0; y < input->height/2; ++y)
      {
        for(int x = 0; x < input->width/2; ++x)
        {
          int32_t cr = input->data[x + y*input->width/2 + u_offset] - 128;
          int32_t cb = input->data[x + y*input->width/2 + v_offset] - 128;

          int32_t rpixel = cr + (cr >> 2) + (cr >> 3) + (cr >> 5);
          int32_t gpixel = - ((cb >> 2) + (cb >> 4) + (cb >> 5)) - ((cr >> 1)+(cr >> 3)+(cr >> 4)+(cr >> 5));
          int32_t bpixel = cb + (cb >> 1)+(cb >> 2)+(cb >> 6);

          int32_t row = 8*y*input->width;
          int32_t next_row = row + 4*input->width;

          // add chroma components to rgb pixels
          // R
          int32_t pixel_value = 0;
          pixel_value = rgb32_frame[8*x + row ] + rpixel;
          rgb32_frame[8*x + row ] = clamp(pixel_value);

          pixel_value = rgb32_frame[8*x + 4 + row     ] + rpixel;
          rgb32_frame[8*x + 4 + row     ] = clamp(pixel_value);

          pixel_value = rgb32_frame[8*x +     next_row] + rpixel;
          rgb32_frame[8*x +     next_row] = clamp(pixel_value);

          pixel_value = rgb32_frame[8*x + 4 + next_row] + rpixel;
          rgb32_frame[8*x + 4 + next_row] = clamp(pixel_value);

          // G
          pixel_value = rgb32_frame[8*x +     row      + 1] + gpixel;
          rgb32_frame[8*x +     row      + 1] = clamp(pixel_value);

          pixel_value = rgb32_frame[8*x + 4 + row      + 1] + gpixel;
          rgb32_frame[8*x + 4 + row      + 1] = clamp(pixel_value);

          pixel_value = rgb32_frame[8*x +     next_row + 1] + gpixel;
          rgb32_frame[8*x +     next_row + 1] = clamp(pixel_value);

          pixel_value = rgb32_frame[8*x + 4 + next_row + 1] + gpixel;
          rgb32_frame[8*x + 4 + next_row + 1] = clamp(pixel_value);

          // B
          pixel_value = rgb32_frame[8*x +     row      + 2] + bpixel;
          rgb32_frame[8*x +     row      + 2] = clamp(pixel_value);

          pixel_value = rgb32_frame[8*x + 4 + row      + 2] + bpixel;
          rgb32_frame[8*x + 4 + row      + 2] = clamp(pixel_value);

          pixel_value = rgb32_frame[8*x +     next_row + 2] + bpixel;
          rgb32_frame[8*x +     next_row + 2] = clamp(pixel_value);

          pixel_value = rgb32_frame[8*x + 4 + next_row + 2] + bpixel;
          rgb32_frame[8*x + 4 + next_row + 2] = clamp(pixel_value);
        }
      }
    }
    input->type = RGB32VIDEO;
    input->data = std::move(rgb32_frame);
    input->data_size = finalDataSize;
    sendOutput(std::move(input));

    input = getInput();
  }
}
