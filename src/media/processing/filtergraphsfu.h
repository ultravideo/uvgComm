#pragma once

#include "filtergraph.h"
#include <map>
#include <utility>

class FilterGraphSFU : public FilterGraph
{
public:
  FilterGraphSFU();

  virtual void uninit();

  // localSSRC here is the SSRC of the destination client, the one we are sending to
  virtual void sendVideoto(uint32_t sessionID,
                           std::shared_ptr<Filter> sender,
                           uint32_t localSSRC,
                           const std::vector<uint32_t>& remoteSSRCs,
                           const std::vector<QString>& remoteCNAMEs,
                           bool isP2P, std::pair<uint16_t, uint16_t> resolution);

  virtual void receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> receiver,
                                VideoInterface *view,
                                uint32_t remoteSSRC,
                                QString cname);

  virtual void sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> sender,
                           uint32_t localSSRC);
  virtual void receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> receiver,
                                uint32_t remoteSSRC, QString cname);

public slots:
  // Handle incoming RTCP APP messages forwarded from the relay/delivery layer.
  void handleRtcpAppPacket(uint32_t senderSsrc, uint32_t targetSsrc, uint32_t rtpTimestamp, QString appName, uint8_t subtype);

protected:

  virtual void lastPeerRemoved();

  // Map (publisherSSRC, targetSSRC) -> out-connection index on the receiver
  std::map<std::pair<uint32_t, uint32_t>, int> outConnectionIndexMap_;

};
