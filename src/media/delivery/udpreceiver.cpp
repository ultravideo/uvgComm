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
    // handle the APP forwarding stopping and starting for RTP packets
    if (input->type == DT_RTP && input->data && input->data_size >= 12 && input->rtpTimestamp != 0)
    {
      uint32_t pkt_ssrc = 0; // get packet SSRC
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
          const uint32_t stopTs = act.rtpTimestamp;
          // Use signed arithmetic to decide whether pkt_ts has reached
          // or passed stopTs, while still handling wrap-around.
          const int32_t delta = static_cast<int32_t>(input->rtpTimestamp - stopTs);
          if (delta >= 0)
          {
            setOutputStatus(outIndex, false);
            Logger::getLogger()->printNormal(this, "Applying STOP forwarding action for outIndex",
                                            {"outIndex","rtpTimestamp","packetTs","ssrc"},
                                            {QString::number(outIndex), QString::number(stopTs),
                                             QString::number(input->rtpTimestamp), QString::number(pkt_ssrc)});
            act.action = ForwardingStatus::PAUSED;
          }
        }
        else if (act.action == ForwardingStatus::PENDING_START)
        {
          const uint32_t startTs = act.rtpTimestamp;
          const int32_t delta = static_cast<int32_t>(input->rtpTimestamp - startTs);
          if (delta >= 0)
          {
            setOutputStatus(outIndex, true);
            Logger::getLogger()->printNormal(this, "Applying START forwarding action for outIndex",
                                            {"outIndex","rtpTimestamp","packetTs","ssrc"},
                                            {QString::number(outIndex), QString::number(startTs),
                                             QString::number(input->rtpTimestamp), QString::number(pkt_ssrc)});
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
