#include "aecplaybackfilter.h"

#include "aecprocessor.h"
#include "common.h"

AECPlaybackFilter::AECPlaybackFilter(QString id, StatisticsInterface* stats,
                                     uint32_t sessionID,
                                     std::shared_ptr<AECProcessor> processor):
  Filter(id, "AEC echo filter", stats, RAWAUDIO, NONE),
  sessionID_(sessionID),
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
    aec_->processEchoFrame(std::move(input->data), input->data_size, sessionID_);

    // get next input
    input = getInput();
  }
}
