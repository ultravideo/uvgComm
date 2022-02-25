#include "rgb32toyuv.h"

#include "yuvconversions.h"

#include "media/hwresourcemanager.h"

#include "settingskeys.h"
#include "common.h"

#include <QSettings>

RGB32toYUV::RGB32toYUV(QString id, StatisticsInterface *stats,
                       std::shared_ptr<HWResourceManager> hwResources):
  Filter(id, "RGB32toYUV", stats, hwResources, DT_RGB32VIDEO, DT_YUV420VIDEO),
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
    // TODO: Currently the RGB32 to YUV420 implementations flip the input orientation
    input->vInfo->flippedVertically = true;
    input = normalizeOrientation(std::move(input));

    uint32_t finalDataSize = input->vInfo->width*input->vInfo->height + 
                             input->vInfo->width*input->vInfo->height/2;
    std::unique_ptr<uchar[]> yuv_data(new uchar[finalDataSize]);

    if(getHWManager()->isSSE41Enabled() && input->vInfo->width % 4 == 0)
    {
      if (threadCount_ != 1)
      {
        rgb_to_yuv420_i_sse41_mt(input->data.get(), yuv_data.get(), 
                                 input->vInfo->width, input->vInfo->height, threadCount_);
      }
      else
      {
        rgb_to_yuv420_i_sse41(input->data.get(), yuv_data.get(), input->vInfo->width, input->vInfo->height);
      }
    }
    else
    {
      rgb_to_yuv420_i_c(input->data.get(), yuv_data.get(), input->vInfo->width, input->vInfo->height);
    }

    input->type = DT_YUV420VIDEO;
    input->data = std::move(yuv_data);
    input->data_size = finalDataSize;
    sendOutput(std::move(input));

    input = getInput();
  }
}
