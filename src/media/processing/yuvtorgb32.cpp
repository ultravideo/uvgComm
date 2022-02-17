#include "yuvtorgb32.h"

#include "yuvconversions.h"
#include "settingskeys.h"

#include "media/hwresourcemanager.h"

#include "common.h"

#include <QSettings>


YUVtoRGB32::YUVtoRGB32(QString id, StatisticsInterface *stats,
                       std::shared_ptr<HWResourceManager> hwResources) :
  Filter(id, "YUVtoRGB32", stats, hwResources, YUV420VIDEO, RGB32VIDEO),
  threadCount_(0)
{
  updateSettings();
}

void YUVtoRGB32::updateSettings()
{
  threadCount_ = settingValue(SettingsKey::videoYUVThreads);

  Filter::updateSettings();
}

// also flips input
void YUVtoRGB32::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    uint32_t finalDataSize = input->width*input->height*4;
    std::unique_ptr<uchar[]> rgb32_frame(new uchar[finalDataSize]);

    // TODO: Select thread count based on input resolution instead of settings.
    // Anything above fullhd should be around 2
    if (getHWManager()->isAVX2Enabled() && threadCount_ != 1 && input->width % 16 == 0)
    {
      yuv420_to_rgb_i_avx2_mt(input->data.get(), rgb32_frame.get(), input->width, input->height,
                     threadCount_);
    }
    else if (getHWManager()->isAVX2Enabled() && input->width % 16 == 0)
    {
      yuv420_to_rgb_i_avx2(input->data.get(), rgb32_frame.get(), input->width, input->height);
    }
    else if (getHWManager()->isSSE41Enabled() && input->width % 16 == 0)
    {
      yuv420_to_rgb_i_sse41(input->data.get(), rgb32_frame.get(), input->width, input->height);
    }
    else
    {
      yuv420_to_rgb_i_c(input->data.get(), rgb32_frame.get(), input->width, input->height);
    }
    input->type = RGB32VIDEO;
    input->data = std::move(rgb32_frame);
    input->data_size = finalDataSize;
    sendOutput(std::move(input));

    input = getInput();
  }
}
