#include "filtergraphsfu.h"

#include "filter.h"
#include "logger.h"


FilterGraphSFU::FilterGraphSFU() : FilterGraph()
{}


void FilterGraphSFU::uninit()
{
  quitting_ = true;
  removeAllParticipants();
}

void FilterGraphSFU::sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> sender,
                                 uint32_t localSSRC, uint32_t remoteSSRC,
                                 const QString& remoteCNAME, bool isP2P)
{
  checkParticipant(sessionID);

  // check if the participant is already in the graph
  if (!peers_[sessionID]->videoSenders.empty())
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant video sender already exists");
    return;
  }

  peers_.at(sessionID)->videoSenders[localSSRC] =sender;

  // find the other participant whose stream is supposed to connected to this sender
  for (auto& peer : peers_)
  {
    // no need to participants video to themselves
    if (peer.first != sessionID && peer.second != nullptr)
    {
      // check if the other participant has a receiver (they should)
      if (!peer.second->videoReceivers.empty() && !peer.second->videoReceivers.begin()->second->empty())
      {
        Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Connecting receiver to newly created sender",
                                        {"Sender SSRC"},
                                        {QString::number(localSSRC)});

        connectFilters(peer.second->videoReceivers.begin()->second->at(0), sender);
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
                                      uint32_t remoteSSRC)
{
  Q_UNUSED(view);

  checkParticipant(sessionID);

  // check if the participant is already in the graph
  if (!peers_[sessionID]->videoReceivers.empty())
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant receiver already exists");
    return;
  }

  // create a receiver and connect it
  auto receiveGraph = std::make_shared<std::vector<std::shared_ptr<Filter>>>
      (std::vector<std::shared_ptr<Filter>>{receiver});

  // add the participant to the graph
  peers_.at(sessionID)->videoReceivers[remoteSSRC] = receiveGraph;

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

  peers_.at(sessionID)->audioSenders[localSSRC] = sender;

  // find the other participant whose stream is supposed to connected to this sender
  for (auto& peer : peers_)
  {
    // no need to participants audio to themselves
    if (peer.first != sessionID && peer.second != nullptr)
    {
      // check if the other participant has a receiver (they should)
      if (!peer.second->audioReceivers.empty() && !peer.second->audioReceivers.begin()->second->empty())
      {
        Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Connecting receiver to newly created sender",
                                        {"Sender SSRC"},
                                        {QString::number(localSSRC)});

        connectFilters(peer.second->audioReceivers.begin()->second->at(0), sender);
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

void FilterGraphSFU::
    receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> receiver, uint32_t remoteSSRC)
{
  checkParticipant(sessionID);

  // check if the participant is already in the graph
  if (!peers_[sessionID]->audioReceivers.empty())
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant receiver already exists");
    return;
  }

  auto receiveGraph = std::make_shared<std::vector<std::shared_ptr<Filter>>>
      (std::vector<std::shared_ptr<Filter>>{receiver});

  // add the participant to the graph
  peers_.at(sessionID)->audioReceivers[remoteSSRC] = receiveGraph;

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
