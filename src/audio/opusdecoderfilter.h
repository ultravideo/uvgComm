#pragma once
#include "filter.h"

#include <opus.h>
#include <QAudioFormat>

class OpusDecoderFilter : public Filter
{
public:
  OpusDecoderFilter(QString id, QAudioFormat format, StatisticsInterface* stats);
  ~OpusDecoderFilter();

  virtual bool init();

protected:
  void process();

private:

  OpusDecoder *dec_;

  int16_t* pcmOutput_;
  int32_t max_data_bytes_;

  QAudioFormat format_;
};
