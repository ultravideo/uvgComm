#pragma once

#include "filter.h"
#include "audiooutputdevice.h"

// There should be only one audio output filter

class AudioOutputFilter : public Filter
{
public:
  AudioOutputFilter(QString id, StatisticsInterface* stats, QAudioFormat format);

protected:
  void process();

private:

  AudioOutputDevice output_;
};
