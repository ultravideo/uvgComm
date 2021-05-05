#pragma once
#include "filter.h"

#include <QAudioFormat>

class SpeexDSP;


enum DSPMode {NO_DSP_MODE, DSP_PROCESSOR, ECHO_FRAME_PROVIDER};

class DSPFilter : public Filter
{
public:
  DSPFilter(QString id, StatisticsInterface* stats,
            DSPMode mode, std::shared_ptr<SpeexDSP> dsp);
  ~DSPFilter();

protected:

  void process();

private:

  std::shared_ptr<SpeexDSP> dsp_;
  DSPMode mode_;
};
