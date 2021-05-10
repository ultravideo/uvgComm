#include "dspfilter.h"

#include "speexdsp.h"
#include "speexaec.h"

#include "common.h"


DSPFilter::DSPFilter(QString id, StatisticsInterface* stats,
                     DSPMode mode, std::shared_ptr<SpeexAEC> aec,
                     QAudioFormat& format):
  Filter(id, "DSP", stats, RAWAUDIO, RAWAUDIO),
  aec_(aec),
  dsp_(nullptr),
  mode_(mode)
{
  if (mode == DSP_PROCESSOR)
  {
    dsp_ = std::make_unique<SpeexDSP> (format);
    dsp_->init();
  }
}


DSPFilter::~DSPFilter()
{
  dsp_.reset();
  mode_ = NO_DSP_MODE;
}


void DSPFilter::updateSettings()
{
  if (dsp_)
  {
    dsp_->updateSettings();
  }
}


void DSPFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    switch (mode_)
    {
    case DSP_PROCESSOR:
    {
      if (aec_)
      {
        input->data = aec_->processInputFrame(std::move(input->data), input->data_size);
      }

      if (dsp_)
      {
        input->data = dsp_->processInputFrame(std::move(input->data), input->data_size);
      }

      break;
    }
    case ECHO_FRAME_PROVIDER:
    {
      if (aec_)
      {
        aec_->processEchoFrame(input->data.get(), input->data_size);
      }
      else
      {
        printProgramError(this, "");
      }

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
