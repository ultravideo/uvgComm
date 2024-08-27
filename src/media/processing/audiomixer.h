#pragma once

#include "filter.h"

#include <QMutex>
#include <QObject>

#include <map>
#include <deque>

#include <memory>

struct Data;

// Mixes multiple audio tracks into one

class AudioMixer : public QObject
{
  Q_OBJECT
public:
  AudioMixer();

  // TODO: This should take into account different sizes of audio frames for
  // compatability with other applications

  std::unique_ptr<Data> mixAudio(std::unique_ptr<Data> input,
                                 std::unique_ptr<Data> potentialOutput,
                                    uint32_t sessionID);

  void addInput()
  {
    ++inputs_;
  }

  void removeInput()
  {
    --inputs_;
  }

private:

  int16_t dbToint16(float dB) const;
  float int16ToDB(int16_t value) const;

  std::unique_ptr<uchar[]> doMixing(uint32_t frameSize);

  int32_t inputs_;

  QMutex mixingMutex_;
  std::map<uint32_t, std::deque<std::unique_ptr<Data>>> mixingBuffer_;
};
