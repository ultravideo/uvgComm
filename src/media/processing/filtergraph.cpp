#include "filtergraph.h"

#include "logger.h"
#include "Filter.h"

#include "media/processing/libyuvconverter.h"
#include "media/processing/yuvtorgb32.h"



FilterGraph::FilterGraph():
  quitting_(false),
  stats_(nullptr),
  hwResources_(nullptr),
  peers_()
{}


void FilterGraph::init(StatisticsInterface* stats, std::shared_ptr<ResourceAllocator> hwResources)
{
  Q_ASSERT(stats);

  uninit();

  quitting_ = false;
  stats_ = stats;
  hwResources_ = hwResources;
}

void FilterGraph::running(bool state)
{
  for(auto& peer : peers_)
  {
    if(peer.second != nullptr)
    {
      for (auto& senderFilter : peer.second->audioSenders)
      {
        if(senderFilter)
        {
          changeState(senderFilter, state);
        }
      }

      for (auto& senderFilter : peer.second->videoSenders)
      {
        if(senderFilter)
        {
          changeState(senderFilter, state);
        }
      }

      for (auto& graph : peer.second->audioReceivers)
      {
        for(std::shared_ptr<Filter>& f : *graph)
        {
          changeState(f, state);
        }
      }

      for (auto& graph : peer.second->videoReceivers)
      {
        for(std::shared_ptr<Filter>& f : *graph)
        {
          changeState(f, state);
        }
      }
    }
  }
}

void FilterGraph::updateVideoSettings()
{
  // send and receive graphs for each individual peer
  for(auto& peer : peers_)
  {
    if(peer.second != nullptr)
    {
      for (auto& senderFilter : peer.second->videoSenders)
      {
        senderFilter->updateSettings();
      }

      // decode and display settings
      for(auto& videoReceivers : peer.second->videoReceivers)
      {
        for (auto& filter : *videoReceivers)
        {
          filter->updateSettings();
        }
      }
    }
  }
}


void FilterGraph::updateAudioSettings()
{
  for (auto& peer : peers_)
  {
    if (peer.second != nullptr)
    {
      for (auto& senderFilter : peer.second->audioSenders)
      {
        senderFilter->updateSettings();
      }

      for (auto& audioReceivers : peer.second->audioReceivers)
      {
        for (auto& filter : *audioReceivers)
        {
          filter->updateSettings();
        }
      }
    }
  }
}


void FilterGraph::checkParticipant(uint32_t sessionID)
{
  Q_ASSERT(stats_);
  Q_ASSERT(sessionID);

  if(peers_.find(sessionID) == peers_.end() || peers_[sessionID] == nullptr)
  {
    peers_[sessionID] = new Peer();
  }
}


bool FilterGraph::addToGraph(std::shared_ptr<Filter> filter,
                                GraphSegment &graph,
                                size_t connectIndex)
{
  // // check if we need some sort of conversion and connect to index
  if(graph.size() > 0 && connectIndex <= graph.size() - 1)
  {
    if(graph.at(connectIndex)->outputType() != filter->inputType())
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
                                      "Filter output and input do not match. Finding conversion",
                                      {"Connection"}, {graph.at(connectIndex)->getName() + "->" + filter->getName()});

      if (graph.at(connectIndex)->outputType() == DT_NONE)
      {
        Logger::getLogger()->printProgramError(this, "The previous filter has no output!");
        return false;
      }

      // TODO: Check the out connections of connected filter for an already existing conversion.

      if(filter->inputType() == DT_YUV420VIDEO)
      {
        Logger::getLogger()->printNormal(this, "Adding libyuv conversion filter to YUV420");
        addToGraph(std::shared_ptr<Filter>(new LibYUVConverter("", stats_, hwResources_,
                                                               graph.at(connectIndex)->outputType())),
                   graph, connectIndex);
      }
      else if(graph.at(connectIndex)->outputType() == DT_YUV420VIDEO &&
               filter->inputType() == DT_RGB32VIDEO)
      {
        Logger::getLogger()->printNormal(this, "Adding YUV420 to RGB32 conversion");
        addToGraph(std::shared_ptr<Filter>(new YUVtoRGB32("", stats_, hwResources_)),
                   graph, connectIndex);
      }
      else
      {
        Logger::getLogger()->printProgramError(this, "Could not find conversion for filter.");
        return false;
      }
      // the conversion filter has been added to the end
      connectIndex = (unsigned int)graph.size() - 1;
    }
    connectFilters(graph.at(connectIndex), filter);
  }

  graph.push_back(filter);
  if(filter->init())
  {
    filter->start();
  }
  else
  {
    Logger::getLogger()->printError(this, "Failed to init filter");
    return false;
  }
  return true;
}


bool FilterGraph::connectFilters(std::shared_ptr<Filter> previous, std::shared_ptr<Filter> filter)
{
  Q_ASSERT(filter != nullptr && previous != nullptr);

  Logger::getLogger()->printDebug(DEBUG_NORMAL, "FilterGraph", "Connecting filters",
                                  {"Connection"}, {previous->getName() + " -> " + filter->getName()});

  if(previous->outputType() != filter->inputType())
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, "FilterGraph",
                                    "The connecting filter output and input DO NOT MATCH.");
    return false;
  }
  previous->addOutConnection(filter);
  return true;
}


bool FilterGraph::existingConnection(std::vector<MediaID>& connections, MediaID id)
{
  for (auto& connection: connections)
  {
    if (connection == id)
    {
      return true;
    }
  }

  return false;
}


void FilterGraph::removeParticipant(uint32_t sessionID)
{
  if (peers_.find(sessionID) != peers_.end() &&
      peers_[sessionID] != nullptr)
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
                                    "Removing peer", {"SessionID", "Remaining sessions"},
                                    {QString::number(sessionID), QString::number(peers_.size())});

    destroyPeer(peers_[sessionID]);
    peers_[sessionID] = nullptr;

    // destroy send graphs if this was the last peer
    bool peerPresent = false;
    for(auto& peer : peers_)
    {
      if (peer.second != nullptr)
      {
        peerPresent = true;
      }
    }

    if(!peerPresent)
    {
      lastPeerRemoved();
    }
  }
}


void FilterGraph::removeAllParticipants()
{
  for(auto& peer : peers_)
  {
    if(peer.second != nullptr)
    {
      removeParticipant(peer.first);
    }
  }
  peers_.clear();
}


void FilterGraph::changeState(std::shared_ptr<Filter> f, bool state)
{
  if(state)
  {
    f->emptyBuffer();
    f->start();
  }
  else
  {
    f->stop();
    f->emptyBuffer();

    while(f->isRunning())
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}


void FilterGraph::destroyFilters(std::vector<std::shared_ptr<Filter> > &filters)
{
  if(filters.size() != 0)
  {
    Logger::getLogger()->printNormal(this, "Destroying filter in one graph",
                                     {"Filter"}, {QString::number(filters.size())});
  }
  for( std::shared_ptr<Filter>& f : filters )
  {
    changeState(f, false);
  }

  filters.clear();
}


void FilterGraph::destroyPeer(Peer* peer)
{
  Logger::getLogger()->printNormal(this, "Destroying peer from Filter Graph");

  for (auto& audioSender : peer->audioSenders)
  {
    changeState(audioSender, false);
    audioSender = nullptr;
  }
  for (auto& videoSender : peer->videoSenders)
  {
    changeState(videoSender, false);
    videoSender = nullptr;
  }
  for (auto& graph : peer->audioReceivers)
  {
    destroyFilters(*graph);
  }

  for (auto& graph : peer->videoReceivers)
  {
    destroyFilters(*graph);
  }

  delete peer;
}
