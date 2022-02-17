#include "yuyvtoyuv420.h"

#include "yuvconversions.h"

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
    std::unique_ptr<uchar[]> yuv420_frame(new uchar[finalDataSize]);

    yuyv_to_yuv420_c(input->data.get(), yuv420_frame.get(), input->width, input->height);

    input->type = YUV420VIDEO;
    input->data = std::move(yuv420_frame);
    input->data_size = finalDataSize;
    sendOutput(std::move(input));

    input = getInput();
  }
}
