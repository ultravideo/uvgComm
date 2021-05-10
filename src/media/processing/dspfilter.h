#pragma once
#include "filter.h"

#include <QAudioFormat>

class SpeexAEC;
class SpeexDSP;

// Digital Signal Processing (DSP) filter. Uses Speex to improve audio quality
// in various ways

enum DSPMode {NO_DSP_MODE, DSP_PROCESSOR, ECHO_FRAME_PROVIDER};

class DSPFilter : public Filter
{
public:
  DSPFilter(QString id, StatisticsInterface* stats,
            DSPMode mode, std::shared_ptr<SpeexAEC> aec, QAudioFormat &format);
  ~DSPFilter();

  void updateSettings();

protected:

  void process();

private:

  // We are not responsible for aec_ since it can be used in multiple instances.
  std::shared_ptr<SpeexAEC> aec_; // always exists
  std::unique_ptr<SpeexDSP> dsp_; // may not exist
  DSPMode mode_;
};
