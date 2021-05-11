#include "audiooutputfilter.h"

AudioOutputFilter::AudioOutputFilter(QString id, StatisticsInterface* stats,
                                     QAudioFormat format):
  Filter(id, "Audio Output", stats, RAWAUDIO, NONE),
  output_()
{
  maxBufferSize_ = 3; // to avoid extra latency
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
