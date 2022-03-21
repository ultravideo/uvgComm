#include "dspfilter.h"

#include "speexdsp.h"
#include "speexaec.h"

#include "common.h"
#include "logger.h"


DSPFilter::DSPFilter(QString id, StatisticsInterface* stats,
                     std::shared_ptr<ResourceAllocator> hwResources,
                     std::shared_ptr<SpeexAEC> aec, QAudioFormat& format,
                     bool AECReference, bool doAEC, bool doDenoise,
                     bool doDereverb, bool doAGC, int32_t volume, int maxGain):
  Filter(id, "DSP", stats, hwResources, DT_RAWAUDIO, DT_RAWAUDIO),
  aec_(aec),
  dsp_(nullptr),
  AECReference_(AECReference),
  doAEC_(doAEC),
  doDSP_(doDereverb || doDereverb || doAGC)
{
  if (doDSP_)
  {
    dsp_ = std::make_unique<SpeexDSP> (format);
    dsp_->init(doAGC, doDenoise, doDereverb, volume, maxGain);
  }

  if (doAEC_ && AECReference_)
  {
    Logger::getLogger()->printProgramWarning(this, "One filter doing both AEC and "
                              "providing reference for it");
  }
}


DSPFilter::~DSPFilter()
{
  doAEC_ = false;
  AECReference_ = false;
  doDSP_ = false;
  dsp_.reset();
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
    // do dsp operation such as denoise, dereverb and agc
    if (doDSP_ && dsp_)
    {
      input->data = dsp_->processInputFrame(std::move(input->data), input->data_size);
    }

    // do echo cancellation
    if (doAEC_ && aec_)
    {
      input->data = aec_->processInputFrame(std::move(input->data), input->data_size);
    }

    // provide a reference frame of our speaker output so AEC can remove echo
    if (AECReference_ && aec_)
    {
      aec_->processEchoFrame(input->data.get(), input->data_size);
    }

    if (input->data != nullptr)
    {
      sendOutput(std::move(input));
    }

    // get next input
    input = getInput();
  }
}
