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

  std::unique_ptr<uchar[]> mixAudio(std::unique_ptr<Data> input,
                                    uint32_t sessionID);

  void setNumberOfInputs(unsigned int inputs)
  {
    inputs_ = inputs;
  }

private:

  std::unique_ptr<uchar[]> doMixing(uint32_t frameSize);

  unsigned int inputs_;

  QMutex mixingMutex_;
  std::map<uint32_t, std::deque<std::unique_ptr<Data>>> mixingBuffer_;
};
