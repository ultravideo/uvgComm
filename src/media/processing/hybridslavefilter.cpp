#include "hybridslavefilter.h"

#include "settingskeys.h"
#include "common.h"

HybridSlaveFilter::HybridSlaveFilter(QString id, StatisticsInterface *stats,
                                     std::shared_ptr<ResourceAllocator> hwResources, DataType type):
Filter(id, "Hybrid", stats, hwResources, type, type)
{}


void HybridSlaveFilter::setConnection(int index, bool status)
{
  setOutputStatus(index, status);
}


int HybridSlaveFilter::getBitrate()
{
  if (output_ == DT_HEVCVIDEO)
  {
    return settingValue(SettingsKey::videoBitrate);
  }
  else if (output_ == DT_OPUSAUDIO)
  {
    return settingValue(SettingsKey::audioBitrate);
  }

  return 0;
}


void HybridSlaveFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {

    sendOutput(std::move(input));
    input = getInput();
  }
}
