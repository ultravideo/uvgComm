#pragma once
#include "filter.h"

#include "openHevcWrapper.h"

class OpenHEVCFilter : public Filter
{
public:
  OpenHEVCFilter(QString id, StatisticsInterface* stats);

  void init();
  void uninit();
  void run();

protected:
  virtual void process();

private:

  std::unique_ptr<Data> combineFrame();

  OpenHevc_Handle handle_;

  bool parameterSets_;

  uint32_t waitFrames_;

  bool slices_;

  std::vector<std::unique_ptr<Data>> sliceBuffer_;
};
