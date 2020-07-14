#pragma once
#include "filter.h"

#include "openHevcWrapper.h"

class OpenHEVCFilter : public Filter
{
public:
  OpenHEVCFilter(uint32_t sessionID, StatisticsInterface* stats);

  virtual bool init();
  void uninit();
  void run();

protected:
  virtual void process();

private:

  // combine the slices to a frame.
  void combineFrame(std::unique_ptr<Data> &combinedFrame);

  OpenHevc_Handle handle_;

  bool parameterSets_;

  uint32_t waitFrames_;

  bool slices_;

  std::vector<std::unique_ptr<Data>> sliceBuffer_;

  uint32_t sessionID_;
};
