#pragma once

#include "filter.h"
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include <QAudioFormat>

class SpeexAECFilter : public Filter
{
public:
  SpeexAECFilter(QString id, StatisticsInterface* stats, QAudioFormat format);
  ~SpeexAECFilter();

protected:

  void process();

private:
  SpeexEchoState *echo_state_;
  SpeexPreprocessState *preprocess_state_;

  QAudioFormat format_;

  uint32_t numberOfSamples_;

  int16_t* pcmOutput_;
  int32_t max_data_bytes_;

  uint16_t in_;
  uint16_t out_;
};
