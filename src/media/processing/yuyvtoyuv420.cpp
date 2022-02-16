#include "yuyvtoyuv420.h"

YUYVtoYUV420::YUYVtoYUV420(QString id, StatisticsInterface *stats,
                           std::shared_ptr<HWResourceManager> hwResources) :
      Filter(id, "YUYVtoYUV420", stats, hwResources, YUYVVIDEO, YUV420VIDEO)
{}


void YUYVtoYUV420::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    uint32_t finalDataSize = input->width*input->height + input->width*input->height/2;
    std::unique_ptr<uchar[]> yuyv_frame(new uchar[finalDataSize]);

    yuyv2yuv420_cpu(input->data.get(), yuyv_frame.get(), input->width, input->height);

    input->type = YUYVVIDEO;
    input->data = std::move(yuyv_frame);
    input->data_size = finalDataSize;
    sendOutput(std::move(input));

    input = getInput();
  }
}


void YUYVtoYUV420::yuyv2yuv420_cpu(uint8_t* input, uint8_t* output,
                                   uint16_t width, uint16_t height)
{
  // Luma pixels
  for(int i = 0; i < width*height; i += 2)
  {
    output[i] = input[i*2];
    output[i + 1] = input[i*2 + 2];
  }
}
