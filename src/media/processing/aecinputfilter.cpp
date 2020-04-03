#include "aecinputfilter.h"

#include "common.h"

// this is how many frames the audio capture seems to send
const uint16_t FRAMESPERSECOND = 25;

bool PREPROCESSOR = false;


AECInputFilter::AECInputFilter(QString id, StatisticsInterface* stats,
                               QAudioFormat format):
  Filter(id, "AEC Input", stats, RAWAUDIO, RAWAUDIO),
  echo_state_(nullptr),
  preprocess_state_(nullptr),
  format_(format),
  samplesPerFrame_(format.sampleRate()/FRAMESPERSECOND),
  max_data_bytes_(65536)
{
  uint16_t echoFilterLength = samplesPerFrame_*10;
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

  pcmOutput_ = new int16_t[max_data_bytes_];

  if (PREPROCESSOR)
  {
    preprocess_state_ = speex_preprocess_state_init(samplesPerFrame_, format_.sampleRate());
    speex_preprocess_ctl(preprocess_state_, SPEEX_PREPROCESS_SET_ECHO_STATE, echo_state_);
  }
}

AECInputFilter::~AECInputFilter()
{
  if (PREPROCESSOR)
  {
    speex_preprocess_state_destroy(preprocess_state_);
  }

  speex_echo_state_destroy(echo_state_);
}

void AECInputFilter::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    for(uint32_t i = 0; i < input->data_size; i += format_.bytesPerFrame()*samplesPerFrame_)
    {
      if(format_.channelCount() == 1 && PREPROCESSOR)
      {
        speex_preprocess_run(preprocess_state_, (int16_t*)input->data.get()+i/2);
      }

      speex_echo_capture(echo_state_, (int16_t*)input->data.get()+i/2, pcmOutput_+i/2);
    }

    std::unique_ptr<uchar[]> pcm_frame(new uchar[input->data_size]);
    //memcpy(pcm_frame.get(), input->data.get(), input->data_size);
    memcpy(pcm_frame.get(), pcmOutput_, input->data_size);
    input->data = std::move(pcm_frame);
    sendOutput(std::move(input));

    input = getInput();
  }
}
