#include "udpreceiver.h"

#include "logger.h"

UDPReceiver::UDPReceiver(QString id, StatisticsInterface *stats,
                         std::shared_ptr<ResourceAllocator> hwResources):
    Filter(id, "UDPReceiver", stats, hwResources, DT_NONE, DT_RTP)
{
  maxBufferSize_ = 1000;
}


void UDPReceiver::process()
{
  std::unique_ptr<Data> input = getInput();

  while(input)
  {
    // If this is an RTP packet, extract the RTP timestamp and check for
    // any pending stop/start requests keyed by out-connection index. When
    // a pending boundary is reached we toggle the corresponding out-connection.
    if (input->type == DT_RTP && input->data && input->data_size >= 12)
    {
      // RTP timestamp is at bytes 4..7 in network byte order
      uint32_t net_ts = 0;
      memcpy(&net_ts, input->data.get() + 4, sizeof(net_ts));
      uint32_t pkt_ts = ntohl(net_ts);
      uint32_t pkt_ssrc = 0;
      if (input->data_size >= 12)
      {
        uint32_t net_ssrc = 0;
        memcpy(&net_ssrc, input->data.get() + 8, sizeof(net_ssrc));
        pkt_ssrc = ntohl(net_ssrc);
      }

      std::lock_guard<std::mutex> g(pendingMutex_);
      for (auto &entry : pendingActions_)
      {
        int outIndex = entry.first;
        ForwardingStatus &act = entry.second;

        if (act.action == ForwardingStatus::PENDING_STOP)
        {
          uint32_t stopTs = act.rtpTimestamp;
          uint32_t diff = pkt_ts - stopTs;
          if (diff < 0x80000000U)
          {
            // Reached or passed stop timestamp -> disable output
            setOutputStatus(outIndex, false);
            Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Applying STOP forwarding action for outIndex",
                                            {"outIndex","rtpTimestamp","packetTs","ssrc"},
                                            {QString::number(outIndex), QString::number(stopTs), QString::number(pkt_ts), QString::number(pkt_ssrc)});
            act.action = ForwardingStatus::PAUSED;
          }
        }
        else if (act.action == ForwardingStatus::PENDING_START)
        {
          uint32_t startTs = act.rtpTimestamp;
          uint32_t diff = pkt_ts - startTs;
          if (diff < 0x80000000U)
          {
            // Reached or passed start timestamp -> enable output
            setOutputStatus(outIndex, true);
            Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Applying START forwarding action for outIndex",
                                            {"outIndex","rtpTimestamp","packetTs","ssrc"},
                                            {QString::number(outIndex), QString::number(startTs), QString::number(pkt_ts), QString::number(pkt_ssrc)});
            act.action = ForwardingStatus::FORWARDING;
          }
        }
      }
    }

    // send data to filter graph
    sendOutput(std::move(input));
    input = getInput();
  }
}

void UDPReceiver::requestStopForwardingForIndex(int outIndex, uint32_t rtpTimestamp)
{
  std::lock_guard<std::mutex> g(pendingMutex_);
  ForwardingStatus a;
  a.action = ForwardingStatus::PENDING_STOP;
  a.rtpTimestamp = rtpTimestamp;
  pendingActions_[outIndex] = a;
}

void UDPReceiver::requestStartForwardingForIndex(int outIndex, uint32_t rtpTimestamp)
{
  std::lock_guard<std::mutex> g(pendingMutex_);
  ForwardingStatus a;
  a.action = ForwardingStatus::PENDING_START;
  a.rtpTimestamp = rtpTimestamp;
  pendingActions_[outIndex] = a;
}
