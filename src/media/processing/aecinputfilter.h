#pragma once
#include "filter.h"

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include <QAudioFormat>

class AECInputFilter : public Filter
{
public:
  AECInputFilter(QString id, StatisticsInterface* stats, QAudioFormat format);
  ~AECInputFilter();

  SpeexEchoState* getEchoState()
  {
    return echo_state_;
  }

protected:

  void process();

private:
  SpeexEchoState *echo_state_;
  SpeexPreprocessState *preprocess_state_;

  QAudioFormat format_;

  uint32_t samplesPerFrame_;

  int16_t* pcmOutput_;
  int32_t max_data_bytes_;
};
