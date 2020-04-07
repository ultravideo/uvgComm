#include "aecprocessor.h"

// this is how many frames the audio capture seems to send

#include "common.h"

const uint16_t FRAMESPERSECOND = 50;
bool PREPROCESSOR = false;

AECProcessor::AECProcessor(QAudioFormat format):
  format_(format),
  samplesPerFrame_(format.sampleRate()/FRAMESPERSECOND),
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
  // currently set to 100 ms
  uint16_t echoFilterLength = format_.sampleRate()/10;
  if(format_.channelCount() > 1)
  {
    echoes_[sessionID]->echo_state = speex_echo_state_init_mc(samplesPerFrame_,
                                                              echoFilterLength,
                                                              format_.channelCount(),
                                                              format_.channelCount());
  }
  else
  {
    echoes_[sessionID]->echo_state = speex_echo_state_init(samplesPerFrame_,
                                                           echoFilterLength);
  }

  if (PREPROCESSOR)
  {
    echoes_[sessionID]->preprocess_state = speex_preprocess_state_init(samplesPerFrame_,
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
    for(uint32_t i = 0; i < dataSize; i += format_.bytesPerFrame()*samplesPerFrame_)
    {
      if(format_.channelCount() == 1 &&
         echo.second->preprocess_state != nullptr)
      {
        speex_preprocess_run(echo.second->preprocess_state, (int16_t*)input.get()+i);
      }

      if (!echo.second->frames.empty())
      {
        //printDebug(DEBUG_NORMAL, "AEC Processor", "Echo cancellation",
        //          {"Echoes"}, {QString::number(echo.second->frames.size())});

        std::unique_ptr<uchar[]> echoFrame = std::move(echo.second->frames.front());
        echo.second->frames.pop_front();

        input = processInput(echo.second->echo_state, std::move(input),
                             dataSize, std::move(echoFrame), i/2);
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
                                                    uint32_t inputSize,
                                                    std::unique_ptr<uchar[]> echo,
                                                    uint32_t pos)
{
  int16_t* pcmOutput = new int16_t[max_data_bytes_];;
  speex_echo_cancellation(echo_state,
                          (int16_t*)input.get() + pos,
                          (int16_t*)echo.get(), pcmOutput);

  // TODO: cast pcmOutput instead of copying
  std::unique_ptr<uchar[]> pcm_frame(new uchar[inputSize]);
  memcpy(pcm_frame.get(), pcmOutput, inputSize);
  return pcm_frame;
}


void AECProcessor::processEchoFrame(std::unique_ptr<uchar[]> echo, uint32_t dataSize,
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
