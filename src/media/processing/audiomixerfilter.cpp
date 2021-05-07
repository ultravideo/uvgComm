#include "audiomixerfilter.h"

#include "audiomixer.h"

#include "statisticsinterface.h"
#include "common.h"

#include <QDateTime>


AudioMixerFilter::AudioMixerFilter(QString id, StatisticsInterface* stats,
                 uint32_t sessionID, std::shared_ptr<AudioMixer> mixer):
  Filter(id, "Audio Mixer", stats, RAWAUDIO, RAWAUDIO),
  sessionID_(sessionID),
  mixer_(mixer),
  stats_(stats)
{}


AudioMixerFilter::~AudioMixerFilter()
{}


void AudioMixerFilter::start()
{
  mixer_->addInput();
  Filter::start();
}


void AudioMixerFilter::stop()
{
  Filter::stop();
  mixer_->removeInput();
}


void AudioMixerFilter::process()
{
  if (!mixer_)
  {
    printProgramError(this, "Audio mixer not set for audio mixer filter");
    return;
  }

  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    // Add audio delay to statistics
    int64_t delay = QDateTime::currentMSecsSinceEpoch() - input->presentationTime;

    stats_->receiveDelay(sessionID_, "Audio", delay);

    if (mixer_)
    {
      std::unique_ptr<Data> potentialOutput =
          std::unique_ptr<Data> (shallowDataCopy(input.get()));

      potentialOutput->data_size = input->data_size;

      std::unique_ptr<Data> output = mixer_->mixAudio(std::move(input), std::move(potentialOutput), sessionID_);

      // mixer only provides output if the mixing happens
      if (output != nullptr && output->data != nullptr)
      {
        sendOutput(std::move(output));
      }
    }
    else
    {
      printProgramError(this, "No mixer set for mixer filter");
    }

    // get next input
    input = getInput();
  }
}
