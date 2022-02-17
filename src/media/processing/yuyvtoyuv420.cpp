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
  uint8_t* lumaY = output;
  uint8_t* chromaU = output + width*height;
  uint8_t* chromaV = output + width*height + width*height/4;

  // Luma values
  for(int i = 0; i < width*height; i += 2)
  {
    lumaY[i]     = input[i*2];
    lumaY[i + 1] = input[i*2 + 2];
  }

  /* Chroma values
   * In YUYV, the chroma is sampled half for the horizontal and fully for the vertical direction
   * for YUV 420, both chroma directions and sampled half
   *
   * In this loop, the input is processed two rows at a time because the output is only
   * half sampled vertically and the input is fully sampled.
   */
  for (int i = 0; i < height/2; ++i)
  {
    // In YUYV, all the luma and chroma values are mixed in YUYV order (YUYV YUYV YUYV etc.)
    // It takes two bytes to represent one pixel in YUYV so we go until 2 times width
    for (int j = 0; j < width*2; j += 4)
    {
      // we take the average of both rows to use all available information
      chromaU[i*width/2 + j/4] = input[i*width*4 + j + 1]/2 + input[i*width*4 + width*2 + j + 1]/2;
      chromaV[i*width/2 + j/4] = input[i*width*4 + j + 3]/2 + input[i*width*4 + width*2 + j + 3]/2;
    }
  }

}
