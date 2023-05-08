#pragma once
#include "filter.h"

#include <opus/opus.h>
#include <QtMultimedia/QAudioFormat>

class OpusDecoderFilter : public Filter
{
public:
  OpusDecoderFilter(uint32_t sessionID, QAudioFormat format,
                    StatisticsInterface* stats,
                    std::shared_ptr<ResourceAllocator> hwResources);
  ~OpusDecoderFilter();

  // setups decoder
  virtual bool init();

protected:

  // decodes input until buffer is empty
  void process();

private:

  OpusDecoder *dec_;

  int16_t* pcmOutput_;
  uint32_t max_data_bytes_;

  QAudioFormat format_;

  uint32_t sessionID_;
};
