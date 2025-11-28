#include "filtergraphsfu.h"

#include "filter.h"
#include "logger.h"
#include "../delivery/udpreceiver.h"


FilterGraphSFU::FilterGraphSFU() : FilterGraph()
{}


void FilterGraphSFU::uninit()
{
  quitting_ = true;
  removeAllParticipants();
}

void FilterGraphSFU::sendVideoto(uint32_t sessionID,
                                 std::shared_ptr<Filter> sender,
                                 uint32_t localSSRC,
                                 const std::vector<uint32_t>& remoteSSRCs,
                                 const std::vector<QString>& remoteCNAMEs,
                                 bool isP2P,
                                 std::pair<uint16_t, uint16_t> resolution)
{
  checkParticipant(sessionID);

  // check if the participant is already in the graph
  if (!peers_[sessionID]->videoSenders.empty())
  {
    Logger::getLogger()->printNormal(this, "Participant video sender already exists");
    return;
  }

  peers_.at(sessionID)->videoSenders[remoteSSRCs.at(0)] = sender;

  // find the other participant whose stream is supposed to connected to this sender
  for (auto& peer : peers_)
  {
    // no need to participants video to themselves
    if (peer.first != sessionID && peer.second != nullptr)
    {
      // check if the other participant has a receiver (they should)
      if (!peer.second->videoReceivers.empty())
      {
        Logger::getLogger()->printNormal(this, "Connecting receiver to newly created sender",
                                        {"Sender SSRC"},
                                        {QString::number(remoteSSRCs.at(0))});

        connectFilters(peer.second->videoReceivers.begin()->second, sender);
        // Record mapping: publisherSSRC = key in videoReceivers map, targetSSRC = localSSRC
        // Find the receiver's SSRC key
        uint32_t publisherSsrc = 0;
        for (auto &vr : peer.second->videoReceivers)
        {
          if (vr.second.get() == peer.second->videoReceivers.begin()->second.get())
          {
            publisherSsrc = vr.first;
            break;
          }
        }
        if (publisherSsrc != 0)
        {
          int idx = peer.second->videoReceivers.begin()->second->getOutConnectionIndex(sender);
          outConnectionIndexMap_[{publisherSsrc, remoteSSRCs.at(0)}] = idx;
          Logger::getLogger()->printNormal(this, "Recorded SFU out-connection mapping when adding a sender",
                                          {"publisher","target","outIndex"},
                                          {QString::number(publisherSsrc), QString::number(remoteSSRCs.at(0)), QString::number(idx)});
        }
        // Also connect any RTCP-only receivers from this participant to the new sender
        if (!peer.second->rtcpReceivers.empty())
        {
          for (auto &rr : peer.second->rtcpReceivers)
          {
            connectFilters(rr.second, sender);
          }
        }
      }
      else
      {
        Logger::getLogger()->printWarning(this, "No receiver found for participant",
                                        {"Participant"},
                                        {QString::number(peer.first)});
      }
    }
  }

  sender->start();
}


void FilterGraphSFU::receiveVideoFrom(uint32_t sessionID,
                                      std::shared_ptr<Filter> receiver,
                                      VideoInterface* view,
                                      uint32_t remoteSSRC,
                                      QString cname)
{
  Q_UNUSED(view);
  Q_UNUSED(cname);

  checkParticipant(sessionID);

  // check if the participant is already in the graph
  if (!peers_[sessionID]->videoReceivers.empty())
  {
    Logger::getLogger()->printNormal(this, "Participant receiver already exists");
    return;
  }

  // add the participant to the graph
  peers_.at(sessionID)->videoReceivers[remoteSSRC] = receiver;

  // connect the receiver to existing senders to distribute this new media
  for (auto& peer : peers_)
  {
    if (peer.first != sessionID && peer.second != nullptr)
    {
      if (!peer.second->videoSenders.empty())
      {
        auto sender = peer.second->videoSenders.begin()->second;
        connectFilters(receiver, sender);

        // Record mapping: publisherSSRC = remoteSSRC (parameter), targetSSRC = key in videoSenders
        uint32_t targetSsrc = peer.second->videoSenders.begin()->first;
        int idx = receiver->getOutConnectionIndex(sender);
        outConnectionIndexMap_[{remoteSSRC, targetSsrc}] = idx;
        Logger::getLogger()->printNormal(this, "Recorded SFU out-connection mapping when adding a receiver",
                                        {"publisher","target","outIndex"},
                                        {QString::number(remoteSSRC), QString::number(targetSsrc), QString::number(idx)});
      }
    }
  }

  receiver->start();
}


void FilterGraphSFU::receiveRTCPFrom(uint32_t sessionID, std::shared_ptr<Filter> receiver,
                                     uint32_t remoteSSRC, QString cname)
{
  Q_UNUSED(cname);

  checkParticipant(sessionID);

  if (!peers_[sessionID]->rtcpReceivers.empty())
  {
    Logger::getLogger()->printNormal(this, "Participant RTCP receiver already exists");
    return;
  }

  peers_.at(sessionID)->rtcpReceivers[remoteSSRC] = receiver;

  // Connect this RTCP-only receiver to existing senders so it follows the
  // same topology as the RTP receiver. Do not record outConnectionIndexMap_
  // entries so APP control messages do not affect this receiver.
  for (auto& peer : peers_)
  {
    if (peer.first != sessionID && peer.second != nullptr)
    {
      if (!peer.second->videoSenders.empty())
      {
        auto sender = peer.second->videoSenders.begin()->second;
        connectFilters(receiver, sender);
      }
    }
  }

  receiver->start();
}


void FilterGraphSFU::sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> sender,
                                 uint32_t localSSRC)
{
  checkParticipant(sessionID);

  // check if the participant is already in the graph
  if (!peers_[sessionID]->audioSenders.empty())
  {
    Logger::getLogger()->printNormal(this, "Participant audio sender already in graph");
    return;
  }

  peers_.at(sessionID)->audioSenders[localSSRC] = sender; // TODO: this is probably incorrect, it should be remote SSRC

  // find the other participant whose stream is supposed to connected to this sender
  for (auto& peer : peers_)
  {
    // no need to participants audio to themselves
    if (peer.first != sessionID && peer.second != nullptr)
    {
      // check if the other participant has a receiver (they should)
      if (!peer.second->audioReceivers.empty())
      {
        Logger::getLogger()->printNormal(this, "Connecting receiver to newly created sender",
                                        {"Sender SSRC"},
                                        {QString::number(localSSRC)});

        connectFilters(peer.second->audioReceivers.begin()->second, sender);
        // Also connect any RTCP-only receivers from this participant to the new audio sender
        if (!peer.second->rtcpReceivers.empty())
        {
          for (auto &rr : peer.second->rtcpReceivers)
          {
            connectFilters(rr.second, sender);
          }
        }
      }
      else
      {
        Logger::getLogger()->printWarning(this, "No receiver found for participant",
                                        {"Participant"},
                                        {QString::number(peer.first)});
      }
    }
  }

  sender->start();
}


void FilterGraphSFU::receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> receiver,
                                      uint32_t remoteSSRC, QString cname)
{
  Q_UNUSED(cname);

  checkParticipant(sessionID);

  // check if the participant is already in the graph
  if (!peers_[sessionID]->audioReceivers.empty())
  {
    Logger::getLogger()->printNormal(this, "Participant receiver already exists");
    return;
  }

  // add the participant to the graph
  peers_.at(sessionID)->audioReceivers[remoteSSRC] = receiver;

  // connect the receiver to existing senders to distribute this new media
  for (auto& peer : peers_)
  {
    if (peer.first != sessionID && peer.second != nullptr)
    {
      if (!peer.second->audioSenders.empty())
      {
        connectFilters(receiver, peer.second->audioSenders.begin()->second);
      }
    }
  }

  // receiver is connected to senders later when the other call are renegotiated
  receiver->start();
}


void FilterGraphSFU::lastPeerRemoved()
{
  // nothing to do
}


void FilterGraphSFU::handleRtcpAppPacket(uint32_t senderSsrc, uint32_t targetSsrc, uint32_t rtpTimestamp, QString appName, uint8_t subtype)
{
  Q_UNUSED(subtype);

  if (senderSsrc == targetSsrc)
  {
    Logger::getLogger()->printWarning(this, "RTCP APP sender SSRC is the same as target SSRC, ignoring",
                                    {"SSRC"}, {QString::number(senderSsrc)});
    return;
  }

  // Find the receiver for the publishing SSRC
  std::shared_ptr<Filter> receiver = nullptr;
  for (auto &p : peers_)
  {
    if (p.second == nullptr) continue;
    if (p.second->videoReceivers.find(senderSsrc) != p.second->videoReceivers.end())
    {
      receiver = p.second->videoReceivers.at(senderSsrc);
      break;
    }
  }

  if (!receiver)
  {
    Logger::getLogger()->printWarning(this, "RTCP APP for unknown sender SSRC",
                                    {"SenderSSRC"}, {QString::number(senderSsrc)});
    return;
  }

  // Find the sender filter for the target SSRC
  std::shared_ptr<Filter> senderFilter = nullptr;
  for (auto &p : peers_)
  {
    if (p.second == nullptr) continue;
    if (p.second->videoSenders.find(targetSsrc) != p.second->videoSenders.end())
    {
      senderFilter = p.second->videoSenders.at(targetSsrc);
      break;
    }
  }

  if (!senderFilter)
  {
    Logger::getLogger()->printWarning(this, "RTCP APP for unknown target SSRC",
                                    {"TargetSSRC"}, {QString::number(targetSsrc)});
    return;
  }

  // First try the explicit mapping (publisherSSRC,targetSSRC) -> outIndex
  int idx = -1;
  auto key = std::make_pair(senderSsrc, targetSsrc);
  if (outConnectionIndexMap_.find(key) != outConnectionIndexMap_.end())
  {
    idx = outConnectionIndexMap_[key];
    Logger::getLogger()->printNormal(this, "Found mapping for RTCP APP",
                                    {"publisher","target","outIndex"},
                                    {QString::number(senderSsrc), QString::number(targetSsrc), QString::number(idx)});
  }
  else
  {
    // Fallback: directly query the receiver for this connection
    idx = receiver->getOutConnectionIndex(senderFilter);
    if (idx >= 0)
    {
      Logger::getLogger()->printWarning(this, "Used fallback index lookup for RTCP APP",
                                      {"publisher","target","outIndex"},
                                      {QString::number(senderSsrc), QString::number(targetSsrc), QString::number(idx)});
    }
  }

  if (idx < 0)
  {
    Logger::getLogger()->printWarning(this, "Could not find connection from receiver to sender",
                                    {"SenderSSRC", "TargetSSRC"}, {QString::number(senderSsrc), QString::number(targetSsrc)});
    return;
  }

  // Try to cast to UDPReceiver (SFU-side receiver) to register pending stop/start by connection index.
  UDPReceiver* rcv = dynamic_cast<UDPReceiver*>(receiver.get());
  if (!rcv)
  {
    Logger::getLogger()->printWarning(this, "Receiver is not a UDPReceiver when handling RTCP APP");
    return;
  }

  if (appName == "STOP")
  {
    Logger::getLogger()->printNormal(this, "RTCP APP STOP received, scheduling stop-forwarding",
                                    {"sender","target","outIndex","rtpTimestamp"},
                                    {QString::number(senderSsrc), QString::number(targetSsrc), QString::number(idx), QString::number(rtpTimestamp)});
    rcv->requestStopForwardingForIndex(idx, rtpTimestamp);
  }
  else if (appName == "STRT")
  {
    Logger::getLogger()->printNormal(this, "RTCP APP STRT received, scheduling start-forwarding",
                                    {"sender","target","outIndex","rtpTimestamp"},
                                    {QString::number(senderSsrc), QString::number(targetSsrc), QString::number(idx), QString::number(rtpTimestamp)});
    rcv->requestStartForwardingForIndex(idx, rtpTimestamp);
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Unknown RTCP APP name", {"app"}, {appName});
  }
}
