#pragma once
#include "filter.h"

#include <memory>

class AudioOutput;

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
