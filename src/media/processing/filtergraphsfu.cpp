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

void FilterGraphSFU::sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> videoFramedSource,
                                 const MediaID &id)
{
  checkParticipant(sessionID);

  // check if the participant is already in the graph
  for (auto& mediaID : peers_.at(sessionID)->receivingStreams)
  {
    if (mediaID == id)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant already in graph");
      return;
    }
  }

  peers_.at(sessionID)->sendingStreams.push_back(id);
  peers_.at(sessionID)->videoSenders.push_back(videoFramedSource);

  // connect the sender to receivers
  for (auto& videoReceiver : peers_.at(sessionID)->videoReceivers)
  {
    connectFilters(videoReceiver->at(0), videoFramedSource);
  }
}


void FilterGraphSFU::receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> videoSink,
                                      VideoInterface *view, const MediaID &id)
{
  Q_UNUSED(view);

  checkParticipant(sessionID);

  // check if the participant is already in the graph
  for (auto& mediaID : peers_.at(sessionID)->receivingStreams)
  {
    if (mediaID == id)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant already in graph");
      return;
    }
  }

  auto receiveGraph = std::make_shared<std::vector<std::shared_ptr<Filter>>>
      (std::vector<std::shared_ptr<Filter>>{videoSink});

  // add the participant to the graph
  peers_.at(sessionID)->receivingStreams.push_back(id);
  peers_.at(sessionID)->videoReceivers.push_back(receiveGraph);
}


void FilterGraphSFU::sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> audioFramedSource,
                                 const MediaID &id)
{
  checkParticipant(sessionID);

  // check if the participant is already in the graph
  for (auto& mediaID : peers_.at(sessionID)->receivingStreams)
  {
    if (mediaID == id)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant already in graph");
      return;
    }
  }

  peers_.at(sessionID)->sendingStreams.push_back(id);
  peers_.at(sessionID)->audioSenders.push_back(audioFramedSource);

  // connect the sender to receivers
  for (auto& audioReceiver : peers_.at(sessionID)->audioReceivers)
  {
    connectFilters(audioReceiver->at(0), audioFramedSource);
  }
}


void FilterGraphSFU::receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> audioSink,
                                      const MediaID &id)
{
  // check if the participant is already in the graph
  for (auto& mediaID : peers_.at(sessionID)->receivingStreams)
  {
    if (mediaID == id)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant already in graph");
      return;
    }
  }

  auto receiveGraph = std::make_shared<std::vector<std::shared_ptr<Filter>>>
      (std::vector<std::shared_ptr<Filter>>{audioSink});

  // add the participant to the graph
  peers_.at(sessionID)->receivingStreams.push_back(id);
  peers_.at(sessionID)->audioReceivers.push_back(receiveGraph);
}


void FilterGraphSFU::addParticipantToSFU(uint32_t sessionID,
                         std::pair<std::shared_ptr<Filter>, MediaID>& videoReceiver,
                         std::pair<std::shared_ptr<Filter>, MediaID>& audioReceiver,
                         std::vector<std::pair<std::shared_ptr<Filter>, MediaID>>& videoSenders,
                         std::vector<std::pair<std::shared_ptr<Filter>, MediaID>>& audioSenders,
                         VideoInterface* view)
{
  checkParticipant(sessionID);

  // check if the participant is already in the graph
  for (auto& mediaID : peers_.at(sessionID)->receivingStreams)
  {
    if (mediaID == videoReceiver.second)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Participant already in graph");
      return;
    }
  }

  receiveVideoFrom(sessionID, videoReceiver.first, view, videoReceiver.second);
  receiveAudioFrom(sessionID, audioReceiver.first, audioReceiver.second);

  for (auto& sender : videoSenders)
  {
    sendVideoto(sessionID, sender.first, sender.second);
  }

  for (auto& sender : audioSenders)
  {
    sendAudioTo(sessionID, sender.first, sender.second);
  }
}


void FilterGraphSFU::lastPeerRemoved()
{
  // nothing to do
}
