#pragma once

#include "filter.h"

#include <opus.h>

class OpusDecoderFilter : public Filter
{
public:
  OpusDecoderFilter(StatisticsInterface* stats);

  void init();

protected:
  void process();

private:

  OpusDecoder *dec_;

  int16_t* pcmOutput_;
  int32_t max_data_bytes_;
};
