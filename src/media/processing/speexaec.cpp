#include "speexaec.h"

#include <QSettings>

#include "audioframebuffer.h"

#include "settingskeys.h"
#include "common.h"
#include "global.h"
#include "logger.h"


SpeexAEC::SpeexAEC(QAudioFormat format):
  format_(format),
  samplesPerFrame_(format.sampleRate()/AUDIO_FRAMES_PER_SECOND),
  echo_state_(nullptr),
  preprocessor_(nullptr),
  echoBuffer_(nullptr),
  echoBufferUpdated_(true),
  playbackDelay_(0),
  echoFilterLength_(0),
  enabled_(false)
{}


void SpeexAEC::updateSettings()
{
  if ( preprocessor_ != nullptr)
  {
    enabled_ = settingEnabled(SettingsKey::audioAEC);

    if (enabled_)
    {
      if (echoFilterLength_ != settingValue(SettingsKey::audioAECFilterLength))
      {
        init();
      }

      playbackDelay_ = settingValue(SettingsKey::audioAECDelay);

      speexMutex_.lock();
      speex_preprocess_ctl(preprocessor_, SPEEX_PREPROCESS_SET_ECHO_STATE, echo_state_);

      // these are the default values
      // when not speaking
      int suppression = -40;
      speex_preprocess_ctl(preprocessor_,
                           SPEEX_PREPROCESS_SET_ECHO_SUPPRESS,
                           &suppression);

      // when speaking
      suppression = -15;
      speex_preprocess_ctl(preprocessor_,
                           SPEEX_PREPROCESS_SET_ECHO_SUPPRESS_ACTIVE,
                           &suppression);

      speexMutex_.unlock();
    }
    else
    {
      speexMutex_.lock();
      speex_preprocess_ctl(preprocessor_, SPEEX_PREPROCESS_SET_ECHO_STATE, nullptr);
      speexMutex_.unlock();
    }

  }
}


void SpeexAEC::init()
{
  if (preprocessor_ != nullptr || echo_state_ != nullptr)
  {
    cleanup();
  }

  echoFilterLength_ = settingValue(SettingsKey::audioAECFilterLength);

  // should be around 1/3 of the room reverberation time
  uint16_t echoFilterLength = format_.sampleRate()*echoFilterLength_/1000;

  Logger::getLogger()->printNormal(this, "Initiating echo frame processing", 
                                   {"Filter length"}, {
                                    QString::number(echoFilterLength) + " samples"});

  speexMutex_.lock();

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

  speexMutex_.unlock();

  updateSettings();
}


void SpeexAEC::cleanup()
{
  speexMutex_.lock();
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

  speexMutex_.unlock();
}


std::unique_ptr<uchar[]> SpeexAEC::processInputFrame(std::unique_ptr<uchar[]> input,
                                                     uint32_t dataSize)
{
  if (enabled_)
  {
    if (echoBuffer_ == nullptr)
    {
      Logger::getLogger()->printProgramError(this, "AEC echo not initialized. AEC not working");
      return nullptr;
    }

    if (dataSize != echoBuffer_->getDesiredSize())
    {
      Logger::getLogger()->printProgramError(this, "Wrong size of input frame for AEC");
      return nullptr;
    }

    if (playbackDelay_ == 0)
    {
      Logger::getLogger()->printWarning(this, "Not delaying AEC playback frames. The AEC will not work well. "
                                              "Something wrong in settings");
    }

    int echoBufferSize = (AUDIO_FRAMES_PER_SECOND*playbackDelay_)/1000;

    if (echoBuffer_->getBufferSize() >= echoBufferSize)
    {
      uint8_t* echoFrame = echoBuffer_->readFrame();

      if (echoFrame != nullptr)
      {
        std::unique_ptr<uchar[]> pcmOutput = std::unique_ptr<uchar[]>(new uchar[dataSize]);

        speexMutex_.lock();
        if (echo_state_)
        {
          // This is the actual AEC, the point being that the delay between echo and input
          // should be minimal for the AEC to work
          speex_echo_cancellation(echo_state_,
                                  (int16_t*)input.get(),
                                  (int16_t*)echoFrame, (int16_t*)pcmOutput.get());
        }
        else
        {
          Logger::getLogger()->printProgramWarning(this, "Echo state not set");
        }

        if(preprocessor_ != nullptr)
        {
          speex_preprocess_run(preprocessor_, (int16_t*)pcmOutput.get());
        }
        else
        {
          Logger::getLogger()->printProgramWarning(this, "Echo preprocessor not set");
        }
        speexMutex_.unlock();

        delete[] echoFrame;
        echoFrame = nullptr;

        // a safety valve that drops frames if we have too much echo
        while (echoBuffer_->getBufferSize() >= echoBufferSize*2)
        {
          Logger::getLogger()->printWarning(this, "Echo buffer has too many samples, "
                              "dropping frames to avoid clock drift", {"Status"},
                       {"Min:" + QString::number(echoBufferSize) + " < " +
                        QString::number(echoBuffer_->getBufferSize()) + " < " +
                       QString::number(echoBufferSize*2)});

          echoFrame = echoBuffer_->readFrame();
          if (echoFrame)
          {
            delete [] echoFrame;
            echoFrame = nullptr;
          }
          else
          {
            break;
          }
        }

        return pcmOutput;
      }
    }
    else if (echoBufferUpdated_)
    {
      Logger::getLogger()->printWarning(this, "We haven't buffered enough audio "
                                              "samples to start echo cancellation",
                                        "Buffered",   
                                        QString::number(echoBuffer_->getBufferSize()) + "/" +
                                        QString::number(echoBufferSize));

      echoBufferUpdated_ = false;
    }
  }

  return input;
}


void SpeexAEC::processEchoFrame(uint8_t *echo, uint32_t dataSize)
{
  if (echoBuffer_)
  {
    echoBuffer_->inputData(echo, dataSize);
    echoBufferUpdated_ = true;
  }
  else
  {
    Logger::getLogger()->printProgramError(this, "AEC not initialized when inputting echo frame");
  }
}


uint8_t* SpeexAEC::createEmptyFrame(uint32_t size)
{
  uint8_t* emptyFrame  = new uint8_t[size];
  memset(emptyFrame, 0, size);
  return emptyFrame;
}
