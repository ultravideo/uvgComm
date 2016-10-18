#include "speexaecfilter.h"


#include <QDebug>



SpeexAECFilter::SpeexAECFilter(StatisticsInterface* stats, QAudioFormat format):
  Filter("SpeexAEC", stats, true, true),
  echo_state_(NULL),
  format_(format),
  frameSize_(0)
{
  uint16_t echoFilterLength = 1024;

  // todo find out correct value
  frameSize_ = 7680/format_.bytesPerFrame();

  if(format.channelCount() > 1)
  {
    echo_state_ = speex_echo_state_init_mc(frameSize_,
                                           echoFilterLength,
                                           format.channelCount(),
                                           format.channelCount());
  }
  else
  {
    echo_state_ = speex_echo_state_init(frameSize_, echoFilterLength);
  }
}


void SpeexAECFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    if(frameSize_ == input->data_size*format_.bytesPerFrame())
    {

      if(input->local)
      {
        speex_echo_playback(echo_state_, (int16_t*)input->data.get());
      }
      else
      {
        speex_echo_capture(echo_state_, (int16_t*)input->data.get(), pcmOutput_);

        std::unique_ptr<uchar[]> pcm_frame(new uchar[input->data_size]);
        memcpy(pcm_frame.get(), pcmOutput_, input->data_size);
        input->data = std::move(pcm_frame);
        sendOutput(std::move(input));
      }

      input = getInput();
    }
    else
    {
      qWarning() << "Warning: Echo cancel received unexpected frame size:"
                 << input->data_size << "Expected:" << frameSize_;
    }
  }
}
