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

void FilterGraphSFU::sendVideoto(uint32_t sessionID,
                                 std::shared_ptr<Filter> videoFramedSource,
                                 const MediaID &id)
{
  checkParticipant(sessionID);

  // check if the participant is already in the graph
  if (!peers_[sessionID]->videoSenders.empty())
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant video sender already exists");
    return;
  }

  peers_.at(sessionID)->videoSenders.push_back(videoFramedSource);

  // find the other participant whose stream is supposed to connected to this sender
  for (auto& peer : peers_)
  {
    // no need to participants video to themselves
    if (peer.first != sessionID)
    {
      // check if the other participant has a receiver (they should)
      if (!peer.second->videoReceivers.empty() && !peer.second->videoReceivers.at(0)->empty())
      {
        Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Connecting receiver to newly created sender",
                                        {"Sender SSRC"},
                                        {id.toString()});

        connectFilters(peer.second->videoReceivers.at(0)->at(0), videoFramedSource);
      }
      else
      {
        Logger::getLogger()->printDebug(DEBUG_WARNING, this, "No receiver found for participant",
                                        {"Participant"},
                                        {QString::number(peer.first)});
      }
    }
  }

  videoFramedSource->start();
}


void FilterGraphSFU::receiveVideoFrom(uint32_t sessionID,
                                      std::shared_ptr<Filter> videoSink,
                                      VideoInterface* view,
                                      const MediaID &id)
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
      (std::vector<std::shared_ptr<Filter>>{videoSink});

  // add the participant to the graph
  peers_.at(sessionID)->videoReceivers.push_back(receiveGraph);

  // connect the receiver to existing senders to distribute this new media
  for (auto& peer : peers_)
  {
    if (peer.first != sessionID)
    {
      if (!peer.second->videoSenders.empty())
      {
        connectFilters(videoSink, peer.second->videoSenders.at(0));
      }
    }
  }

  videoSink->start();
}

void FilterGraphSFU::sendAudioTo(uint32_t sessionID,
                                 std::shared_ptr<Filter> audioFramedSource,
                                 const MediaID &id)
{
  checkParticipant(sessionID);

  // check if the participant is already in the graph
  if (!peers_[sessionID]->audioSenders.empty())
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant audio sender already in graph");
    return;
  }

  peers_.at(sessionID)->audioSenders.push_back(audioFramedSource);

  // find the other participant whose stream is supposed to connected to this sender
  for (auto& peer : peers_)
  {
    // no need to participants audio to themselves
    if (peer.first != sessionID)
    {
      // check if the other participant has a receiver (they should)
      if (!peer.second->audioReceivers.empty() && !peer.second->audioReceivers.at(0)->empty())
      {
        Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Connecting receiver to newly created sender",
                                        {"Sender SSRC"},
                                        {id.toString()});

        connectFilters(peer.second->audioReceivers.at(0)->at(0), audioFramedSource);
      }
      else
      {
        Logger::getLogger()->printDebug(DEBUG_WARNING, this, "No receiver found for participant",
                                        {"Participant"},
                                        {QString::number(peer.first)});
      }
    }
  }

  audioFramedSource->start();
}


void FilterGraphSFU::receiveAudioFrom(uint32_t sessionID,
                                      std::shared_ptr<Filter> audioSink,
                                      const MediaID &id)
{
  checkParticipant(sessionID);

  // check if the participant is already in the graph
  if (!peers_[sessionID]->audioReceivers.empty())
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant receiver already exists");
    return;
  }

  auto receiveGraph = std::make_shared<std::vector<std::shared_ptr<Filter>>>
      (std::vector<std::shared_ptr<Filter>>{audioSink});

  // add the participant to the graph
  peers_.at(sessionID)->audioReceivers.push_back(receiveGraph);

  // connect the receiver to existing senders to distribute this new media
  for (auto& peer : peers_)
  {
    if (peer.first != sessionID)
    {
      if (!peer.second->audioSenders.empty())
      {
        connectFilters(audioSink, peer.second->audioSenders.at(0));
      }
    }
  }

  // receiver is connected to senders later when the other call are renegotiated
  audioSink->start();
}


void FilterGraphSFU::lastPeerRemoved()
{
  // nothing to do
}
