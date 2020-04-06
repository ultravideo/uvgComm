#include "aecplaybackfilter.h"

#include "aecprocessor.h"
#include "common.h"

AECPlaybackFilter::AECPlaybackFilter(QString id, StatisticsInterface* stats,
                                     std::shared_ptr<AECProcessor> processor):
  Filter(id, "AEC filter", stats, RAWAUDIO, NONE),
  aec_(processor)
{}


void AECPlaybackFilter::process()
{
  if (!aec_)
  {
    printProgramError(this, "AEC not set");
    return;
  }

  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    aec_->processEchoFrame(std::move(input->data), input->data_size);

    // get next input
    input = getInput();
  }
}
