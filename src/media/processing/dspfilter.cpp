#include "dspfilter.h"

#include "speexdsp.h"

#include "common.h"


DSPFilter::DSPFilter(QString id, StatisticsInterface* stats,
                     DSPMode mode, std::shared_ptr<SpeexDSP> dsp):
  Filter(id, "DSP", stats, RAWAUDIO, RAWAUDIO),
  dsp_(dsp),
  mode_(mode)
{}


DSPFilter::~DSPFilter()
{
  dsp_ = nullptr;
  mode_ = NO_DSP_MODE;
}


void DSPFilter::process()
{
  if (!dsp_)
  {
    printProgramError(this, "DSP not set");
    return;
  }

  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    switch (mode_)
    {
    case DSP_PROCESSOR:
    {
      input->data = dsp_->processInputFrame(std::move(input->data), input->data_size);
      break;
    }
    case ECHO_FRAME_PROVIDER:
    {
      dsp_->processEchoFrame(input->data.get(), input->data_size);
      break;
    }
    default:
    {
      printError(this, "DSP mode not set");
      return;
    }
    }

    if (input->data != nullptr)
    {
      sendOutput(std::move(input));
    }

    // get next input
    input = getInput();
  }
}
