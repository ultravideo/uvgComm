#include "mediamanager.h"
#include "media/processing/filtergraph.h"
#include "media/processing/filter.h"
#include "ui/gui/videoviewfactory.h"
#include "initiation/negotiation/sdptypes.h"
#include "statisticsinterface.h"
#include "common.h"

#include <QHostAddress>
#include <QtEndian>
#include <QDebug>

#ifdef __KVZRTP__
#include "media/delivery/kvzrtp/rtpstreamer.h"
#include "media/delivery/kvzrtp/framedsourcefilter.h"
#include "media/delivery/kvzrtp/rtpsinkfilter.h"
#else
#include "media/delivery/live555/rtpstreamer.h"
#include "media/delivery/live555/framedsourcefilter.h"
#include "media/delivery/live555/rtpsinkfilter.h"
#endif

MediaManager::MediaManager():
  stats_(nullptr),
  fg_(new FilterGraph()),
  session_(nullptr),
  streamer_(new RTPStreamer()),
  mic_(true),
  camera_(true)
{}

MediaManager::~MediaManager()
{
  fg_->running(false);
  fg_->uninit();
}

void MediaManager::init(std::shared_ptr<VideoviewFactory> viewfactory, StatisticsInterface *stats)
{
  qDebug() << "Iniating: Media manager";
  viewfactory_ = viewfactory;
  streamer_->init(stats);
  streamer_->start();
  stats_ = stats;
  fg_->init(viewfactory_->getVideo(0, 0), stats); // 0 is the selfview index. The view should be created by GUI
}

void MediaManager::uninit()
{
  qDebug() << "Closing," << metaObject()->className();
  // first filter graph, then streamer because of the rtpfilters
  fg_->running(false);
  fg_->uninit();

  stats_ = nullptr;
  streamer_->stop();
  streamer_->uninit();
}

void MediaManager::updateSettings()
{
  fg_->updateSettings();
  fg_->camera(camera_); // kind of a hack to make sure the camera/mic state is preserved
  fg_->mic(mic_);
}

void MediaManager::addParticipant(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> peerInfo,
                    const std::shared_ptr<SDPMessageInfo> localInfo)
{
  // TODO: support stop-time and start-time as recommended by RFC 4566 section 5.9

  if(peerInfo->timeDescriptions.at(0).startTime != 0 || localInfo->timeDescriptions.at(0).startTime != 0)
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_ADD_MEDIA, "Nonzero start-time not supported!");
    return;
  }

  if(peerInfo->connection_nettype == "IN")
  {
    QHostAddress audio_addr(peerInfo->connection_address);
    QHostAddress video_addr(peerInfo->connection_address);

    // determine if we want to use SDP address or the address in media
    // TODO: this should be fixed so that each media can have a different address.    if (!peerInfo->media[0].connection_address.isEmpty())    {
      audio_addr.setAddress(peerInfo->media[0].connection_address);
    }

    if (!peerInfo->media[1].connection_address.isEmpty())
    {
      video_addr.setAddress(peerInfo->media[1].connection_address);
    }

    if (stats_ != nullptr)
    {
      stats_->addParticipant(peerInfo->connection_address, "0", "0");
    }

    if (peerInfo->connection_addrtype == "IP4"
       || ((peerInfo->connection_addrtype == "IP6" && video_addr.toString().left(7) == "::ffff:") &&
           (peerInfo->connection_addrtype == "IP6" && audio_addr.toString().left(7) == "::ffff:"))
       || ((QHostAddress(video_addr.toIPv4Address()) == QHostAddress(video_addr)) &&
           (QHostAddress(audio_addr.toIPv4Address()) == QHostAddress(audio_addr)))) // address is ipv4 indeed
    {
      if(!streamer_->addPeer(sessionID, video_addr, audio_addr))
      {
        printDebug(DEBUG_PROGRAM_ERROR, this, DC_ADD_MEDIA, "Error creating RTP peer. Simultaneous destruction?.");
        return;
      }
    }
    else {
      printDebug(DEBUG_PROGRAM_ERROR, this, DC_ADD_MEDIA, "Not supported in media creation.",
                 { "Media type", "video address", "audio address" },
                 { peerInfo->connection_addrtype, video_addr.toString(), audio_addr.toString() });
      return;
    }
  }
  else
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_ADD_MEDIA, "What are we using if not the internet!?");
    return;
  }

  qDebug() << "\n\nCREATING MEDIA...";

  // create each agreed media stream
  for(auto media : peerInfo->media)
  {
    qDebug() << "\n\t\t\tcreating outgoing media for" << media.type << "\n";
    createOutgoingMedia(sessionID, media);
  }

  for(auto media : localInfo->media)
  {
    qDebug() << "\n\t\t\tcreating incoming media for" << media.type << "\n";
    createIncomingMedia(sessionID, media);
  }

  qDebug() << "DONE CREATING MEDIA\n\n";


  // crashes at the moment.
  //fg_->print();
}


void MediaManager::createOutgoingMedia(uint32_t sessionID, const MediaInfo& remoteMedia)
{
  bool send = true;
  bool recv = true;

  transportAttributes(remoteMedia.flagAttributes, send, recv);

  // if they want to receive
  if(recv)
  {
    Q_ASSERT(remoteMedia.receivePort);
    Q_ASSERT(!remoteMedia.rtpNums.empty());

    QString codec = rtpNumberToCodec(remoteMedia);

    if(remoteMedia.proto == "RTP/AVP")
    {
      std::shared_ptr<Filter> framedSource = streamer_->addSendStream(sessionID, remoteMedia.receivePort,
                                                                      codec, remoteMedia.rtpNums.at(0));
      if(remoteMedia.type == "audio")
      {
        qDebug() << "\nSENDING AUDIO TO" << remoteMedia.receivePort;
        fg_->sendAudioTo(sessionID, std::shared_ptr<Filter>(framedSource));
      }
      else if(remoteMedia.type == "video")
      {
        qDebug() << "\nSENDING VIDEO TO" << remoteMedia.receivePort;
        fg_->sendVideoto(sessionID, std::shared_ptr<Filter>(framedSource));
      }
      else
      {
        printDebug(DEBUG_PROGRAM_ERROR, this, DC_ADD_MEDIA, "Unsupported media type!",
                  {"type"}, QStringList() << remoteMedia.type);
      }
    }
    else
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, DC_ADD_MEDIA, "SDP transport protocol not supported.");
    }
  }
  else
  {
    printDebug(DEBUG_NORMAL, this, DC_ADD_MEDIA,
               "Not creating media because they don't seem to want any according to attribute.");

    // TODO: Spec says we should still send RTCP
  }
}

void MediaManager::createIncomingMedia(uint32_t sessionID, const MediaInfo &localMedia)
{
  bool send = true;
  bool recv = true;

  transportAttributes(localMedia.flagAttributes, send, recv);
  if(recv)
  {
    Q_ASSERT(localMedia.receivePort);
    Q_ASSERT(!localMedia.rtpNums.empty());

    QString codec = rtpNumberToCodec(localMedia);

    qDebug() << "Creating incoming media with codec:" << codec;

    if(localMedia.proto == "RTP/AVP")
    {
      std::shared_ptr<Filter> rtpSink = streamer_->addReceiveStream(sessionID, localMedia.receivePort,
                                                                    codec, localMedia.rtpNums.at(0));
      if(localMedia.type == "audio")
      {
        qDebug() << "\nRECEIVING AUDIO FROM" << localMedia.receivePort;
        fg_->receiveAudioFrom(sessionID, std::shared_ptr<Filter>(rtpSink));
      }
      else if(localMedia.type == "video")
      {
        fg_->receiveVideoFrom(sessionID, std::shared_ptr<Filter>(rtpSink), viewfactory_->getVideo(sessionID, 0));
      }
      else
      {
        printDebug(DEBUG_PROGRAM_ERROR, this, DC_ADD_MEDIA, "Unsupported incoming media type!",
                  {"type"}, QStringList() << localMedia.type);
      }
    }
    else
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, DC_ADD_MEDIA, "Incoming SDP transport protocol not supported.");
    }
  }
  else
  {
    printDebug(DEBUG_NORMAL, this, DC_ADD_MEDIA,
               "Not creating media because they don't seem to want any according to attribute.");
  }
}

void MediaManager::removeParticipant(uint32_t sessionID)
{
  fg_->removeParticipant(sessionID);
  fg_->camera(camera_); // if the last participant was destroyed, restore camera state
  fg_->mic(mic_);
  streamer_->removePeer(sessionID);
  qDebug() << "Session " << sessionID << " media removed.";
}

void MediaManager::endAllCalls()
{
  fg_->removeAllParticipants();
  streamer_->removeAllPeers();

  fg_->camera(camera_); // if the last participant was destroyed, restore camera state
  fg_->mic(mic_);
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
  return "pcmu";
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
