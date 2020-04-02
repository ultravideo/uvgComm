#pragma once
#include "filter.h"

// converts the YUV420 video frame to and RGB32 frame. May use optimizations.

class YUVtoRGB32 : public Filter
{
public:
  YUVtoRGB32(QString id, StatisticsInterface* stats);

  virtual void updateSettings();

protected:
  void process();

private:
  bool sse_;
  bool avx2_;
  int threadCount_;
};

