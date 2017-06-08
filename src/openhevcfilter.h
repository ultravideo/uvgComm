#pragma once
#include "filter.h"

#include "openHevcWrapper.h"

class OpenHEVCFilter : public Filter
{
public:
  OpenHEVCFilter(QString id, StatisticsInterface* stats);

  void init();

  void run();

protected:
  virtual void process();

private:
  OpenHevc_Handle handle_;

  bool parameterSets_;

  uint32_t waitFrames_;
};
