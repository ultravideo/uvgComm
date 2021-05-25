#pragma once
#include "filter.h"

#include <QString>

#include <memory>

class AudioMixer;
class StatisticsInterface;

// This filter handles the mixing of audio streams. The mixing is done having
// a central mixer that outputs frames. All of the mixer filters connected to
// same mixer should also be connected to same output

class AudioMixerFilter : public Filter
{
public:

  AudioMixerFilter(QString id, StatisticsInterface* stats,
                   uint32_t sessionID, std::shared_ptr<AudioMixer> mixer);

  ~AudioMixerFilter();

  virtual void start();
  virtual void stop();

protected:
  void process();

private:
  uint32_t sessionID_;
  std::shared_ptr<AudioMixer> mixer_;

  StatisticsInterface* stats_;
};
