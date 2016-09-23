#pragma once

#include "filter.h"

#include "openHevcWrapper.h"



class OpenHEVCFilter : public Filter
{
public:
  OpenHEVCFilter(StatisticsInterface* stats);

  void init();

protected:
  virtual void process();

private:
  OpenHevc_Handle handle_;

  bool parameterSets_;
};
