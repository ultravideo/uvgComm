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
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant video sender already exists");
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
        Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Connecting receiver to newly created sender",
                                        {"Sender SSRC"},
                                        {QString::number(remoteSSRCs.at(0))});

        connectFilters(peer.second->videoReceivers.begin()->second, sender);
      }
      else
      {
        Logger::getLogger()->printDebug(DEBUG_WARNING, this, "No receiver found for participant",
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
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant receiver already exists");
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
        connectFilters(receiver, peer.second->videoSenders.begin()->second);
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
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant audio sender already in graph");
    return;
  }

  peers_.at(sessionID)->audioSenders[localSSRC] = sender; // TODO: This is not correct, should be remote SSRC

  // find the other participant whose stream is supposed to connected to this sender
  for (auto& peer : peers_)
  {
    // no need to participants audio to themselves
    if (peer.first != sessionID && peer.second != nullptr)
    {
      // check if the other participant has a receiver (they should)
      if (!peer.second->audioReceivers.empty())
      {
        Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Connecting receiver to newly created sender",
                                        {"Sender SSRC"},
                                        {QString::number(localSSRC)});

        connectFilters(peer.second->audioReceivers.begin()->second, sender);
      }
      else
      {
        Logger::getLogger()->printDebug(DEBUG_WARNING, this, "No receiver found for participant",
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
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant receiver already exists");
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
    Logger::getLogger()->printDebug(DEBUG_WARNING, this, "RTCP APP for unknown sender SSRC",
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
    Logger::getLogger()->printDebug(DEBUG_WARNING, this, "RTCP APP for unknown target SSRC",
                                    {"TargetSSRC"}, {QString::number(targetSsrc)});
    return;
  }

  int idx = receiver->getOutConnectionIndex(senderFilter);
  if (idx < 0)
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, this, "Could not find connection from receiver to sender",
                                    {"SenderSSRC", "TargetSSRC"}, {QString::number(senderSsrc), QString::number(targetSsrc)});
    return;
  }

  // Try to cast to UDPReceiver (SFU-side receiver) to register pending stop/start by connection index.
  UDPReceiver* rcv = dynamic_cast<UDPReceiver*>(receiver.get());
  if (!rcv)
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, this, "Receiver is not a UDPReceiver when handling RTCP APP");
    return;
  }

  if (appName == "STOP")
  {
    rcv->requestStopForwardingForIndex(idx, rtpTimestamp);
  }
  else if (appName == "STRT")
  {
    rcv->requestStartForwardingForIndex(idx, rtpTimestamp);
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, this, "Unknown RTCP APP name", {"app"}, {appName});
  }
}
