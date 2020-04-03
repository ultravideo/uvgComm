#pragma once
#include "filter.h"

#include <speex/speex_echo.h>

#include <QAudioFormat>

class AECPlaybackFilter : public Filter
{
public:
  AECPlaybackFilter(QString id, StatisticsInterface* stats,
                    QAudioFormat format, SpeexEchoState *echo_state);

protected:
  void process();

private:
  SpeexEchoState *echo_state_;

  QAudioFormat format_;

  uint32_t samplesPerFrame_;
};
