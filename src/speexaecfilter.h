#pragma once

#include "filter.h"
#include <speex/speex_echo.h>
#include <QAudioFormat>

class SpeexAECFilter : public Filter
{
public:
  SpeexAECFilter(StatisticsInterface* stats, QAudioFormat format);


protected:

  void process();

private:
  SpeexEchoState *echo_state_;

  QAudioFormat format_;

  uint32_t frameSize_;
  int16_t* pcmOutput_;
};
