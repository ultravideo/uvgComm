#include "roimanualfilter.h"

#include "ui/gui/videointerface.h"

#include "media/resourceallocator.h"

#include "settingskeys.h"
#include "common.h"

ROIManualFilter::ROIManualFilter(QString id, StatisticsInterface* stats,
                                   std::shared_ptr<ResourceAllocator> hwResources,
                                   VideoInterface* roiInterface):
  Filter(id, "ManualROI", stats, hwResources, DT_YUV420VIDEO, DT_YUV420VIDEO),
  roiSurface_(roiInterface),
  qp_(0)
{}


bool ROIManualFilter::init()
{
  qp_ = settingValue(SettingsKey::videoQP);
  return true;
}


void ROIManualFilter::updateSettings()
{
  qp_ = settingValue(SettingsKey::videoQP);
}


void ROIManualFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    // TODO: Check that rate control is not enabled
    if (getHWManager()->useManualROI())
    {
      input->vInfo->roiWidth = input->vInfo->width;
      input->vInfo->roiHeight = input->vInfo->height;
      input->vInfo->roiArray = roiSurface_->getRoiMask(input->vInfo->roiWidth,
                                                       input->vInfo->roiHeight, qp_, true);
    }

    sendOutput(std::move(input));

    input = getInput();
  }
}
