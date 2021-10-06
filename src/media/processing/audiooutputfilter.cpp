#include "audiooutputfilter.h"

#include "global.h"

AudioOutputFilter::AudioOutputFilter(QString id, StatisticsInterface* stats,
                                     std::shared_ptr<HWResourceManager> hwResources,
                                     QAudioFormat format):
  Filter(id, "Audio Output", stats, hwResources, RAWAUDIO, NONE),
  output_()
{
#ifdef __linux__
  // linux uses very large audio frames at mic for some reason. That is why there
  // will be many audio samples arriving simultaniosly at this filter and we
  // need a relatively large buffer
  maxBufferSize_ = AUDIO_FRAMES_PER_SECOND/5;
#else
  maxBufferSize_ = AUDIO_FRAMES_PER_SECOND/2;
#endif
  output_.init(format);
}


void AudioOutputFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    output_.input(std::move(input));

    // get next input
    input = getInput();
  }
}
