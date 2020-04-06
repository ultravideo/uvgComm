#include "aecfilter.h"

#include "aecprocessor.h"

#include "common.h"


AECFilter::AECFilter(QString id, StatisticsInterface* stats,
                     QAudioFormat format, AECType type,
                     std::shared_ptr<AECProcessor> echo_state):
  Filter(id, "AEC Input", stats, RAWAUDIO, RAWAUDIO),
  type_(type),
  aec_(echo_state)
{
  if (aec_ == nullptr)
  {
    aec_ = std::make_shared<AECProcessor>();
    aec_->init(format);
  }
}


AECFilter::~AECFilter()
{
  if (type_ == AEC_INPUT && aec_)
  {
    aec_->cleanup();
  }
}


void AECFilter::process()
{
  if (!aec_)
  {
    printProgramError(this, "AEC not set");
    return;
  }

  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    if (type_ == AEC_INPUT)
    {
      input->data = aec_->processInputFrame(std::move(input->data), input->data_size);
    }
    else if(type_ == AEC_ECHO)
    {
      input->data = aec_->processEchoFrame(std::move(input->data), input->data_size);
    }
    else
    {
      printProgramError(this, "Unknown aec type");
    }

    sendOutput(std::move(input));

    // get next input
    input = getInput();
  }
}
