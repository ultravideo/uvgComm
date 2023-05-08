#pragma once
#include "filter.h"

#include <opus/opus.h>
#include <QtMultimedia/QAudioFormat>

class OpusEncoderFilter : public Filter
{
public:
  OpusEncoderFilter(QString id, QAudioFormat format, StatisticsInterface* stats,
                    std::shared_ptr<ResourceAllocator> hwResources);
  ~OpusEncoderFilter();

  virtual void updateSettings();

  bool init();

protected:
  void process();

private:
  OpusEncoder* enc_;

  uchar* opusOutput_;
  uint32_t max_data_bytes_;

  QAudioFormat format_;

  uint32_t samplesPerFrame_;
};
