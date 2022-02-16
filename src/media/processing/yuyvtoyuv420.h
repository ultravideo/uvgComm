#pragma once

#include "filter.h"

class YUYVtoYUV420 : public Filter
{
public:
  YUYVtoYUV420(QString id, StatisticsInterface *stats,
               std::shared_ptr<HWResourceManager> hwResources);

protected:
  void process();


private:

  void yuyv2yuv420_cpu(uint8_t* input, uint8_t* output,
                       uint16_t width, uint16_t height);
};
