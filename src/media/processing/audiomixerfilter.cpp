#include "audiomixerfilter.h"

#include "audiomixer.h"

#include "statisticsinterface.h"
#include "common.h"
#include "logger.h"

#include <QDateTime>


AudioMixerFilter::AudioMixerFilter(QString id, StatisticsInterface* stats,
                                   std::shared_ptr<ResourceAllocator> hwResources,
                 uint32_t sessionID, std::shared_ptr<AudioMixer> mixer):
  Filter(id, "Audio Mixer", stats, hwResources, DT_RAWAUDIO, DT_RAWAUDIO),
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
    Logger::getLogger()->printProgramError(this, "Audio mixer not set for audio mixer filter");
    return;
  }

  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    if (input->creationTimestamp > 0)
    {
      // Add audio delay to statistics
      auto now = std::chrono::system_clock::now();
      int64_t delay = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
                      - input->creationTimestamp;
      stats_->audioLatency(sessionID_, "", input->presentationTimestamp, delay);
    }
    else
    {
      stats_->audioLatency(sessionID_, "", input->presentationTimestamp, -1);
    }

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
      Logger::getLogger()->printProgramError(this, "No mixer set for mixer filter");
    }

    // get next input
    input = getInput();
  }
}
