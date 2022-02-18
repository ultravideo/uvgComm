#include "rgb32toyuv.h"

#include "yuvconversions.h"

#include "media/hwresourcemanager.h"

#include "settingskeys.h"
#include "common.h"

#include <QSettings>

RGB32toYUV::RGB32toYUV(QString id, StatisticsInterface *stats,
                       std::shared_ptr<HWResourceManager> hwResources):
  Filter(id, "RGB32toYUV", stats, hwResources, RGB32VIDEO, YUV420VIDEO),
  threadCount_(0)
{
  updateSettings();
}


void RGB32toYUV::updateSettings()
{
  threadCount_ = settingValue(SettingsKey::videoRGBThreads);
  Filter::updateSettings();
}


void RGB32toYUV::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    uint32_t finalDataSize = input->width*input->height + input->width*input->height/2;
    std::unique_ptr<uchar[]> yuv_data(new uchar[finalDataSize]);

    if(getHWManager()->isSSE41Enabled() && input->width % 4 == 0)
    {
      if (threadCount_ != 1)
      {
        rgb_to_yuv420_i_sse41_mt(input->data.get(), yuv_data.get(), input->width, input->height, threadCount_);
      }
      else
      {
        rgb_to_yuv420_i_sse41(input->data.get(), yuv_data.get(), input->width, input->height);
      }
    }
    else
    {
      rgb_to_yuv420_i_c(input->data.get(), yuv_data.get(), input->width, input->height);
    }

    input->type = YUV420VIDEO;
    input->data = std::move(yuv_data);
    input->data_size = finalDataSize;
    sendOutput(std::move(input));

    input = getInput();
  }
}
