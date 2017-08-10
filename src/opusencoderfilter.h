#pragma once
#include "filter.h"

#include <opus.h>
#include <QAudioFormat>

class OpusEncoderFilter : public Filter
{
public:
  OpusEncoderFilter(QString id, QAudioFormat format, StatisticsInterface* stats);
  ~OpusEncoderFilter();

  bool init();

protected:
  void process();

private:
  OpusEncoder* enc_;

  uchar* opusOutput_;
  uint32_t max_data_bytes_;

  QAudioFormat format_;

  uint32_t numberOfSamples_;
};
