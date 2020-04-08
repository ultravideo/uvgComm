#include "aecprocessor.h"

// this is how many frames the audio capture seems to send

#include "common.h"
#include "global.h"


bool PREPROCESSOR = true;
const int REVERBERATION_TIME_MS = 100;

AECProcessor::AECProcessor(QAudioFormat format):
  format_(format),
  samplesPerFrame_(format.sampleRate()/AUDIO_FRAMES_PER_SECOND),
  global_preprocessor_(nullptr)
{

  global_preprocessor_ = speex_preprocess_state_init(samplesPerFrame_,
                                                     format_.sampleRate());

  // TODO: investigate these more for benefits.
  // not sure if multiple states work.
  void* state = new int(1);
  speex_preprocess_ctl(global_preprocessor_,
                       SPEEX_PREPROCESS_SET_AGC, state);
  //speex_preprocess_ctl(global_preprocessor_,
  //                     SPEEX_PREPROCESS_SET_DEREVERB, state);

  printNormal(this, "Set AGC preprocessor");
}


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

  if (global_preprocessor_ != nullptr)
  {
    speex_preprocess_state_destroy(global_preprocessor_);
  }
}


std::unique_ptr<uchar[]> AECProcessor::processInputFrame(std::unique_ptr<uchar[]> input,
                                                         uint32_t dataSize)
{
  // The audiocapturefilter makes sure the frames are the correct (samplesPerFrame_) size.
  if (dataSize != samplesPerFrame_*format_.bytesPerFrame())
  {
    printProgramError(this, "Wrong size of input frame for AEC");
    return nullptr;
  }


  // Do preprocess trickery defined in constructor once for input.
  if(format_.channelCount() == 1 &&
     global_preprocessor_ != nullptr)
  {
    //printNormal(this, "Running preprocessor");
    speex_preprocess_run(global_preprocessor_, (int16_t*)input.get());
  }

  echoMutex_.lock();
  for (auto& echo : echoes_)
  {
    if(format_.channelCount() == 1 &&
       echo.second->preprocess_state != nullptr)
    {
      speex_preprocess_run(echo.second->preprocess_state, (int16_t*)input.get());
    }

    if (!echo.second->frames.empty())
    {
      // remove echo queue so we use newer echo frames.
      while (echo.second->frames.size() > 2)
      {
        //printWarning(this, "Clearing echo samples because there is too many of them.");
        echo.second->frames.pop_front();
      }

      std::unique_ptr<uchar[]> echoFrame = std::move(echo.second->frames.front());
      echo.second->frames.pop_front();

      input = processInput(echo.second->echo_state, std::move(input), std::move(echoFrame));

    }
    else
    {
      printDebug(DEBUG_WARNING, "AEC Processor", "No echo to process");
    }
  }
  echoMutex_.unlock();

  return input;
}


std::unique_ptr<uchar[]> AECProcessor::processInput(SpeexEchoState *echo_state,
                                                    std::unique_ptr<uchar[]> input,
                                                    std::unique_ptr<uchar[]> echo)
{ 
  int16_t* pcmOutput = (int16_t*)input.get();

  speex_echo_cancellation(echo_state,
                          (int16_t*)input.get(),
                          (int16_t*)echo.get(), pcmOutput);

  return input;
}


void AECProcessor::processEchoFrame(std::unique_ptr<uchar[]> echo,
                                    uint32_t dataSize,
                                    uint32_t sessionID)
{
  // TODO: This should prepare for different size of frames in case since they
  // are not generated by us
  if (dataSize != samplesPerFrame_*format_.bytesPerFrame())
  {
    printPeerError(this, "Wrong size of echo frame for AEC. AEC will no operate");
    return;
  }

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
