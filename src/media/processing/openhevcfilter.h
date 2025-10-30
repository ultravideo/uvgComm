#pragma once
#include "filter.h"

#include "openHevcWrapper.h"
#include <utility>

class OpenHEVCFilter : public Filter
{
public:
  OpenHEVCFilter(uint32_t sessionID, QString cname, StatisticsInterface* stats,
                 std::shared_ptr<ResourceAllocator> hwResources);

  virtual bool init();
  void uninit();

  virtual void updateSettings();

protected:
  virtual void process();

private:

  void sendDecodedOutput(int &gotPicture);

  OpenHevc_Handle handle_;

  bool vpsReceived_;
  bool spsReceived_;
  bool ppsReceived_;

  uint32_t sessionID_;
  QString cname_;

  int threads_;
  QString parallelizationMode_;

  // temporarily store frame info during decoding together with any extra
  // compressed bytes (VPS/SPS/PPS/SEI/etc.) that arrived outside VCL NALs.
  std::deque<std::pair<std::unique_ptr<Data>, uint32_t>> decodingFrames_;

  QMutex settingsMutex_;

  uint32_t discardedFrames_;
  // accumulator for non-VCL bytes seen between VCL frames
  uint32_t pendingParamSetBytes_;
};
