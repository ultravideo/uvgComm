#include "roimanualfilter.h"

#include "ui/gui/videointerface.h"

#include "media/resourceallocator.h"

ROIManualFilter::ROIManualFilter(QString id, StatisticsInterface* stats,
                                   std::shared_ptr<ResourceAllocator> hwResources,
                                   VideoInterface* roiInterface):
  Filter(id, "ManualROI", stats, hwResources, DT_YUV420VIDEO, DT_YUV420VIDEO),
  roiSurface_(roiInterface)
{}


void ROIManualFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    if (getHWManager()->useManualROI())
    {
      input->vInfo->roiWidth = input->vInfo->width;
      input->vInfo->roiHeight = input->vInfo->height;
      input->vInfo->roiArray = roiSurface_->getRoiMask(input->vInfo->roiWidth,
                                                       input->vInfo->roiHeight);
    }

    sendOutput(std::move(input));

    input = getInput();
  }
}
