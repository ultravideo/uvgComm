#include "rgb32toyuv.h"

#include <QtDebug>

#include <algorithm>    // std::min and std:max

RGB32toYUV::RGB32toYUV()
{

}





//calculation can be done using integers
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
      double ypixel = 0.299*input->data[i] + 0.587 * input->data[i+1]
          + 0.114 * input->data[i+2];
      Y[i/4] = std::max(0.0,std::min(255.0, ypixel + 0.5)); // +0.5 is for rounding up.
    }

    for(unsigned int i = 0; i < input->data_size - input->width*4; i += 2*input->width*4)
    {
      for(unsigned int j = i; j < i+input->width*4; j += 4*2)
      {
        double upixel =  -0.147 * input->data[j+2]                  - 0.289 * input->data[j+1]                  + 0.436 * input->data[j];
               upixel += -0.147 * input->data[j+2+4]                - 0.289 * input->data[j+1+4]                + 0.436 * input->data[j+4];
               upixel += -0.147 * input->data[j+2+input->width*4]   - 0.289 * input->data[j+1+input->width*4]   + 0.436 * input->data[j+input->width*4];
               upixel += -0.147 * input->data[j+2+4+input->width*4] - 0.289 * input->data[j+1+4+input->width*4] + 0.436 * input->data[j+4+input->width*4];
        U[i/16 + (j-i)/8] = std::max(0.0,std::min(255.0, (upixel+4*128)/4 + 0.5)); // +0.5 is for rounding up


        double vpixel =   0.615 * input->data[j+2]                  - 0.515 * input->data[j+1]                  - 0.100 * input->data[j];
               vpixel +=  0.615 * input->data[j+2+4]                - 0.515 * input->data[j+1+4]                - 0.100 * input->data[j+4];
               vpixel +=  0.615 * input->data[j+2+input->width*4]   - 0.515 * input->data[j+1+input->width*4]   - 0.100 * input->data[j+input->width*4];
               vpixel +=  0.615 * input->data[j+2+4+input->width*4] - 0.515 * input->data[j+1+4+input->width*4] - 0.100 * input->data[j+4+input->width*4];
        V[i/16 + (j-i)/8] = std::max(0.0,std::min(255.0, (vpixel+4*128)/4 + 0.5)); // +0.5 is for rounding up.
      }
    }

    std::unique_ptr<Data> u_yuv_data( yuv_data );
    putOutput(std::move(u_yuv_data));

    input = getInput();
  }
}
