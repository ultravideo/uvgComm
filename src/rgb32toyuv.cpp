#include "rgb32toyuv.h"

#include <QtDebug>

#include <algorithm>    // std::min and std:max

RGB32toYUV::RGB32toYUV()
{
  name_ = "RGBtoYUVF";

}





// also flips input
void RGB32toYUV::process()
{
  qDebug() << "toYUVF: Converting input";
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    qDebug() << "toYUVF: Next";
    Data *yuv_data = new Data;
    yuv_data->data_size = input->width*input->height + input->width*input->height/2;
    yuv_data->data = std::unique_ptr<uchar[]>(new uchar[input->width*input->height + input->width*input->height/2]);
    yuv_data->width = input->width;
    yuv_data->height = input->height;

    uint8_t* Y = yuv_data->data.get();
    uint8_t* U = &(yuv_data->data[input->width*input->height]);
    uint8_t* V = &(yuv_data->data[input->width*input->height + input->width*input->height/4]);

    // Luma pixels
    for(unsigned int i = 0; i < input->data_size; i += 4)
    {
      int32_t ypixel = 66*input->data[i] + 129 * input->data[i+1]
          + 25 * input->data[i+2];
      Y[input->width*input->height - i/4] = (ypixel + 128) >> 8;
    }

    for(unsigned int i = 0; i < input->data_size - input->width*4; i += 2*input->width*4)
    {
      for(unsigned int j = i; j < i+input->width*4; j += 4*2)
      {
        int32_t upixel = -43 * input->data[j+2]                   - 84 * input->data[j+1]                   + 127 * input->data[j];
               upixel += -43 * input->data[j+2+4]                 - 84 * input->data[j+1+4]                 + 127 * input->data[j+4];
               upixel += -43 * input->data[j+2+input->width*4]    - 84  * input->data[j+1+input->width*4]   + 127 * input->data[j+input->width*4];
               upixel += -43 * input->data[j+2+4+input->width*4]  - 84  * input->data[j+1+4+input->width*4] + 127 * input->data[j+4+input->width*4];
        U[input->width*input->height/4 - (i/16 + (j-i)/8)] = ((upixel + 512) >> 10) + 128;


        int32_t vpixel =  127 * input->data[j+2]                  - 106 * input->data[j+1]                  - 21 * input->data[j];
               vpixel +=  127 * input->data[j+2+4]                - 106 * input->data[j+1+4]                - 21 * input->data[j+4];
               vpixel +=  127 * input->data[j+2+input->width*4]   - 106 * input->data[j+1+input->width*4]   - 21 * input->data[j+input->width*4];
               vpixel +=  127 * input->data[j+2+4+input->width*4] - 106 * input->data[j+1+4+input->width*4] - 21 * input->data[j+4+input->width*4];
        V[input->width*input->height/4 - (i/16 + (j-i)/8)] = ((vpixel + 512) >> 10) + 128;
      }
    }

    std::unique_ptr<Data> u_yuv_data( yuv_data );
    putOutput(std::move(u_yuv_data));

    input = getInput();
  }

  qDebug() << "toYUVF: Buffer empty";
}
