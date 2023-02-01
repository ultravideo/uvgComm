#include "mediamanager.h"

#include "media/processing/filtergraph.h"
#include "media/processing/filter.h"
#include "media/delivery/delivery.h"
#include "initiation/negotiation/sdptypes.h"
#include "statisticsinterface.h"

#include "resourceallocator.h"

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


void MediaManager::init(QList<VideoInterface*> selfViews,
                        StatisticsInterface *stats)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Initiating");
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

  fg_->init(selfViews, stats, hwResources);
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
                                  const std::shared_ptr<SDPMessageInfo> localInfo,
                                  VideoInterface* videoView, bool iceController)
{
  // TODO: support stop-time and start-time as recommended by RFC 4566 section 5.9
  if (!sessionChecks(peerInfo, localInfo))
  {
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

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Start creating media");

  if (participants_.find(sessionID) == participants_.end())
  {
    participants_[sessionID].ice = std::unique_ptr<ICE>(new ICE(sessionID));

    // connect signals so we get information when ice is ready
    QObject::connect(participants_[sessionID].ice.get(), &ICE::nominationSucceeded,
                     this, &MediaManager::iceSucceeded);

    QObject::connect(participants_[sessionID].ice.get(), &ICE::nominationFailed,
                     this, &MediaManager::iceFailed);
  }

  return modifyParticipant(sessionID, peerInfo, localInfo, videoView, iceController);
}


void MediaManager::modifyParticipant(uint32_t sessionID,
                                  std::shared_ptr<SDPMessageInfo> peerInfo,
                                  const std::shared_ptr<SDPMessageInfo> localInfo,
                                  VideoInterface* videoView, bool iceController)
{
  // TODO: support stop-time and start-time as recommended by RFC 4566 section 5.9
  if (!sessionChecks(peerInfo, localInfo))
  {
    return;
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Start creating media");

  if (stats_ != nullptr)
  {
    // TODO: Update SDP in stats, currently it just adds a new one
    //sdpToStats(sessionID, peerInfo, false);
    //sdpToStats(sessionID, localInfo, true);
  }

  // perform ICE
  if (!localInfo->candidates.empty() && !peerInfo->candidates.empty())
  {
    participants_[sessionID].localInfo = localInfo;
    participants_[sessionID].peerInfo = peerInfo;
    participants_[sessionID].videoView = videoView;
    participants_[sessionID].followOurSDP = iceController;

    participants_[sessionID].ice->startNomination(localInfo->media.count()*2,
                                                  localInfo->candidates,
                                                  peerInfo->candidates, iceController);
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Did not find any ICE candidates, not performing ICE");
    createCall(sessionID, peerInfo, localInfo, videoView, iceController);
  }
}


void MediaManager::createCall(uint32_t sessionID,
                std::shared_ptr<SDPMessageInfo> peerInfo,
                const std::shared_ptr<SDPMessageInfo> localInfo,
                VideoInterface* videoView, bool followOurSDP)
{
  // create each agreed media stream
  for(int i = 0; i < peerInfo->media.size(); ++i)  {
    // TODO: I don't like that we match indexes
    createOutgoingMedia(sessionID,
                        localInfo->media.at(i),
                        getMediaAddress(peerInfo, i),
                        peerInfo->media.at(i), followOurSDP);
  }

  for (int i = 0; i < localInfo->media.size(); ++i)
  {
    createIncomingMedia(sessionID, localInfo->media.at(i),
                        getMediaAddress(localInfo, i),
                        peerInfo->media.at(i), videoView, followOurSDP);
  }
}


void MediaManager::createOutgoingMedia(uint32_t sessionID,
                                       const MediaInfo& localMedia,
                                       QString peerAddress,
                                       const MediaInfo& remoteMedia,
                                       bool useOurSDP)
{
  if (peerAddress == "")
  {
    Logger::getLogger()->printProgramError(this, "Address was empty when creating outgoing media");
    return;
  }

  bool send = true;
  bool recv = true;
  if (useOurSDP)
  {
    transportAttributes(localMedia.flagAttributes, send, recv);
  }
  else
  {
    transportAttributes(remoteMedia.flagAttributes, send, recv);
  }

  // if we want to send
  if(send && remoteMedia.receivePort != 0)
  {
    Q_ASSERT(remoteMedia.receivePort);
    Q_ASSERT(!remoteMedia.rtpNums.empty());

    QString codec = rtpNumberToCodec(remoteMedia);

    if(remoteMedia.proto == "RTP/AVP")
    {
      std::shared_ptr<Filter> framedSource = streamer_->addSendStream(sessionID, peerAddress,
                                                                      localMedia.receivePort,
                                                                      remoteMedia.receivePort,
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
               "Not sending media according to attribute", {"Type"}, {localMedia.type});

    // TODO: Spec says we should still send RTCP if the port is not 0
  }
}


void MediaManager::createIncomingMedia(uint32_t sessionID,
                                       const MediaInfo &localMedia,
                                       QString localAddress,
                                       const MediaInfo &remoteMedia,
                                       VideoInterface* videoView,
                                       bool useOurSDP)
{
  if (localAddress == "")
  {
    Logger::getLogger()->printProgramError(this, "Address was empty when creating incoming media");
    return;
  }
  bool send = true;
  bool recv = true;

  if (useOurSDP)
  {
    transportAttributes(localMedia.flagAttributes, send, recv);
  }
  else
  {
    transportAttributes(remoteMedia.flagAttributes, send, recv);
  }

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
        Q_ASSERT(videoView);
        if (videoView != nullptr)
        {
          fg_->receiveVideoFrom(sessionID, std::shared_ptr<Filter>(rtpSink), videoView);
        }
        else
        {
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
                                    "Not receiving media according to attribute", {"Type"}, {localMedia.type});
  }
}

void MediaManager::removeParticipant(uint32_t sessionID)
{
  if (participants_.find(sessionID) != participants_.end())
  {
    participants_[sessionID].ice->uninit();
    participants_.erase(sessionID);
  }

  fg_->removeParticipant(sessionID);
  streamer_->removePeer(sessionID);

  Logger::getLogger()->printDebug(DEBUG_NORMAL, "Media Manager", "Session media removed",
            {"SessionID"}, {QString::number(sessionID)});
}

void MediaManager::iceSucceeded(QList<std::shared_ptr<ICEPair>>& streams,
                                quint32 sessionID)
{
  if (participants_.find(sessionID) == participants_.end())
  {
    Logger::getLogger()->printProgramError(this, "Could not find participant when ice finished");
    return;
  }

  Logger::getLogger()->printNormal(this, "ICE nomination has succeeded", {"SessionID"},
                                   {QString::number(sessionID)});

  // Video. 0 is RTP, 1 is RTCP
  if (streams.at(0) != nullptr && streams.at(1) != nullptr)
  {
    setMediaPair(participants_[sessionID].localInfo->media[1],  streams.at(0)->local, true);
    setMediaPair(participants_[sessionID].peerInfo->media[1], streams.at(0)->remote, false);
  }

  // Audio. 2 is RTP, 3 is RTCP
  if (streams.at(2) != nullptr && streams.at(3) != nullptr)
  {
    setMediaPair(participants_[sessionID].localInfo->media[0],  streams.at(2)->local, true);
    setMediaPair(participants_[sessionID].peerInfo->media[0], streams.at(2)->remote, false);
  }

  createCall(sessionID,
             participants_[sessionID].peerInfo,
             participants_[sessionID].localInfo,
             participants_[sessionID].videoView,
             participants_[sessionID].followOurSDP);
}


void MediaManager::iceFailed(uint32_t sessionID)
{
  Logger::getLogger()->printError(this, "ICE failed, removing participant");

  // the participant is removed later
  emit iceMediaFailed(sessionID);
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


void MediaManager::sdpToStats(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> sdp, bool local)
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
    if (local)
    {
      stats_->incomingMedia(sessionID, sdp->originator_username, ipList, audioPorts, videoPorts);
    }
    else
    {
      stats_->outgoingMedia(sessionID, sdp->originator_username,ipList, audioPorts, videoPorts);
    }
  }
}


void MediaManager::setMediaPair(MediaInfo& media, std::shared_ptr<ICEInfo> mediaInfo, bool local)
{
  if (mediaInfo == nullptr)
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "SDPNegotiationHelper",
                                    "Null mediainfo in setMediaPair");
    return;
  }

  // for local address, we bind to our rel-address if using non-host connection type
  if (local &&
      mediaInfo->type != "host" &&
      mediaInfo->rel_address != "" && mediaInfo->rel_port != 0)
  {
    media.connection_address = mediaInfo->rel_address;
    media.receivePort        = mediaInfo->rel_port;
  }
  else
  {
    media.connection_address = mediaInfo->address;
    media.receivePort        = mediaInfo->port;
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



bool MediaManager::sessionChecks(std::shared_ptr<SDPMessageInfo> peerInfo,
                   const std::shared_ptr<SDPMessageInfo> localInfo) const
{

  Q_ASSERT(peerInfo->media.size() == localInfo->media.size());
  if (peerInfo->media.size() != localInfo->media.size() || peerInfo->media.empty())
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "Media manager",
               "addParticipant, invalid SDPs",
                {"LocalInfo", "PeerInfo"},
                {QString::number(localInfo->media.size()),
                 QString::number(peerInfo->media.size())});
    return false;
  }

  if(peerInfo->timeDescriptions.at(0).startTime != 0 ||
     localInfo->timeDescriptions.at(0).startTime != 0)
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "Nonzero start-time not supported!");
    return false;
  }

  return true;
}
