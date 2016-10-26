#include "speexaecfilter.h"

#include <QDebug>


const uint16_t FRAMESPERSECOND = 25;


SpeexAECFilter::SpeexAECFilter(StatisticsInterface* stats, QAudioFormat format):
  Filter("SpeexAEC", stats, true, true),
  preprocess_state_(NULL),
  echo_state_(NULL),
  format_(format),
  frameSize_(0),
  max_data_bytes_(65536),
  in_(0),
  out_(0)
{

  frameSize_ = format_.sampleRate()/FRAMESPERSECOND;
  uint16_t echoFilterLength = frameSize_*10;
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

  pcmOutput_ = new int16_t[max_data_bytes_];

  preprocess_state_ = speex_preprocess_state_init(frameSize_, format_.sampleRate());

  speex_preprocess_ctl(preprocess_state_, SPEEX_PREPROCESS_SET_ECHO_STATE, echo_state_);
}

SpeexAECFilter::~SpeexAECFilter()
{
  speex_preprocess_state_destroy(preprocess_state_);
  speex_echo_state_destroy(echo_state_);

}


void SpeexAECFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    if(format_.bytesPerFrame()*frameSize_ == input->data_size)
    {
      if(input->local)
      {
        ++in_;
        //qDebug() << "AEC: Getting input from mic:" << in_;
        speex_echo_playback(echo_state_, (int16_t*)input->data.get());
      }
      else if( in_ >= out_)
      {
        ++out_;
        //qDebug() << "AEC: Getting reply from outside:" << out_ << "in:" << in_;
        std::unique_ptr<uchar[]> pcm_frame(new uchar[input->data_size]);

        // TODO: implement more channels
        if(format_.channelCount() == 1)
        {
          speex_preprocess_run(preprocess_state_, (int16_t*)input->data.get());
        }

        speex_echo_capture(echo_state_, (int16_t*)input->data.get(), pcmOutput_);

        //memcpy(pcm_frame.get(), input->data.get(), input->data_size);
        memcpy(pcm_frame.get(), pcmOutput_, input->data_size);
        input->data = std::move(pcm_frame);
        sendOutput(std::move(input));
      }
      else
      {
        qWarning() << "Warning: AEC received too much output before input";
      }
    }
    else
    {
      qWarning() << "Warning: Echo cancel received unexpected frame size:"
                 << input->data_size << "Expected:" << format_.bytesPerFrame()*frameSize_;
    }
    input = getInput();
  }
}
