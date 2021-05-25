#pragma once
#include "filter.h"

#include <QAudioFormat>

class SpeexAEC;
class SpeexDSP;

// Digital Signal Processing (DSP) filter. Uses Speex to improve audio quality
// in various ways

class DSPFilter : public Filter
{
public:
  DSPFilter(QString id, StatisticsInterface* stats,
            std::shared_ptr<SpeexAEC> aec, QAudioFormat &format,
            bool AECReference, bool doAEC, bool doDenoise, bool doDereverb,
            bool doAGC, int32_t volume = 0, int maxGain = 0);

  ~DSPFilter();

  void updateSettings();

protected:

  void process();

private:

  // We are not responsible for aec_ since it can be used in multiple instances.
  std::shared_ptr<SpeexAEC> aec_; // always exists
  std::unique_ptr<SpeexDSP> dsp_; // may not exist

  bool AECReference_;
  bool doAEC_;
  bool doDSP_;
};
