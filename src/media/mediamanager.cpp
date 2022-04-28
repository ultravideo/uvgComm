#include "mediamanager.h"

#include "media/processing/filtergraph.h"
#include "media/processing/filter.h"
#include "media/delivery/delivery.h"
#include "ui/gui/videoviewfactory.h"
#include "initiation/negotiation/sdptypes.h"
#include "statisticsinterface.h"

#include "resourceallocator.h"

#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QHostAddress>
#include <QtEndian>
#include <QSettings>


MediaManager::MediaManager():
  stats_(nullptr),
  fg_(new FilterGraph()),
  streamer_(nullptr)
{}


MediaManager::~MediaManager()
{
  fg_->running(false);
  fg_->uninit();
}


void MediaManager::init(std::shared_ptr<VideoviewFactory> viewfactory,
                        StatisticsInterface *stats)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Initiating");
  viewfactory_ = viewfactory;
  stats_ = stats;
  streamer_ = std::unique_ptr<Delivery> (new Delivery());

  connect(
    streamer_.get(),
    &Delivery::handleZRTPFailure,
    this,
    &MediaManager::handleZRTPFailure);

  connect(
    streamer_.get(),
    &Delivery::handleNoEncryption,
    this,
    &MediaManager::handleNoEncryption);

  std::shared_ptr<ResourceAllocator> hwResources =
      std::shared_ptr<ResourceAllocator>(new ResourceAllocator());

  fg_->init(viewfactory_->getSelfVideos(), stats, hwResources);
  streamer_->init(stats_, hwResources);

  QObject::connect(this, &MediaManager::updateVideoSettings,
                   fg_.get(), &FilterGraph::updateVideoSettings);

  QObject::connect(this, &MediaManager::updateAudioSettings,
                   fg_.get(), &FilterGraph::updateAudioSettings);

  QObject::connect(this, &MediaManager::updateAutomaticSettings,
                   fg_.get(), &FilterGraph::updateAutomaticSettings);
}


void MediaManager::uninit()
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Closing");
  // first filter graph, then streamer because of the rtpfilters
  fg_->running(false);
  fg_->uninit();

  stats_ = nullptr;
  if (streamer_ != nullptr)
  {
    streamer_->uninit();
    streamer_ = nullptr;
  }
}


void MediaManager::addParticipant(uint32_t sessionID,
                                  std::shared_ptr<SDPMessageInfo> peerInfo,
                                  const std::shared_ptr<SDPMessageInfo> localInfo)
{
  // TODO: support stop-time and start-time as recommended by RFC 4566 section 5.9

  Q_ASSERT(peerInfo->media.size() == localInfo->media.size());
  if (peerInfo->media.size() != localInfo->media.size() || peerInfo->media.empty())
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "Media manager",
               "addParticipant, invalid SDPs",
                {"LocalInfo", "PeerInfo"},
                {QString::number(localInfo->media.size()),
                 QString::number(peerInfo->media.size())});
    return;
  }

  if(peerInfo->timeDescriptions.at(0).startTime != 0 ||
     localInfo->timeDescriptions.at(0).startTime != 0)
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "Nonzero start-time not supported!");
    return;
  }

  if (getMediaNettype(peerInfo, 0) == "IN")
  {
    if(!streamer_->addPeer(sessionID,
                           getMediaAddrtype(peerInfo, 0),
                           getMediaAddress(peerInfo, 0),
                           getMediaAddrtype(localInfo, 0),
                           getMediaAddress(localInfo, 0)))
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                 "Error creating RTP peer. Simultaneous destruction?");
      return;
    }
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "What are we using if not the internet!?");
    return;
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Start creating media.", 
                                  {"Outgoing media", "Incoming media"},
                                  {QString::number(peerInfo->media.size()), 
                                   QString::number(localInfo->media.size())});

  if (stats_ != nullptr)
  {
    sdpToStats(sessionID, peerInfo, false);
    sdpToStats(sessionID, localInfo, true);
  }

  // create each agreed media stream
  for(int i = 0; i < peerInfo->media.size(); ++i)  {
    // TODO: I don't like that we match
    createOutgoingMedia(sessionID,
                        localInfo->media.at(i),
                        getMediaAddress(peerInfo, i),
                        peerInfo->media.at(i));
  }

  // TODO: videoID should be gotten from somewhere instead of guessed
  uint32_t videoID = 0;
  for (int i = 0; i < localInfo->media.size(); ++i)
  {
    createIncomingMedia(sessionID, localInfo->media.at(i),
                        getMediaAddress(localInfo, i),
                        peerInfo->media.at(i), videoID);

    if (localInfo->media.at(i).type == "video" )
    {
      ++videoID;
    }
  }

  // TODO: crashes at the moment.
  //fg_->print();
}


void MediaManager::createOutgoingMedia(uint32_t sessionID,
                                       const MediaInfo& localMedia,
                                       QString peerAddress,
                                       const MediaInfo& remoteMedia)
{
  if (peerAddress == "")
  {
    Logger::getLogger()->printProgramError(this, "Address was empty when creating outgoing media");
    return;
  }

  bool send = true;
  bool recv = true;

  transportAttributes(remoteMedia.flagAttributes, send, recv);

  // if they want to receive
  if(recv && remoteMedia.receivePort != 0)
  {
    Q_ASSERT(remoteMedia.receivePort);
    Q_ASSERT(!remoteMedia.rtpNums.empty());


    QString codec = rtpNumberToCodec(remoteMedia);

    if(remoteMedia.proto == "RTP/AVP")
    {
      std::shared_ptr<Filter> framedSource = streamer_->addSendStream(sessionID, peerAddress,
                                                                      localMedia.receivePort, remoteMedia.receivePort,
                                                                      codec, remoteMedia.rtpNums.at(0));

      Q_ASSERT(framedSource != nullptr);

      if(remoteMedia.type == "audio")
      {
        fg_->sendAudioTo(sessionID, std::shared_ptr<Filter>(framedSource));
      }
      else if(remoteMedia.type == "video")
      {
        fg_->sendVideoto(sessionID, std::shared_ptr<Filter>(framedSource));
      }
      else
      {
        Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Unsupported media type!",
                  {"type"}, QStringList() << remoteMedia.type);
      }
    }
    else
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "SDP transport protocol not supported.");
    }
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
               "Not creating media because they don't seem to want any according to attribute.");

    // TODO: Spec says we should still send RTCP
  }
}


void MediaManager::createIncomingMedia(uint32_t sessionID,
                                       const MediaInfo &localMedia,
                                       QString localAddress,
                                       const MediaInfo &remoteMedia, uint32_t videoID)
{
  if (localAddress == "")
  {
        Logger::getLogger()->printProgramError(this, "Address was empty when creating incoming media");
    return;
  }
  bool send = true;
  bool recv = true;

  transportAttributes(localMedia.flagAttributes, send, recv);
  if(recv)
  {
    Q_ASSERT(localMedia.receivePort);
    Q_ASSERT(!localMedia.rtpNums.empty());

    QString codec = rtpNumberToCodec(localMedia);

    if(localMedia.proto == "RTP/AVP")
    {
      std::shared_ptr<Filter> rtpSink = streamer_->addReceiveStream(sessionID, localAddress,
                                                                    localMedia.receivePort,
                                                                    remoteMedia.receivePort,
                                                                    codec, localMedia.rtpNums.at(0));
      Q_ASSERT(rtpSink != nullptr);
      if(localMedia.type == "audio")
      {
        fg_->receiveAudioFrom(sessionID, std::shared_ptr<Filter>(rtpSink));
      }
      else if(localMedia.type == "video")
      {
        VideoInterface *view = viewfactory_->getVideo(sessionID, videoID);
        Q_ASSERT(view);
        if (view != nullptr)
        {
          fg_->receiveVideoFrom(sessionID, std::shared_ptr<Filter>(rtpSink), view);
        }
        else {
          Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Failed to get view from viewFactory");
        }
      }
      else
      {
        Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Unsupported incoming media type!",
                  {"type"}, QStringList() << localMedia.type);
      }
    }
    else
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, 
                                      "Incoming SDP transport protocol not supported.");
    }
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
               "Not creating media because they don't seem to want any according to attribute.");
  }
}

void MediaManager::removeParticipant(uint32_t sessionID)
{
  fg_->removeParticipant(sessionID);
  streamer_->removePeer(sessionID);

  Logger::getLogger()->printDebug(DEBUG_NORMAL, "Media Manager", "Session media removed",
            {"SessionID"}, {QString::number(sessionID)});
}


QString MediaManager::rtpNumberToCodec(const MediaInfo& info)
{
  // If we are not using raw.
  // This is the place where all other preset audio video codec numbers should be set
  // but its unlikely that we will support any besides raw pcmu.
  if(info.rtpNums.at(0) != 0)
  {
    Q_ASSERT(!info.codecs.empty());
    if(!info.codecs.empty())
    {
      return info.codecs.at(0).codec;
    }
  }
  return "PCMU";
}


void MediaManager::transportAttributes(const QList<SDPAttributeType>& attributes, bool& send, bool& recv)
{
  send = true;
  recv = true;

  for(SDPAttributeType attribute : attributes)
  {
    if(attribute == A_SENDRECV)
    {
      send = true;
      recv = true;
    }
    else if(attribute == A_SENDONLY)
    {
      send = true;
      recv = false;
    }
    else if(attribute == A_RECVONLY)
    {
      send = false;
      recv = true;
    }
    else if(attribute == A_INACTIVE)
    {
      send = false;
      recv = false;
    }
  }
}


void MediaManager::sdpToStats(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> sdp, bool incoming)
{
  // TODO: This feels like a hack to do this here. Instead we should give stats the whole SDP
  QStringList ipList;
  QStringList audioPorts;
  QStringList videoPorts;

  // create each agreed media stream
  for (auto& media : sdp->media)
  {
    if (media.type == "audio")
    {
      audioPorts.append(QString::number(media.receivePort));
    }
    else if (media.type == "video")
    {
      videoPorts.append(QString::number(media.receivePort));
    }

    if (media.connection_address != "")
    {
      ipList.append(media.connection_address);
    }
    else
    {
      ipList.append(sdp->connection_address);
    }
  }

  if (stats_)
  {
    if (incoming)
    {
      stats_->incomingMedia(sessionID, sdp->originator_username, ipList, audioPorts, videoPorts);
    }
    else
    {
      stats_->outgoingMedia(sessionID, sdp->originator_username,ipList, audioPorts, videoPorts);
    }
  }
}


QString MediaManager::getMediaNettype(std::shared_ptr<SDPMessageInfo> sdp, int mediaIndex)
{
  if (sdp->media.size() >= mediaIndex && sdp->media.at(mediaIndex).connection_nettype != "")
  {
    return sdp->media.at(mediaIndex).connection_nettype;
  }
  return sdp->connection_nettype;
}


QString MediaManager::getMediaAddrtype(std::shared_ptr<SDPMessageInfo> sdp, int mediaIndex)
{
  if (sdp->media.size() >= mediaIndex && sdp->media.at(mediaIndex).connection_addrtype != "")
  {
    return sdp->media.at(mediaIndex).connection_addrtype;
  }
  return sdp->connection_addrtype;
}


QString MediaManager::getMediaAddress(std::shared_ptr<SDPMessageInfo> sdp, int mediaIndex)
{
  if (sdp->media.size() >= mediaIndex && sdp->media.at(mediaIndex).connection_address != "")
  {
    return sdp->media.at(mediaIndex).connection_address;
  }
  return sdp->connection_address;
}
