#include "mediamanager.h"
#include "media/processing/filtergraph.h"
#include "media/processing/filter.h"
#include "ui/gui/videoviewfactory.h"
#include "initiation/negotiation/sdptypes.h"
#include "statisticsinterface.h"
#include "common.h"

#include <QHostAddress>
#include <QtEndian>
#include <QSettings>

#include "media/delivery/delivery.h"

MediaManager::MediaManager():
  stats_(nullptr),
  fg_(new FilterGraph()),
  streamer_(nullptr),
  mic_(true),
  camera_(true),
  screenShare_(false)
{}

MediaManager::~MediaManager()
{
  fg_->running(false);
  fg_->uninit();
}

void MediaManager::init(std::shared_ptr<VideoviewFactory> viewfactory, StatisticsInterface *stats)
{
  printDebug(DEBUG_NORMAL, this, "Initiating");
  viewfactory_ = viewfactory;

  stats_ = stats;
  fg_->init(viewfactory_->getVideo(0, 0), stats); // 0 is the selfview index. The view should be created by GUI

  streamer_ = std::unique_ptr<Delivery> (new Delivery());
  streamer_->init(stats_);

  connect(
    streamer_.get(),
    &Delivery::handleZRTPFailure,
    this,
    &MediaManager::handleZRTPFailure);
}


void MediaManager::uninit()
{
  printDebug(DEBUG_NORMAL, this, "Closing");
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

void MediaManager::updateSettings()
{
  fg_->updateSettings();
  fg_->camera(camera_); // kind of a hack to make sure the camera/mic state is preserved
  fg_->mic(mic_);
}


void MediaManager::addParticipant(uint32_t sessionID,
                                  std::shared_ptr<SDPMessageInfo> peerInfo,
                                  const std::shared_ptr<SDPMessageInfo> localInfo)
{
  // TODO: support stop-time and start-time as recommended by RFC 4566 section 5.9

  Q_ASSERT(peerInfo->media.size() == localInfo->media.size());
  if (peerInfo->media.size() != localInfo->media.size())
  {
    printDebug(DEBUG_PROGRAM_ERROR, "Media manager",
               "Addparticipant, number of media in localInfo and peerInfo don't match.",
                {"LocalInfo", "PeerInfo"},
                {QString::number(localInfo->media.size()), QString::number(peerInfo->media.size())});
    return;
  }

  if(peerInfo->timeDescriptions.at(0).startTime != 0 || localInfo->timeDescriptions.at(0).startTime != 0)
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, "Nonzero start-time not supported!");
    return;
  }

  if(peerInfo->connection_nettype == "IN")
  {

    // TODO: Should check if wer should use global or media address.
    if(!streamer_->addPeer(sessionID,
                           peerInfo->media.at(0).connection_address,
                           localInfo->media.at(0).connection_address))
    {
      printDebug(DEBUG_PROGRAM_ERROR, this,
                 "Error creating RTP peer. Simultaneous destruction?");
      return;
    }
  }
  else
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, "What are we using if not the internet!?");
    return;
  }

  printDebug(DEBUG_NORMAL, this, "Start creating media.", {"Outgoing media", "Incoming media"},
            {QString::number(peerInfo->media.size()), QString::number(localInfo->media.size())});

  if (stats_ != nullptr)
  {
    sdpToStats(sessionID, peerInfo, false);
    sdpToStats(sessionID, localInfo, true);
  }

  // create each agreed media stream
  for(int i = 0; i <peerInfo->media.size(); ++i)  {
    // TODO: I don't like that we match
    createOutgoingMedia(sessionID,
                        localInfo->media.at(i),
                        peerInfo->connection_address,
                        peerInfo->media.at(i));
  }

  // TODO: THis should be got from somewhere instead of guessed.
  uint32_t videoID = 0;
  for (int i = 0; i < localInfo->media.size(); ++i)
  {
    createIncomingMedia(sessionID, localInfo->media.at(i),
                        localInfo->connection_address,
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
                                       QString peerGlobalAddress,
                                       const MediaInfo& remoteMedia)
{
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
      bool globalAddressPresent = !peerGlobalAddress.isNull() && peerGlobalAddress != "";
      bool specificAddressPresent = remoteMedia.connection_address != ""
           && !remoteMedia.connection_address.isNull();

      QHostAddress remoteAddress;
      if (specificAddressPresent)
      {
        remoteAddress.setAddress(remoteMedia.connection_address);

        printDebug(DEBUG_NORMAL, this, "Using media specific address for outgoing media.",
                  {"Type", "Path"},
                  {remoteMedia.type, localMedia.connection_address + ":" + QString::number(localMedia.receivePort) + " -> " +
                                     remoteAddress.toString() + ":" + QString::number(remoteMedia.receivePort)});
      }
      else if (globalAddressPresent)
      {
        remoteAddress.setAddress(peerGlobalAddress);
        printDebug(DEBUG_NORMAL, this, "Using global address for outgoing media.",
                  {"Type", "Remote address"},
                  {remoteMedia.type, remoteAddress.toString() + ":" + QString::number(remoteMedia.receivePort)});
      }
      else
      {
        printDebug(DEBUG_ERROR, this, "Creating outgoing media. "
                                      "No viable connection address in mediainfo. "
                                      "Should be detected earlier.");
        return;
      }

      std::shared_ptr<Filter> framedSource = streamer_->addSendStream(sessionID, remoteAddress,
                                                                      localMedia.receivePort, remoteMedia.receivePort,
                                                                      codec, remoteMedia.rtpNums.at(0));

      Q_ASSERT(framedSource != nullptr);

      if(remoteMedia.type == "audio")
      {
        fg_->sendAudioTo(sessionID, std::shared_ptr<Filter>(framedSource));
        fg_->mic(mic_);
      }
      else if(remoteMedia.type == "video")
      {
        fg_->sendVideoto(sessionID, std::shared_ptr<Filter>(framedSource));
      }
      else
      {
        printDebug(DEBUG_PROGRAM_ERROR, this, "Unsupported media type!",
                  {"type"}, QStringList() << remoteMedia.type);
      }
    }
    else
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, "SDP transport protocol not supported.");
    }
  }
  else
  {
    printDebug(DEBUG_NORMAL, this,
               "Not creating media because they don't seem to want any according to attribute.");

    // TODO: Spec says we should still send RTCP
  }
}


void MediaManager::createIncomingMedia(uint32_t sessionID,
                                       const MediaInfo &localMedia,
                                       QString localGlobalAddress,
                                       const MediaInfo &remoteMedia, uint32_t videoID)
{
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
      bool globalAddressPresent = localGlobalAddress != "" && !localGlobalAddress.isNull();
      bool specificAddressPresent = localMedia.connection_address != ""
           && !localMedia.connection_address.isNull();

      QHostAddress localAddress;
      if (specificAddressPresent)
      {
        localAddress.setAddress(localMedia.connection_address);
        printDebug(DEBUG_NORMAL, this, "Using media specific address for incoming.",
                  {"Type", "Address", "Port"},
                  {localMedia.type, localAddress.toString(), QString::number(localMedia.receivePort)});

      }
      else if (globalAddressPresent)
      {
        localAddress.setAddress(localGlobalAddress);
        printDebug(DEBUG_NORMAL, this, "Using global address for incoming.",
                   {"Type", "Address"},
                   {localMedia.type, localAddress.toString() + ":" + QString::number(localMedia.receivePort)});
      }
      else
      {
        printDebug(DEBUG_ERROR, this, "Creating incoming media. "
                                                    "No viable connection address in mediainfo. "
                                                    "Should be detected earlier.");
        return;
      }

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
          printDebug(DEBUG_PROGRAM_ERROR, this, "Failed to get view from viewFactory");
        }
      }
      else
      {
        printDebug(DEBUG_PROGRAM_ERROR, this, "Unsupported incoming media type!",
                  {"type"}, QStringList() << localMedia.type);
      }
    }
    else
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, "Incoming SDP transport protocol not supported.");
    }
  }
  else
  {
    printDebug(DEBUG_NORMAL, this,
               "Not creating media because they don't seem to want any according to attribute.");
  }
}

void MediaManager::removeParticipant(uint32_t sessionID)
{
  fg_->removeParticipant(sessionID);
  fg_->camera(camera_); // if the last participant was destroyed, restore camera state
  fg_->mic(mic_);
  streamer_->removePeer(sessionID);

  printDebug(DEBUG_NORMAL, "Media Manager", "Session media removed",
            {"SessionID"}, {QString::number(sessionID)});
}


bool MediaManager::toggleMic()
{
  mic_ = !mic_;
  fg_->mic(mic_);
  return mic_;
}

bool MediaManager::toggleCamera()
{
  camera_ = !camera_;
  fg_->camera(camera_);
  return camera_;
}

bool MediaManager::toggleScreenShare()
{
  screenShare_ = !screenShare_;
  fg_->screenShare(screenShare_, camera_);
  return screenShare_;
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
  return "pcm";
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
  // TODO: This feels like a hack to this here. Instead we should give stats the whole SDP
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
