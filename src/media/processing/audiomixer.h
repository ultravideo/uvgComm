#pragma once

#include "filter.h"

#include <QMutex>
#include <QObject>

#include <map>
#include <deque>

#include <memory>

struct Data;

class AudioMixer : public QObject
{
  Q_OBJECT
public:
  AudioMixer();

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

  std::unique_ptr<uchar[]> doMixing(uint32_t frameSize);

  int32_t inputs_;

  QMutex mixingMutex_;
  std::map<uint32_t, std::deque<std::unique_ptr<Data>>> mixingBuffer_;
};
