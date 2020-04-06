#pragma once
#include "filter.h"

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include <QAudioFormat>

enum AECType {AEC_INPUT, AEC_ECHO};

class AECFilter : public Filter
{
public:
  AECFilter(QString id, StatisticsInterface* stats, QAudioFormat format,
            AECType type, SpeexEchoState *echo_state = nullptr);
  ~AECFilter();

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

  AECType type_;
};
