#pragma once
#include "filter.h"

class YUVtoRGB32 : public Filter
{
public:
  YUVtoRGB32(QString id, StatisticsInterface* stats, uint32_t peer);

  virtual void updateSettings();

protected:
  void process();

private:
  bool sse_;
  bool avx2_;
  int threadCount_;
};

