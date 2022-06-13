#pragma once
#include "filter.h"

#include "openHevcWrapper.h"

class OpenHEVCFilter : public Filter
{
public:
  OpenHEVCFilter(uint32_t sessionID, StatisticsInterface* stats,
                 std::shared_ptr<ResourceAllocator> hwResources);

  virtual bool init();
  void uninit();

  virtual void updateSettings();

protected:
  virtual void process();

private:

  // combine the slices to a frame.
  void combineFrame(std::unique_ptr<Data> &combinedFrame);

  OpenHevc_Handle handle_;

  bool vpsReceived_;
  bool spsReceived_;
  bool ppsReceived_;

  uint32_t sessionID_;

  int threads_;

  // temporarily store frame info during decoding
  std::deque<std::unique_ptr<Data>> decodingFrames_;
};
