#include "yuvtorgb32.h"


#include <QDebug>


YUVtoRGB32::YUVtoRGB32()
{
  name_ = "toRGB32";
}




// also flips input
void YUVtoRGB32::process()
{
  qDebug() << name_ << ": Converting input";
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    qDebug() << name_ << ": Next";
    Data *rgb32_frame = new Data;
    rgb32_frame->data_size = input->width*input->height*4;
    rgb32_frame->data = std::unique_ptr<uchar[]>(new uchar[rgb32_frame->data_size]);
    rgb32_frame->width = input->width;
    rgb32_frame->height = input->height;


    // Luma pixels
    for(unsigned int i = 0; i < input->width*input->height; ++i)
    {
      rgb32_frame->data.get()[i*4] = input->data[i];
      rgb32_frame->data.get()[i*4+1] = input->data[i];
      rgb32_frame->data.get()[i*4+2] = input->data[i];

    }

    uint32_t u_offset = input->width*input->height;
    uint32_t v_offset = input->width*input->height + input->height*input->width/4;

    for(unsigned int y = 0; y < input->height/2; ++y)
    {
      for(unsigned int x = 0; x < input->width/2; ++x)
      {
        int32_t cb = input->data[x + y*input->width/2 + u_offset] - 128;
        int32_t cr = input->data[x + y*input->width/2 + v_offset] - 128;

        int32_t rpixel = cr + (cr >> 2) + (cr >> 3) + (cr >> 5);
        int32_t gpixel = - ((cb >> 2) + (cb >> 4) + (cb >> 5)) - ((cr >> 1)+(cr >> 3)+(cr >> 4)+(cr >> 5));
        int32_t bpixel = cb + (cb >> 1)+(cb >> 2)+(cb >> 6);

        rgb32_frame->data.get()[(8*x +     8*y*input->width)                  ] += rpixel;
        rgb32_frame->data.get()[(8*x + 4 + 8*y*input->width)                  ] += rpixel;
        rgb32_frame->data.get()[(8*x +     8*y*input->width)  + 4*input->width] += rpixel;
        rgb32_frame->data.get()[(8*x + 4 + 8*y*input->width)  + 4*input->width] += rpixel;

        rgb32_frame->data.get()[(8*x +     8*y*input->width)                   + 1] += gpixel;
        rgb32_frame->data.get()[(8*x + 4 + 8*y*input->width)                   + 1] += gpixel;
        rgb32_frame->data.get()[(8*x +     8*y*input->width)  + 4*input->width + 1] += gpixel;
        rgb32_frame->data.get()[(8*x + 4 + 8*y*input->width)  + 4*input->width + 1] += gpixel;

        rgb32_frame->data.get()[(8*x +     8*y*input->width)                   + 2] += bpixel;
        rgb32_frame->data.get()[(8*x + 4 + 8*y*input->width)                   + 2] += bpixel;
        rgb32_frame->data.get()[(8*x +     8*y*input->width)  + 4*input->width + 2] += bpixel;
        rgb32_frame->data.get()[(8*x + 4 + 8*y*input->width)  + 4*input->width + 2] += bpixel;
      }
    }

    rgb32_frame->type = RGB32VIDEO;
    std::unique_ptr<Data> u_rgb_data( rgb32_frame );
    putOutput(std::move(u_rgb_data));

    input = getInput();
  }

  qDebug() << name_ << ": Buffer empty";
}
