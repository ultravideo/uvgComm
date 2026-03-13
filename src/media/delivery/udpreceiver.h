#pragma once

#include "media/processing/filter.h"
#include <map>
#include <mutex>

class UDPReceiver : public Filter
{
public:
  UDPReceiver(QString id, StatisticsInterface *stats,
              std::shared_ptr<ResourceAllocator> hwResources);

protected:

  void process();

public:
  // Pending forwarding actions keyed by out-connection index.
  struct ForwardingStatus
  {
    enum ActionType {NONE = 0, PENDING_STOP, PAUSED, PENDING_START, FORWARDING};
    ActionType action = NONE;
    uint32_t rtpTimestamp = 0;
  };

  // Called by SFU control to request stopping forwarding to a specific
  // out-connection index at the given RTP timestamp boundary.
  void requestStopForwardingForIndex(int outIndex, uint32_t rtpTimestamp);

  // Request starting (resuming) forwarding to a specific out-connection
  // index at the given RTP timestamp boundary.
  void requestStartForwardingForIndex(int outIndex, uint32_t rtpTimestamp);

private:
  std::mutex pendingMutex_;
  std::map<int, ForwardingStatus> pendingActions_;

};
