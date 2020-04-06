#include "aecinputfilter.h"

#include "aecprocessor.h"

#include "common.h"


AECInputFilter::AECInputFilter(QString id, StatisticsInterface* stats):
  Filter(id, "AEC filter", stats, RAWAUDIO, RAWAUDIO)
{}


AECInputFilter::~AECInputFilter()
{
  aec_->cleanup();
}

void AECInputFilter::initInput(QAudioFormat format)
{
  aec_ = std::make_shared<AECProcessor>();
  aec_->init(format);
}


void AECInputFilter::process()
{
  if (!aec_)
  {
    printProgramError(this, "AEC not set");
    return;
  }

  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    input->data = aec_->processInputFrame(std::move(input->data), input->data_size);
    sendOutput(std::move(input));

    // get next input
    input = getInput();
  }
}
