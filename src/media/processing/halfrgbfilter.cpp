#include "halfrgbfilter.h"

#include "yuvconversions.h"



HalfRGBFilter::HalfRGBFilter(QString id, StatisticsInterface *stats,
                             std::shared_ptr<ResourceAllocator> hwResources):
  Filter(id, "HalfRGB", stats, hwResources, DT_RGB32VIDEO, DT_RGB32VIDEO)
{
  updateSettings();
}


void HalfRGBFilter::updateSettings()
{
  Filter::updateSettings();
}


void HalfRGBFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    if (input->vInfo->height >= 720)
    {
      uint32_t finalDataSize = input->data_size/4;
      std::unique_ptr<uchar[]> rgb_data(new uchar[finalDataSize]);

      half_rgb(input->data.get(), rgb_data.get(),
               input->vInfo->width, input->vInfo->height);

      input->data = std::move(rgb_data);
      input->data_size = finalDataSize;
      input->vInfo->width  = input->vInfo->width/2;
      input->vInfo->height = input->vInfo->height/2;
    }

    sendOutput(std::move(input));
  }
}
