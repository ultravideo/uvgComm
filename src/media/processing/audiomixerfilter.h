#pragma once
#include "filter.h"

#include <memory>

class AudioOutput;

// This class is only a passthough class which holds the streams sessionID
// This sessionID can then be used to identify which stream a samples belongs
// to in mixing.

class AudioMixerFilter : public Filter
{
public:

  AudioMixerFilter(QString id, StatisticsInterface* stats,
                   uint32_t sessionID, std::shared_ptr<AudioOutput> output);

protected:
  void process();

private:
  uint32_t sessionID_;
  std::shared_ptr<AudioOutput> output_;
};
