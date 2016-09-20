#pragma once

#include "filter.h"

#include <opus.h>

class OpusEncoderFilter : public Filter
{
public:
  OpusEncoderFilter(StatisticsInterface* stats);

  void init();

protected:
  void process();


private:
  OpusEncoder* enc_;

  unsigned char* output_;

  opus_int32 max_data_bytes_;

};
