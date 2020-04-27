#include "audiomixerfilter.h"

#include "audiooutputdevice.h"

AudioMixerFilter::AudioMixerFilter(QString id, StatisticsInterface* stats,
                 uint32_t sessionID, std::shared_ptr<AudioOutputDevice> output):
  Filter(id, "Audio Mixer", stats, RAWAUDIO, RAWAUDIO),
  sessionID_(sessionID),
  output_(output)
{}


void AudioMixerFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    if (output_)
    {
      output_->takeInput(std::move(input), sessionID_);
    }

    // get next input
    input = getInput();
  }
}
