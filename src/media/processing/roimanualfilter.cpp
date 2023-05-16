#include "roimanualfilter.h"

#include "ui/gui/videointerface.h"

#include "media/resourceallocator.h"

#include "settingskeys.h"
#include "common.h"
#include "logger.h"

ROIManualFilter::ROIManualFilter(QString id, StatisticsInterface* stats,
                                   std::shared_ptr<ResourceAllocator> hwResources,
                                   VideoInterface* roiInterface):
  Filter(id, "ManualROI", stats, hwResources, DT_YUV420VIDEO, DT_YUV420VIDEO),
  roiSurface_(roiInterface),
  qp_(0),
  rateControl_(0)
{}


bool ROIManualFilter::init()
{
  qp_ = settingValue(SettingsKey::videoQP);
  rateControl_ = settingValue(SettingsKey::videoBitrate);
  return true;
}


void ROIManualFilter::updateSettings()
{
  enabled_ = settingString(SettingsKey::roiMode) == "manual";
  qp_ = settingValue(SettingsKey::videoQP);
  rateControl_ = settingValue(SettingsKey::videoBitrate);
}


void ROIManualFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    // TODO: Check that rate control is not enabled
    if (enabled_ && getHWManager()->useManualROI())
    {
      if (rateControl_ == 0)
      {
        input->vInfo->roiWidth = input->vInfo->width;
        input->vInfo->roiHeight = input->vInfo->height;
        if (roiSurface_)
        {
          input->vInfo->roiArray = roiSurface_->getRoiMask(input->vInfo->roiWidth,
                                                           input->vInfo->roiHeight, qp_, true);
        }
        else
        {
          Logger::getLogger()->printWarning(this, "RoI surface not set");
        }
      }
      else
      {
        Logger::getLogger()->printWarning(this, "Please disable rate control when using ROI");
      }
    }

    sendOutput(std::move(input));

    input = getInput();
  }
}
