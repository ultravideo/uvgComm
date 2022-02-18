#include "yuyvtorgb32.h"

#include "yuvconversions.h"

YUYVtoRGB32::YUYVtoRGB32(QString id, StatisticsInterface *stats,
                           std::shared_ptr<HWResourceManager> hwResources):
      Filter(id, "YUYVtoRGB32", stats, hwResources, YUYVVIDEO, RGB32VIDEO)
{}


void YUYVtoRGB32::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    uint32_t finalDataSize = input->width*input->height*4;
    std::unique_ptr<uchar[]> rgb32_frame(new uchar[finalDataSize]);

    yuyv_to_rgb_c(input->data.get(), rgb32_frame.get(),
                  input->width, input->height);

    input->type = RGB32VIDEO;
    input->data = std::move(rgb32_frame);
    input->data_size = finalDataSize;
    sendOutput(std::move(input));

    input = getInput();
  }
}
