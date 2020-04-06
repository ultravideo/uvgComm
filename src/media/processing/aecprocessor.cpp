#include "aecprocessor.h"

// this is how many frames the audio capture seems to send
const uint16_t FRAMESPERSECOND = 25;
bool PREPROCESSOR = false;

AECProcessor::AECProcessor():
  format_(),
  samplesPerFrame_(0),
  pcmOutput_(nullptr),
  max_data_bytes_(65536),
  echo_state_(nullptr),
  preprocess_state_(nullptr)
{}


void AECProcessor::init(QAudioFormat format)
{
  format_ = format;
  samplesPerFrame_ = format.sampleRate()/FRAMESPERSECOND;

  if (echo_state_ != nullptr)
  {
    cleanup();
  }

  // should be around 1/3 of
  uint16_t echoFilterLength = samplesPerFrame_*10;
  if(format_.channelCount() > 1)
  {
    echo_state_ = speex_echo_state_init_mc(samplesPerFrame_,
                                           echoFilterLength,
                                           format_.channelCount(),
                                           format_.channelCount());
  }
  else
  {
    echo_state_ = speex_echo_state_init(samplesPerFrame_, echoFilterLength);
  }

  if (PREPROCESSOR)
  {
    preprocess_state_ = speex_preprocess_state_init(samplesPerFrame_,
                                                    format_.sampleRate());
    speex_preprocess_ctl(preprocess_state_, SPEEX_PREPROCESS_SET_ECHO_STATE,
                         echo_state_);
  }

  pcmOutput_ = new int16_t[max_data_bytes_];
}


void AECProcessor::cleanup()
{
  if (PREPROCESSOR)
  {
    speex_preprocess_state_destroy(preprocess_state_);
    preprocess_state_ = nullptr;
  }
  speex_echo_state_destroy(echo_state_);
  echo_state_ = nullptr;
}


std::unique_ptr<uchar[]> AECProcessor::processInputFrame(std::unique_ptr<uchar[]> input,
                                                         uint32_t dataSize)
{
  for(uint32_t i = 0; i < dataSize; i += format_.bytesPerFrame()*samplesPerFrame_)
  {
    if(format_.channelCount() == 1 && PREPROCESSOR)
    {
      speex_preprocess_run(preprocess_state_, (int16_t*)input.get()+i/2);
    }

    speex_echo_capture(echo_state_, (int16_t*)input.get()+i/2, pcmOutput_+i/2);
  }

  std::unique_ptr<uchar[]> pcm_frame(new uchar[dataSize]);
  memcpy(pcm_frame.get(), pcmOutput_, dataSize);
  return pcm_frame;
}


std::unique_ptr<uchar[]> AECProcessor::processEchoFrame(std::unique_ptr<uchar[]> echo, uint32_t dataSize)
{
  for(uint32_t i = 0; i < dataSize; i += format_.bytesPerFrame()*samplesPerFrame_)
  {
    speex_echo_playback(echo_state_, (int16_t*)echo.get() + i/2);
  }

  return echo;
}

