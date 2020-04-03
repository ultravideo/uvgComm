#include "aecplaybackfilter.h"

// this is how many frames the audio capture seems to send
const uint16_t FRAMESPERSECOND = 25;


AECPlaybackFilter::AECPlaybackFilter(QString id, StatisticsInterface *stats,
                                     QAudioFormat format, SpeexEchoState *echo_state):
  Filter(id, "AEC Playback", stats, RAWAUDIO, RAWAUDIO),
  echo_state_(echo_state),
  format_(format),
  samplesPerFrame_(format.sampleRate()/FRAMESPERSECOND)
{}


void AECPlaybackFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    for(uint32_t i = 0; i < input->data_size; i += format_.bytesPerFrame()*samplesPerFrame_)
    {
      speex_echo_playback(echo_state_, (int16_t*)input->data.get() + i/2);
    }

    // do not modify input
    sendOutput(std::move(input));
  }
}
