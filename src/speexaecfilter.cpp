#include "speexaecfilter.h"

#include <QDebug>

// this is how many frames the audio capture seems to send
const uint16_t FRAMESPERSECOND = 25;

SpeexAECFilter::SpeexAECFilter(QString id, StatisticsInterface* stats, QAudioFormat format):
  Filter(id, "SpeexAEC", stats, RAWAUDIO, RAWAUDIO),
  echo_state_(NULL),
  preprocess_state_(NULL),
  format_(format),
  numberOfSamples_(0),
  max_data_bytes_(65536),
  in_(0),
  out_(0)
{
  numberOfSamples_ = format_.sampleRate()/FRAMESPERSECOND;
  uint16_t echoFilterLength = numberOfSamples_*10;
  if(format.channelCount() > 1)
  {
    echo_state_ = speex_echo_state_init_mc(numberOfSamples_,
                                           echoFilterLength,
                                           format.channelCount(),
                                           format.channelCount());
  }
  else
  {
    echo_state_ = speex_echo_state_init(numberOfSamples_, echoFilterLength);
  }

  pcmOutput_ = new int16_t[max_data_bytes_];

  preprocess_state_ = speex_preprocess_state_init(numberOfSamples_, format_.sampleRate());

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
    for(uint32_t i = 0; i < input->data_size; i += format_.bytesPerFrame()*numberOfSamples_)
    {
      //qDebug() << "AEC audio input:" << input->data_size << "index:" << i << "samples:" << numberOfSamples_;
      if(input->source == LOCAL)
      {
        ++in_;
        //qDebug() << "AEC: Getting input from mic:" << in_;
        speex_echo_playback(echo_state_, (int16_t*)input->data.get() + i/2);
      }
      else if( in_ >= out_)
      {
        ++out_;
        //qDebug() << "AEC: Getting reply from outside:" << out_ << "in:" << in_;

        // TODO: implement more channels
        if(format_.channelCount() == 1)
        {
          speex_preprocess_run(preprocess_state_, (int16_t*)input->data.get()+i/2);
        }

        speex_echo_capture(echo_state_, (int16_t*)input->data.get()+i/2, pcmOutput_+i/2);
      }
      else
      {
        qWarning() << "Warning: AEC received too much output before input";
        break;
      }
    }

    //qDebug() << "Echo cancel received frame size:"
    //         << input->data_size << "One frame size:" << format_.bytesPerFrame()*numberOfSamples_;

    if(input->source == REMOTE)
    {
      std::unique_ptr<uchar[]> pcm_frame(new uchar[input->data_size]);
      //memcpy(pcm_frame.get(), input->data.get(), input->data_size);
      memcpy(pcm_frame.get(), pcmOutput_, input->data_size);
      input->data = std::move(pcm_frame);
      sendOutput(std::move(input));
    }

    input = getInput();
  }
}
