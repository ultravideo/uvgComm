#include "mediamanager.h"

#include "initiation/negotiation/sdptypes.h"
#include "media/delivery/delivery.h"
#include "media/processing/filter.h"
#include "media/processing/filtergraphclient.h"
#include "media/processing/filtergraphsfu.h"
#include "statisticsinterface.h"
#include "videoviewfactory.h"

#include "resourceallocator.h"
#include "settingskeys.h"

#include "logger.h"
#include "common.h"
#include "cname.h"

#include <QHostAddress>
#include <QtEndian>
#include <QSettings>


MediaManager::MediaManager():
  stats_(nullptr),
  clientFg_(new FilterGraphClient()),
  sfuFg_(new FilterGraphSFU()),
  streamer_(nullptr),
  localInitialIndex_(-1)
{}


MediaManager::~MediaManager()
{
  clientFg_->running(false);
  clientFg_->uninit();
}


void MediaManager::init(std::shared_ptr<VideoviewFactory> viewFactory,
                        StatisticsInterface *stats)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Initiating");
  stats_ = stats;
  streamer_ = std::unique_ptr<Delivery> (new Delivery());
  viewFactory_ = viewFactory;

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

  hwResources_ = std::shared_ptr<ResourceAllocator>(new ResourceAllocator());

  clientFg_->init(stats, hwResources_);
  clientFg_->setSelfViews(viewFactory_->getSelfVideos());

  sfuFg_->init(stats, hwResources_);

  streamer_->init(stats_, hwResources_);

  QObject::connect(this, &MediaManager::updateVideoSettings,
                   clientFg_.get(), &FilterGraph::updateVideoSettings);

  QObject::connect(this, &MediaManager::updateAudioSettings,
                   clientFg_.get(), &FilterGraph::updateAudioSettings);

  QObject::connect(this, &MediaManager::updateAutomaticSettings,
                   clientFg_.get(), &FilterGraph::updateAutomaticSettings);
}


void MediaManager::uninit()
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Closing");

  // first filter graph, then streamer because of the rtpfilters
  clientFg_->running(false);
  clientFg_->uninit();

  sfuFg_->running(false);
  sfuFg_->uninit();

  stats_ = nullptr;
  if (streamer_ != nullptr)
  {
    streamer_->uninit();
    streamer_ = nullptr;
  }

  localInitialIndex_ = -1;
}

void MediaManager::newSession(uint32_t sessionID,
                              std::shared_ptr<SDPMessageInfo> peerInfo,
                              const std::shared_ptr<SDPMessageInfo> localInfo,
                              bool iceController,
                              bool followOurSDP)
{
  if (localInitialIndex_ < 0)
  {
    localInitialIndex_ = viewFactory_->getConferenceSize();
  }

  // TODO: support stop-time and start-time as recommended by RFC 4566 section 5.9
  if (!sessionChecks(peerInfo, localInfo))
  {
    return;
  }

  if (getMediaNettype(peerInfo, 0) != "IN")
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "What are we using if not the internet!?");
    return;
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Start creating media");

  if (sessions_.find(sessionID) == sessions_.end())
  {
    sessions_[sessionID].ice = std::unique_ptr<ICE>(new ICE(sessionID, stats_));

    // connect signals so we get information when ice is ready
    QObject::connect(sessions_[sessionID].ice.get(), &ICE::mediaNominationSucceeded,
                     this, &MediaManager::iceSucceeded);

    QObject::connect(sessions_[sessionID].ice.get(), &ICE::mediaNominationFailed,
                     this, &MediaManager::iceFailed);
  }

  return modifySession(sessionID, peerInfo, localInfo, iceController, followOurSDP);
}


void MediaManager::modifySession(uint32_t sessionID,
                                 std::shared_ptr<SDPMessageInfo> peerInfo,
                                 const std::shared_ptr<SDPMessageInfo> localInfo,
                                 bool iceController,
                                 bool followOurSDP)
{
  // TODO: support stop-time and start-time as recommended by RFC 4566 section 5.9
  if (!sessionChecks(peerInfo, localInfo))
  {
    return;
  }

  if (stats_ != nullptr)
  {
    if(seenCNames_.find(sessionID) == seenCNames_.end())
    {
      seenCNames_[sessionID] = QSet<QString>();
    }

    QSet<QString>& seenCNAMEs = seenCNames_[sessionID];

    for (auto& media : peerInfo->media)
    {
      std::vector<QString> remoteCNAMEs;
      findCNAMEs(media, remoteCNAMEs);

      for (const auto& cname : remoteCNAMEs)
      {
        // Skip if already processed
        if (seenCNAMEs.contains(cname))
          continue;

        // Skip local cname unless it's the only one
        if (cname == CName::cname() && remoteCNAMEs.size() > 1)
          continue;

        stats_->addParticipant(sessionID, cname);
        seenCNAMEs.insert(cname);
      }
    }
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Modifying participant");
  QList<std::shared_ptr<ICEInfo>> localCandidates;
  QList<std::shared_ptr<ICEInfo>> remoteCandidates;

  for (auto& media : localInfo->media)
  {
    localCandidates += media.candidates;
  }

  for (auto& media : peerInfo->media)
  {
    remoteCandidates += media.candidates;
  }

  // perform ICE
  if (!localCandidates.empty() && !remoteCandidates.empty())
  {
    sessions_[sessionID].localInfo = localInfo;
    sessions_[sessionID].peerInfo = peerInfo;
    sessions_[sessionID].followOurSDP = followOurSDP;

     // in mesh conference host, we also have media meant for others, so we don't have and id for those
    unsigned int idIndex = 0;

    // each media has its own separate ICE
    for (unsigned int i = 0; i < localInfo->media.size(); ++i)
    {
      // only test if this is a local candidate
      if (isLocalCandidate(localInfo->media.at(i).candidates.first()))
      {
        /*
         * TODO: Search for SSRC in the media and use that as the ID
        participants_[sessionID].ice->startNomination(allIDs.at(idIndex),
                                                      localInfo->media.at(i),
                                                      peerInfo->media.at(i),
                                                      iceController);
        ++idIndex;
*/
      }
    }
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Did not find any ICE candidates, not performing ICE");

    unsigned int medias = localInfo->media.size();

    if (peerInfo->media.size() < medias)
    {
      Logger::getLogger()->printProgramError(this, "Different amount of medias in local vs peer");
      medias = peerInfo->media.size();
    }

    bool haveSFU = false;
    bool haveP2P = false;

    for (unsigned int i = 0; i < medias; ++i)
    {
      for(auto& attribute : localInfo->media.at(i).valueAttributes)
      {
        if (attribute.type == A_LABEL &&
            attribute.value == "SFU")
        {
          haveSFU = true;
          break;
        }
        else if (attribute.type == A_LABEL &&
                 attribute.value == "P2P")
        {
          haveP2P = true;
          break;
        }
      }
    }

    if (haveSFU && haveP2P)
    {
      hwResources_->setArchitectureBitrate(HYBRID_UPLINK_BITRATE);
    }
    else if (haveSFU)
    {
      hwResources_->setArchitectureBitrate(SINGLE_UPLINK_BITRATE);
    }
    else if (haveP2P)
    {
      hwResources_->setArchitectureBitrate(MULTI_UPLINK_BITRATE);
    }
    else
    {
      // no sfu available, we must send our media to all other participants
      hwResources_->setArchitectureBitrate(MULTI_UPLINK_BITRATE);
    }

    for (unsigned int i = 0; i < medias; ++i)
    {
      bool send = false;
      bool receive = false;
      getMediaAttributes(localInfo->media.at(i), peerInfo->media.at(i), followOurSDP,
                         send, receive);

      if (isLocalAddress(localInfo->media.at(i).connection_address))
      {
        if (settingString(SettingsKey::sipRole) == "Client")
        {
          // we are a client, we need to create connections from SDP, but we dont care about topology
          clientMedia(sessionID, localInfo->media.at(i), peerInfo->media.at(i), send, receive);
        }
        else if (settingString(SettingsKey::sipRole) == "Server")
        {
          if (!followOurSDP)
          {
            Logger::getLogger()->printWarning(this, "Server is not host, this may cause issues");
          }

          if (settingString(SettingsKey::sipTopology) == "P2P Mesh")
          {
            Logger::getLogger()->printNormal(this, "Acting as P2P mesh host, no media");
          }
          else if (settingString(SettingsKey::sipTopology) == "SFU")
          {
            Logger::getLogger()->printNormal(this, "Acting as SFU server, setting up UDP relays");
            sfuMedia(sessionID, localInfo->media.at(i), peerInfo->media.at(i), send, receive);
          }
          else if (settingString(SettingsKey::sipTopology) == "Hybrid")
          {
            Logger::getLogger()->printNormal(this, "Acting as Hybrid server, setting up UDP relays for SFU portion");
            sfuMedia(sessionID, localInfo->media.at(i), peerInfo->media.at(i), send, receive);
          }
          else if (settingString(SettingsKey::sipTopology) == "MCU")
          {
            Logger::getLogger()->printUnimplemented(this, "MCU server topology not implemented");
          }
          else if (settingString(SettingsKey::sipTopology) == "No Conferencing")
          {
            Logger::getLogger()->printNormal(this, "No conferencing, no server media");
          }
          else
          {
            Logger::getLogger()->printProgramError(this, "Unknown server topology");
          }
        }
        else
        {
          Logger::getLogger()->printProgramError(this, "Unknown media role");
        }
      }
    }

    if (settingString(SettingsKey::sipRole) != "Server")
    {
      // TODO: This does not work if video is not enabled
      clientFg_->updateConferenceSize();
    }
  }
}


void MediaManager::clientMedia(uint32_t sessionID,
                               const MediaInfo& localMedia,
                               const MediaInfo& remoteMedia,
                               bool send,
                               bool receive)
{
  if (localMedia.connection_address == "" || remoteMedia.connection_address == "")
  {
    Logger::getLogger()->printProgramError(this, "Address was empty when creating outgoing media");
    return;
  }

  // add structures for keeping track of this session in Delivery
  if(!streamer_->addSession(sessionID,
                             remoteMedia.connection_addrtype,
                             remoteMedia.connection_address,
                             localMedia.connection_addrtype,
                             localMedia.connection_address))
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "Error creating RTP peer");
    return;
  }

  if(remoteMedia.proto == "RTP/AVP" ||
      remoteMedia.proto == "RTP/AVPF" ||
      remoteMedia.proto == "RTP/SAVP" ||
      remoteMedia.proto == "RTP/SAVPF")
  {
    // there is only one stream/SSRC on the client side
    std::vector<uint32_t> localSSRCs;
    findSSRCs(localMedia, localSSRCs);

    QString codec = rtpNumberToCodec(remoteMedia);

    if (localSSRCs.size() != 1)
    {
      Logger::getLogger()->printProgramError(this, "Our media has incorrect amount of SSRCs");
      return;
    }

    // This is a hack. Only send video if we are within sensible amount of participants.
    if (localInitialIndex_ < settingValue(SettingsKey::sipVisibleParticipants) || localMedia.type != "video")
    {
      clientSendMedia(sessionID, localMedia, remoteMedia, send, codec,
                      localSSRCs.at(0));
    }
    else
    {
      Logger::getLogger()->printNormal(this, "Not sending video due to participant limit",
                                       {"Limit"}, {QString::number(settingValue(SettingsKey::sipVisibleParticipants))});
    }

    // go through all SSRCs and try to receive them
    for (auto& attributeList : remoteMedia.multiAttributes)
    {
      // in P2P Mesh, sometimes the host has to generate us an SSRC,
      // so we should not try to receive it
      // if the message only contains one attribute which is ours, assume this is a self call
      if (attributeList.size() >= 2 &&
          attributeList.at(0).type == A_SSRC &&
          attributeList.at(1).type == A_CNAME &&
          (attributeList.at(1).value != CName::cname() || remoteMedia.multiAttributes.size() == 1))
      {
        // as a client we should only have one SSRC and the each remote SSRC will have its own attributeList
        clientReceiveMedia(sessionID, localMedia, remoteMedia, receive, codec,
                           localSSRCs.at(0), attributeList.at(0).value.toULong(), attributeList.at(1).value);
      }
    }
  }
  else
  {
    Logger::getLogger()->printUnimplemented(this, "Remote has unknown proto");
    return;
  }
}


void MediaManager::clientSendMedia(uint32_t sessionID,
                                   const MediaInfo& localMedia,
                                   const MediaInfo& remoteMedia,
                                   bool enabled,
                                   QString codec,
                                   uint32_t localSSRC)
{
  std::vector<uint32_t> remoteSSRCs;
  findSSRCs(remoteMedia, remoteSSRCs);

  std::vector<QString> remoteCNAMEs;
  findCNAMEs(remoteMedia, remoteCNAMEs);

  if (remoteSSRCs.empty() || remoteCNAMEs.size() != remoteSSRCs.size()) {
    Logger::getLogger()->printWarning(this, "Invalid or missing SSRCs/CNAMEs in remote media, stream not activated");
    return;
  }

  std::vector<uint32_t> filteredSSRCs;
  std::vector<QString> filteredCnames;

  QString ourCname = CName::cname();

  for (size_t i = 0; i < remoteSSRCs.size(); ++i)
  {
    if (remoteCNAMEs.at(i) != ourCname || remoteSSRCs.size() == 1)
    {
      filteredSSRCs.push_back(remoteSSRCs.at(i));
      filteredCnames.push_back(remoteCNAMEs.at(i));
    }
  }

  // the remote SSRC does not matter for the outgoing media
  std::shared_ptr<Filter> senderFilter = streamer_->addRTPSendStream(sessionID,
                                                                     localMedia.connection_address,
                                                                     remoteMedia.connection_address,
                                                                     localMedia.receivePort,
                                                                     remoteMedia.receivePort,
                                                                     codec, remoteMedia.rtpNums.at(0),
                                                                     localSSRC, filteredSSRCs.at(0));

  // if we want to send
  if(enabled && remoteMedia.receivePort != 0)
  {
    Q_ASSERT(remoteMedia.receivePort);
    Q_ASSERT(!remoteMedia.rtpNums.empty());

    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Creating client send stream", {"Destination", "Type"},
                                    {remoteMedia.connection_address + ":" + QString::number(remoteMedia.receivePort),
                                     remoteMedia.type});

    Q_ASSERT(senderFilter != nullptr);

    if(remoteMedia.type == "audio")
    {
      if (localMedia.bandwidth.size() > 0)
      {
        hwResources_->setConferenceBitrate(DT_OPUSAUDIO, localMedia.bandwidth.at(0).value*1000);
      }
      clientFg_->sendAudioTo(sessionID, senderFilter, localSSRC);
    }
    else if(remoteMedia.type == "video")
    {
      if (localMedia.bandwidth.size() > 0)
      {
        hwResources_->setConferenceBitrate(DT_HEVCVIDEO, localMedia.bandwidth.at(0).value*1000);
      }

      clientFg_->sendVideoto(sessionID, senderFilter, localSSRC, filteredSSRCs, filteredCnames,
                             isP2P(localMedia, remoteMedia), getResolution(localMedia));
    }
    else
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Unsupported media type!",
                                      {"type"}, QStringList() << remoteMedia.type);
    }
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
                                    "Not sending media according to attribute", {"Type"}, {localMedia.type});

    // TODO: Spec says we should still send RTCP if the port is not 0
  }
}


void MediaManager::clientReceiveMedia(uint32_t sessionID,
                                      const MediaInfo& localMedia,
                                      const MediaInfo& remoteMedia,
                                      bool enabled,
                                      QString codec,
                                      uint32_t localSSRC,
                                      uint32_t remoteSSRC,
                                      QString remoteCNAME)
{
  std::shared_ptr<Filter> receiverFilter = streamer_->addRTPReceiveStream(sessionID,
                                                                          localMedia.connection_address,
                                                                          remoteMedia.connection_address,
                                                                          localMedia.receivePort,
                                                                          remoteMedia.receivePort,
                                                                          codec, localMedia.rtpNums.at(0),
                                                                          localSSRC, remoteSSRC);

  if (enabled)
  {
    Q_ASSERT(localMedia.receivePort);
    Q_ASSERT(!localMedia.rtpNums.empty());

    Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
                                    "Creating client receive stream",
                                    {"Interface", "codec"},
                                    {localMedia.connection_address + ":"
                                         + QString::number(localMedia.receivePort),
                                     codec});

    Q_ASSERT(receiverFilter != nullptr);
    if (localMedia.type == "audio")
    {
      clientFg_->receiveAudioFrom(sessionID, receiverFilter, remoteSSRC, remoteCNAME);
    }
    else if (localMedia.type == "video")
    {
      VideoInterface* videoView = viewFactory_->getVideo(remoteSSRC);
      if (videoView != nullptr)
      {
        clientFg_->receiveVideoFrom(sessionID, receiverFilter, videoView, remoteSSRC, remoteCNAME);
      }
      else
      {
        Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                        "Failed to get view from viewFactory");
      }
    }
    else
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                      "Unsupported incoming media type!",
                                      {"type"}, QStringList() << localMedia.type);
    }
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
                                    "Not receiving media according to attribute",
                                    {"Type"}, {localMedia.type});
  }
}

void MediaManager::sfuMedia(uint32_t sessionID,
                            const MediaInfo& localMedia,
                            const MediaInfo& remoteMedia,
                            bool send, bool receive)
{
  if (localMedia.connection_address == "" || remoteMedia.connection_address == "")
  {
    Logger::getLogger()->printProgramError(this, "Address was empty when creating outgoing media");
    return;
  }

  // add structures for keeping track of this session in Delivery
  if(!streamer_->addSession(sessionID,
                             remoteMedia.connection_addrtype,
                             remoteMedia.connection_address,
                             localMedia.connection_addrtype,
                             localMedia.connection_address))
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "Error creating RTP peer");
    return;
  }

  if(remoteMedia.proto == "RTP/AVP" ||
      remoteMedia.proto == "RTP/AVPF" ||
      remoteMedia.proto == "RTP/SAVP" ||
      remoteMedia.proto == "RTP/SAVPF")
  {
    // there is only one stream/SSRC on the client sid

    std::vector<uint32_t> localSSRCs;
    findSSRCs(localMedia, localSSRCs);

    if (localSSRCs.empty())
    {
      Logger::getLogger()->printNormal(this, "We are not yet sending anything");
      return;
    }

    for (auto& localSSRC : localSSRCs)
    {
      sfuSendMedia(sessionID, localMedia, remoteMedia, send, localSSRC);
    }

    sfuReceiveMedia(sessionID, localMedia, remoteMedia, receive, "sfu");
  }
  else
  {
    Logger::getLogger()->printUnimplemented(this, "Remote has unknown proto");
    return;
  }
}


void MediaManager::sfuSendMedia(uint32_t sessionID,
                                const MediaInfo& localMedia,
                                const MediaInfo& remoteMedia,
                                bool enabled,
                                uint32_t localSSRC)
{
  std::vector<uint32_t> remoteSSRCs;
  findSSRCs(remoteMedia, remoteSSRCs);

  if (remoteSSRCs.empty())
  {
    Logger::getLogger()->printWarning(this, "Invalid or missing SSRCs in remote media, stream not activated");
    return;
  }

  std::shared_ptr<Filter> send = streamer_->addUDPSendStream(sessionID,
                                                             localMedia.connection_address,
                                                             remoteMedia.connection_address,
                                                             localMedia.receivePort,
                                                             remoteMedia.receivePort, remoteSSRCs.at(0));

  if (enabled)
  {
    Q_ASSERT(localMedia.receivePort);
    Q_ASSERT(!localMedia.rtpNums.empty());

    Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Creating SFU send stream", {"Destination", "Type"},
                                    {remoteMedia.connection_address + ":" + QString::number(remoteMedia.receivePort),
                                     remoteMedia.type});

    Q_ASSERT(send != nullptr);

    if(localMedia.type == "audio")
    {
      sfuFg_->sendAudioTo(sessionID, send, localSSRC);
    }
    else if(localMedia.type == "video")
    {
      sfuFg_->sendVideoto(sessionID, send, localSSRC, remoteSSRCs, {}, false, getResolution(localMedia));
    }
    else
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Unsupported media type!",
                                      {"type"}, QStringList() << localMedia.type);
    }
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
                                    "Not sending media according to attribute", {"Type"}, {localMedia.type});
  }
}


void MediaManager::sfuReceiveMedia(uint32_t sessionID,
                                   const MediaInfo& localMedia,
                                   const MediaInfo& remoteMedia,
                                   bool enabled,
                                   QString remoteCNAME)
{
  // the SFU receives one media which it distributes to multiple participants

  std::vector<uint32_t> remoteSSRCs;
  findSSRCs(remoteMedia, remoteSSRCs);

  if (remoteSSRCs.size() != 1)
  {
    Logger::getLogger()->printError(this, "Incorrect amount of SSRCs for SFU receive");
    return;
  }

  std::shared_ptr<Filter> receive = streamer_->addUDPReceiveStream(sessionID,
                                                                   localMedia.connection_address,
                                                                   localMedia.receivePort,
                                                                   remoteSSRCs.at(0));

  if (enabled)
  {
    if (localMedia.type == "audio")
    {
      sfuFg_->receiveAudioFrom(sessionID, receive, remoteSSRCs.at(0), remoteCNAME);
    }
    else if (localMedia.type == "video")
    {
      VideoInterface* videoView = viewFactory_->getVideo(remoteSSRCs.at(0));
      sfuFg_->receiveVideoFrom(sessionID, receive, videoView, remoteSSRCs.at(0), remoteCNAME);
    }
    else
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, "Unsupported incoming media type!",
                                      {"type"}, QStringList() << localMedia.type);
    }
  }
}


void MediaManager::removeParticipant(uint32_t sessionID)
{
  if (sessions_.find(sessionID) != sessions_.end())
  {
    sessions_[sessionID].ice->uninit();
    sessions_.erase(sessionID);
  }

  if (settingString(SettingsKey::sipRole) != "Server")
  {
    clientFg_->removeParticipant(sessionID);
  }

  if (settingString(SettingsKey::sipRole) != "Client")
  {
    sfuFg_->removeParticipant(sessionID);
  }

  if (streamer_ != nullptr)
  {
    streamer_->removePeer(sessionID);
  }

  seenCNames_.erase(sessionID);

  Logger::getLogger()->printDebug(DEBUG_NORMAL, "Media Manager", "Session media removed",
            {"SessionID"}, {QString::number(sessionID)});
}

void MediaManager::iceSucceeded(const uint32_t& ssrc, uint32_t sessionID,
                                MediaInfo local, MediaInfo remote)
{
  if (sessions_.find(sessionID) == sessions_.end())
  {
    Logger::getLogger()->printProgramError(this, "Could not find participant when ice finished");
    return;
  }

  Logger::getLogger()->printNormal(this, "ICE nomination has succeeded", {"SessionID"},
                                   {QString::number(sessionID)});

  /*
   * TODO: Figure out how this goes with SSRCs
  VideoInterface* view = nullptr;

  if (local.type == "video")
  {
    for (auto& media : participants_[sessionID].allIDs)
    {
      if (media == id)
      {
        view = viewFactory_->getVideo(media);
        if (view == nullptr)
        {
            Logger::getLogger()->printProgramError(this, "Media view was not set correctly");
            return;
        }
        break;
      }
    }

    if (view == nullptr)
    {
      Logger::getLogger()->printProgramError(this, "Could not find a view for media");
      return;
    }
  }

  // TODO: Support SFU for ICE by also calling sfuMedia() here
  clientMedia(sessionID, local, remote, view, id.getSend(), id.getReceive());
*/
}


void MediaManager::iceFailed(const uint32_t &ssrc, uint32_t sessionID)
{
  Logger::getLogger()->printError(this, "ICE failed, removing participant");

  // the participant is removed later by receiver of this signal
  emit iceMediaFailed(sessionID);
}


QString MediaManager::rtpNumberToCodec(const MediaInfo& info)
{
  // If we are not using raw.
  // This is the place where all other preset audio video codec numbers should be set
  // but its unlikely that we will support any besides raw pcmu.
  if(info.rtpNums.at(0) != 0)
  {
    Q_ASSERT(!info.rtpMaps.empty());
    if(!info.rtpMaps.empty())
    {
      return info.rtpMaps.at(0).codec;
    }
  }
  return "PCMU";
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


uint32_t MediaManager::countParticipants(const std::shared_ptr<SDPMessageInfo> peerInfo)
{
  uint32_t participants = 0;

  for (auto& media : peerInfo->media)
  {
    if (media.type == "video")
    {
      for (auto& attributeList : media.multiAttributes)
      {
        QString localCName = CName::cname();
        if (attributeList.size() == 2 &&
            attributeList.at(0).type == A_SSRC &&
            attributeList.at(1).type == A_CNAME &&
            attributeList.at(1).value != localCName)
        {
          ++participants;
        }
      }
    }
  }

  return participants;
}


bool MediaManager::isP2P(const MediaInfo& localMedia, const MediaInfo& remoteMedia) const
{
  // Heuristic 1: Check 'mid' attribute prefix
  for (const SDPAttribute& attr : remoteMedia.valueAttributes)
  {
    if (attr.type == A_MID)
    {
      if (attr.value.startsWith("sfu", Qt::CaseInsensitive))
        return false;
      if (attr.value.startsWith("p2p", Qt::CaseInsensitive))
        return true;
    }
  }

  // Heuristic 2: Check 'label' attribute
  for (const SDPAttribute& attr : remoteMedia.valueAttributes)
  {
    if (attr.type == A_LABEL)
    {
      if (attr.value.contains("sfu", Qt::CaseInsensitive))
        return false;
      if (attr.value.contains("p2p", Qt::CaseInsensitive))
        return true;
    }
  }

  // Heuristic 3: Check number of SSRCs
  int ssrcCount = 0;
  for (const SDPAttribute& attr : remoteMedia.valueAttributes)
  {
    if (attr.type == A_SSRC)
      ++ssrcCount;
  }

  if (ssrcCount > 1)
    return false; // SFU generally aggregates streams from multiple users

  // Fallback: Default to P2P
  return true;
}


std::pair<uint16_t, uint16_t> MediaManager::getResolution(const MediaInfo& localMedia)
{
  for (const auto& [rtpnum, attr] : localMedia.imgAttributes)
  {
    if (attr.sendResolution.has_value())
    {
      Logger::getLogger()->printNormal(this, "Using SDP resolution", "Resolution",
                                       QString::number(attr.sendResolution->x) + "x" +
                                           QString::number(attr.sendResolution->y));
      return {attr.sendResolution->x, attr.sendResolution->y};
    }
  }

  if (settingEnabled(SettingsKey::videoFileEnabled))
  {
    Logger::getLogger()->printNormal(this, "Using file resolution", "Resolution",
                                     settingString(SettingsKey::videoFileResolutionWidth) + "x" +
                                         settingString(SettingsKey::videoFileResolutionHeight));
    return {
        settingValue(SettingsKey::videoFileResolutionWidth),
        settingValue(SettingsKey::videoFileResolutionHeight)
    };
  }

  Logger::getLogger()->printNormal(this, "Using camera resolution", "Resolution",
                                   settingString(SettingsKey::videoResolutionWidth) + "x" +
                                       settingString(SettingsKey::videoResolutionHeight));
  return {
      settingValue(SettingsKey::videoResolutionWidth),
      settingValue(SettingsKey::videoResolutionHeight)
  };
}
