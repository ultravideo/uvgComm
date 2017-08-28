#include "rgb32toyuv.h"

#include "optimized/rgb2yuv.h"

#include <QtDebug>

RGB32toYUV::RGB32toYUV(QString id, StatisticsInterface *stats) :
  Filter(id, "RGB32toYUV", stats, RGB32VIDEO, YUVVIDEO),
  sse_(true)
{}

void RGB32toYUV::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    uint32_t finalDataSize = input->width*input->height + input->width*input->height/2;
    std::unique_ptr<uchar[]> yuv_data(new uchar[finalDataSize]);

    if(sse_ && input->width % 4 == 0)
    {
      rgb2yuv_i_sse41(input->data.get(), yuv_data.get(), input->width, input->height);
    }
    else
    {
      uint8_t* Y = yuv_data.get();
      uint8_t* U = &(yuv_data[input->width*input->height]);
      uint8_t* V = &(yuv_data[input->width*input->height + input->width*input->height/4]);

      // Luma pixels
      for(unsigned int i = 0; i < input->data_size; i += 4)
      {
        int32_t ypixel = 76*input->data[i] + 150 * input->data[i+1]
            + 29 * input->data[i+2];
        Y[input->width*input->height - i/4 - 1] = (ypixel + 128) >> 8;
      }

      for(unsigned int i = 0; i < input->data_size - input->width*4; i += 2*input->width*4)
      {
        for(unsigned int j = i; j < i+input->width*4; j += 4*2)
        {
          int32_t upixel = -43 * input->data[j+2]                   - 84 * input->data[j+1]                   + 127 * input->data[j];
          upixel += -43 * input->data[j+2+4]                 - 84 * input->data[j+1+4]                 + 127 * input->data[j+4];
          upixel += -43 * input->data[j+2+input->width*4]    - 84  * input->data[j+1+input->width*4]   + 127 * input->data[j+input->width*4];
          upixel += -43 * input->data[j+2+4+input->width*4]  - 84  * input->data[j+1+4+input->width*4] + 127 * input->data[j+4+input->width*4];
          U[input->width*input->height/4 - (i/16 + (j-i)/8) - 1] = ((upixel + 512) >> 10) + 128;


          int32_t vpixel =  127 * input->data[j+2]                  - 106 * input->data[j+1]                  - 21 * input->data[j];
          vpixel +=  127 * input->data[j+2+4]                - 106 * input->data[j+1+4]                - 21 * input->data[j+4];
          vpixel +=  127 * input->data[j+2+input->width*4]   - 106 * input->data[j+1+input->width*4]   - 21 * input->data[j+input->width*4];
          vpixel +=  127 * input->data[j+2+4+input->width*4] - 106 * input->data[j+1+4+input->width*4] - 21 * input->data[j+4+input->width*4];
          V[input->width*input->height/4 - (i/16 + (j-i)/8) - 1] = ((vpixel + 512) >> 10) + 128;
        }
      }
    }

    input->type = YUVVIDEO;
    input->data = std::move(yuv_data);
    input->data_size = finalDataSize;
    sendOutput(std::move(input));

    input = getInput();
  }
}
