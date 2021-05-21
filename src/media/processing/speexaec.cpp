#include "speexaec.h"

#include <QSettings>

#include "audioframebuffer.h"

#include "settingskeys.h"
#include "common.h"
#include "global.h"

// I tested this to be the best or at least close enough
// this is also recommended by speex documentation
// if you are in a large room, optimal time may be larger.
const int REVERBERATION_TIME_MS = 100;

const int ECHO_DELAY = 120;


SpeexAEC::SpeexAEC(QAudioFormat format):
  format_(format),
  samplesPerFrame_(format.sampleRate()/AUDIO_FRAMES_PER_SECOND),
  echo_state_(nullptr),
  preprocessor_(nullptr),
  echoBuffer_(nullptr),
  enabled_(false)
{}


void SpeexAEC::updateSettings()
{
  if ( preprocessor_ != nullptr)
  {
    enabled_ = settingEnabled(SettingsKey::audioAEC);

    if (enabled_)
    {
      speex_preprocess_ctl(preprocessor_, SPEEX_PREPROCESS_SET_ECHO_STATE, echo_state_);

      // these are the default values
      int* suppression = new int(-40);
      speex_preprocess_ctl(preprocessor_,
                           SPEEX_PREPROCESS_SET_ECHO_SUPPRESS,
                           suppression);

      *suppression = -15;
      speex_preprocess_ctl(preprocessor_,
                           SPEEX_PREPROCESS_SET_ECHO_SUPPRESS_ACTIVE,
                           suppression);

      delete suppression;
    }
    else
    {
      speex_preprocess_ctl(preprocessor_, SPEEX_PREPROCESS_SET_ECHO_STATE, nullptr);
    }

  }
}


void SpeexAEC::init()
{
  if (preprocessor_ != nullptr || echo_state_ != nullptr)
  {
    cleanup();
  }

  // should be around 1/3 of the room reverberation time
  uint16_t echoFilterLength = format_.sampleRate()*REVERBERATION_TIME_MS/1000;

  printNormal(this, "Initiating echo frame processing", {"Filter length"}, {
                QString::number(echoFilterLength)});

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

  // set echo sample rate
  int sampleRate = format_.sampleRate();
  speex_echo_ctl(echo_state_, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);

  // TODO: Support multiple channels
  int frameSize = format_.sampleRate()*format_.bytesPerFrame()/AUDIO_FRAMES_PER_SECOND;

  // here we input the samples to be made the right size for our application
  echoBuffer_ = std::make_unique<AudioFrameBuffer>(frameSize);


  preprocessor_ = speex_preprocess_state_init(samplesPerFrame_,
                                              format_.sampleRate());

  updateSettings();
}


void SpeexAEC::cleanup()
{
  if (preprocessor_ != nullptr)
  {
    speex_preprocess_state_destroy(preprocessor_);
    preprocessor_ = nullptr;
  }

  if (echo_state_ != nullptr)
  {
    speex_echo_state_destroy(echo_state_);
    echo_state_ = nullptr;
  }
}


std::unique_ptr<uchar[]> SpeexAEC::processInputFrame(std::unique_ptr<uchar[]> input,
                                                     uint32_t dataSize)
{
  if (enabled_)
  {
    if (echoBuffer_ == nullptr)
    {
      printProgramError(this, "AEC echo not initialized. AEC not working");
      return nullptr;
    }

    if (dataSize != echoBuffer_->getDesiredSize())
    {
      printProgramError(this, "Wrong size of input frame for AEC");
      return nullptr;
    }

    int echoBufferSize = (AUDIO_FRAMES_PER_SECOND*ECHO_DELAY)/1000;

    if (echoBuffer_->getBufferSize() >= echoBufferSize)
    {
      uint8_t* echoFrame = echoBuffer_->readFrame();

      if (echoFrame != nullptr)
      {
        std::unique_ptr<uchar[]> pcmOutput = std::unique_ptr<uchar[]>(new uchar[dataSize]);

        if (echo_state_)
        {
          speex_echo_cancellation(echo_state_,
                                  (int16_t*)input.get(),
                                  (int16_t*)echoFrame, (int16_t*)pcmOutput.get());
        }
        else
        {
          printProgramWarning(this, "Echo state not set");
        }

        if(preprocessor_ != nullptr)
        {
          speex_preprocess_run(preprocessor_, (int16_t*)pcmOutput.get());
        }
        else
        {
          printProgramWarning(this, "Echo preprocessor not set");
        }

        return pcmOutput;
      }
    }
    else
    {
      printWarning(this, "We haven't buffered enough audio samples to start echo cancellation", "Buffered",
                   QString::number(echoBuffer_->getBufferSize()) + "/" + QString::number(echoBufferSize));
    }
  }

  return input;
}


void SpeexAEC::processEchoFrame(uint8_t *echo,
                                uint32_t dataSize)
{
  if (echoBuffer_)
  {
    echoBuffer_->inputData(echo, dataSize);
  }
  else
  {
    printProgramError(this, "AEC not initialized when inputting echo frame");
  }
}


uint8_t* SpeexAEC::createEmptyFrame(uint32_t size)
{
  uint8_t* emptyFrame  = new uint8_t[size];
  memset(emptyFrame, 0, size);
  return emptyFrame;
}
