#include "aecfilter.h"

#include "common.h"

// this is how many frames the audio capture seems to send
const uint16_t FRAMESPERSECOND = 25;

bool PREPROCESSOR = false;


AECFilter::AECFilter(QString id, StatisticsInterface* stats,
                     QAudioFormat format, AECType type,
                     SpeexEchoState *echo_state):
  Filter(id, "AEC Input", stats, RAWAUDIO, RAWAUDIO),
  echo_state_(echo_state),
  preprocess_state_(nullptr),
  format_(format),
  samplesPerFrame_(format.sampleRate()/FRAMESPERSECOND),
  max_data_bytes_(65536),
  type_(type)
{
  uint16_t echoFilterLength = samplesPerFrame_*10;
  if (echo_state == nullptr)
  {
    if(format.channelCount() > 1)
    {
      echo_state_ = speex_echo_state_init_mc(samplesPerFrame_,
                                             echoFilterLength,
                                             format.channelCount(),
                                             format.channelCount());
    }
    else
    {
      echo_state_ = speex_echo_state_init(samplesPerFrame_, echoFilterLength);
    }
  }

  pcmOutput_ = new int16_t[max_data_bytes_];

  if (PREPROCESSOR && type_ == AEC_INPUT)
  {
    preprocess_state_ = speex_preprocess_state_init(samplesPerFrame_, format_.sampleRate());
    speex_preprocess_ctl(preprocess_state_, SPEEX_PREPROCESS_SET_ECHO_STATE, echo_state_);
  }
}

AECFilter::~AECFilter()
{
  if (type_ == AEC_INPUT)
  {
    if (PREPROCESSOR)
    {
      speex_preprocess_state_destroy(preprocess_state_);
    }
    speex_echo_state_destroy(echo_state_);
  }
}

void AECFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    for(uint32_t i = 0; i < input->data_size; i += format_.bytesPerFrame()*samplesPerFrame_)
    {
      if (type_ == AEC_INPUT)
      {
        if(format_.channelCount() == 1 && PREPROCESSOR)
        {
          speex_preprocess_run(preprocess_state_, (int16_t*)input->data.get()+i/2);
        }

        speex_echo_capture(echo_state_, (int16_t*)input->data.get()+i/2, pcmOutput_+i/2);
      }
      else if (type_ == AEC_ECHO)
      {
        speex_echo_playback(echo_state_, (int16_t*)input->data.get() + i/2);
      }
    }

    if (type_ == AEC_INPUT)
    {
      std::unique_ptr<uchar[]> pcm_frame(new uchar[input->data_size]);
      //memcpy(pcm_frame.get(), input->data.get(), input->data_size);
      memcpy(pcm_frame.get(), pcmOutput_, input->data_size);
      input->data = std::move(pcm_frame);
    }


    sendOutput(std::move(input));

    input = getInput();
  }
}
