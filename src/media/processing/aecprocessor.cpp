#include "aecprocessor.h"

// this is how many frames the audio capture seems to send

#include "common.h"

const uint16_t FRAMESPERSECOND = 50;
bool PREPROCESSOR = true;
const int REVERBERATION_TIME_MS = 200;

AECProcessor::AECProcessor(QAudioFormat format):
  format_(format),
  samplesPerFrame_(format_.bytesPerFrame()*format.sampleRate()/FRAMESPERSECOND),
  max_data_bytes_(65536)
{}


void AECProcessor::initEcho(uint32_t sessionID)
{
  echoMutex_.lock();
  if (echoes_.find(sessionID) != echoes_.end())
  {
    echoMutex_.unlock();
    return;
  }

  echoes_[sessionID] = std::make_shared<EchoBuffer>();
  echoes_[sessionID]->preprocess_state = nullptr;
  echoes_[sessionID]->echo_state = nullptr;

  // should be around 1/3 of the room reverberation time
  //
  uint16_t echoFilterLength = format_.sampleRate()*REVERBERATION_TIME_MS/1000;

  printNormal(this, "Initiating echo frame processing", {"Filter length"}, {
                QString::number(echoFilterLength)});

  if(format_.channelCount() > 1)
  {
    echoes_[sessionID]->echo_state = speex_echo_state_init_mc(samplesPerFrame_/2,
                                                              echoFilterLength,
                                                              format_.channelCount(),
                                                              format_.channelCount());
  }
  else
  {
    echoes_[sessionID]->echo_state = speex_echo_state_init(samplesPerFrame_/2,
                                                           echoFilterLength);
  }

  if (PREPROCESSOR)
  {
    echoes_[sessionID]->preprocess_state = speex_preprocess_state_init(samplesPerFrame_/2,
                                                                       format_.sampleRate());
    speex_preprocess_ctl(echoes_[sessionID]->preprocess_state,
                         SPEEX_PREPROCESS_SET_ECHO_STATE,
                         echoes_[sessionID]->echo_state);
  }
  else
  {
    echoes_[sessionID]->preprocess_state = nullptr;
  }
  echoMutex_.unlock();
}


void AECProcessor::cleanup()
{
  echoMutex_.lock();

  for (auto& echo : echoes_)
  {
    echo.second->frames.clear();
    if (echo.second->preprocess_state != nullptr)
    {
      speex_preprocess_state_destroy(echo.second->preprocess_state);
      echo.second->preprocess_state = nullptr;
    }

    if (echo.second->echo_state != nullptr)
    {
      speex_echo_state_destroy(echo.second->echo_state);
      echo.second->echo_state = nullptr;
    }
  }
  echoMutex_.unlock();
}


std::unique_ptr<uchar[]> AECProcessor::processInputFrame(std::unique_ptr<uchar[]> input,
                                                         uint32_t dataSize)
{
  echoMutex_.lock();

  for (auto& echo : echoes_)
  {

    for(uint32_t i = 0; i < dataSize; i += samplesPerFrame_)
    {
      if(format_.channelCount() == 1 &&
         echo.second->preprocess_state != nullptr)
      {
        speex_preprocess_run(echo.second->preprocess_state, (int16_t*)input.get()+i/2);
      }

      if (!echo.second->frames.empty())
      {
        std::unique_ptr<uchar[]> echoFrame = std::move(echo.second->frames.front());
        echo.second->frames.pop_front();

        input = processInput(echo.second->echo_state, std::move(input), std::move(echoFrame), i);

      }
      else
      {
        printDebug(DEBUG_WARNING, "AEC Processor", "No echo to process");
      }
    }
  }
  echoMutex_.unlock();

  return input;
}


std::unique_ptr<uchar[]> AECProcessor::processInput(SpeexEchoState *echo_state,
                                                    std::unique_ptr<uchar[]> input,
                                                    std::unique_ptr<uchar[]> echo,
                                                    uint32_t pos)
{ 
  int16_t* pcmOutput = (int16_t*)input.get() + pos/2;

  speex_echo_cancellation(echo_state,
                          (int16_t*)input.get() + pos/2, // divide with 2 because it is uint16
                          (int16_t*)echo.get(), pcmOutput);

  return input;
}


void AECProcessor::processEchoFrame(std::unique_ptr<uchar[]> echo,
                                    uint32_t dataSize,
                                    uint32_t sessionID)
{
  echoMutex_.lock();
  if (echoes_.find(sessionID) == echoes_.end())
  {
    echoMutex_.unlock();
    initEcho(sessionID);
  }
  else
  {
    echoMutex_.unlock();
  }

  echoMutex_.lock();
  echoes_[sessionID]->frames.push_back(std::move(echo));
  echoMutex_.unlock();
}
