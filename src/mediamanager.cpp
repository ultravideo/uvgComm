#include "mediamanager.h"

#include "filtergraph.h"
#include "rtpstreamer.h"
#include "filter.h"
#include "framedsourcefilter.h"
#include "rtpsinkfilter.h"
#include "gui/videoviewfactory.h"
#include "sip/sdptypes.h"
#include "statisticsinterface.h"

#include "common.h"

#include <QHostAddress>
#include <QtEndian>
#include <QDebug>

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
  fg_->init(viewfactory_->getVideo(0), stats); // 0 is the selfview index. The view should be created by GUI
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
    printDebug(DEBUG_ERROR, this, "Add Participant", "Nonzero start-time not supported!");
  }

#if 0
  qDebug() << "local opus" << localInfo->media[0].connection_address << ":" << localInfo->media[0].receivePort;
  qDebug() << "local hevc" << localInfo->media[1].connection_address << ":" << localInfo->media[1].receivePort;
  qDebug() << "remote opus" << peerInfo->media[0].connection_address  << ":" << peerInfo->media[0].receivePort;
  qDebug() << "remote hevc" << peerInfo->media[0].connection_address  << ":" << peerInfo->media[1].receivePort << "\n";
#endif

  if(peerInfo->connection_nettype == "IN")
  {
    QHostAddress address;

    if (peerInfo->media[0].connection_address.isEmpty())
    {
      address.setAddress(peerInfo->connection_address);
    }
    else
    {
      address.setAddress(peerInfo->media[0].connection_address); // media[0] is RTP
    }


    if(stats_ != nullptr)
    {
      stats_->addParticipant(peerInfo->connection_address, "0", "0");
    }

    if(peerInfo->connection_addrtype == "IP4"
       || (peerInfo->connection_addrtype == "IP6" && address.toString().left(7) == "::ffff:"))
    {
      in_addr ip;
#ifdef _WIN32
      ip.S_un.S_addr = qToBigEndian(address.toIPv4Address());
#else
      ip.s_addr = qToBigEndian(address.toIPv4Address());
#endif

      // TODO: Make it possible to have a separate ip address for each mediastream by fixing this.
      if(!streamer_->addPeer(ip, sessionID))
      {
        printDebug(DEBUG_ERROR, this, "Add Participant", "Error creating RTP peer. Simultaneous destruction?.");
        return;
      }
    }
    else {
      printDebug(DEBUG_ERROR, this, "Add Participant", "Not supported in media creation.",
                      {"Media type", "address"}, {peerInfo->connection_addrtype, address.toString()});
      return;
    }
  }
  else
  {
    printDebug(DEBUG_ERROR, this, "Add Participant", "What are we using if not the internet!?");
    return;
  }

  // create each agreed media stream
  for(auto media : peerInfo->media)
  {
    createOutgoingMedia(sessionID, media);
  }

  for(auto media : localInfo->media)
  {
    createIncomingMedia(sessionID, media);
  }

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
        fg_->sendAudioTo(sessionID, std::shared_ptr<Filter>(framedSource));
      }
      else if(remoteMedia.type == "video")
      {
        fg_->sendVideoto(sessionID, std::shared_ptr<Filter>(framedSource));
      }
      else
      {
        printDebug(DEBUG_ERROR, this, "Add Participant", "Unsupported media type!",
                  {"type"}, QStringList() << remoteMedia.type);
      }
    }
    else
    {
      printDebug(DEBUG_ERROR, this, "Add Participant", "SDP transport protocol not supported.");
    }
  }
  else
  {
    printDebug(DEBUG_NORMAL, this, "Add Participant",
               "Not creating media because they don't seem to want any according to attribute.");
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
        fg_->receiveAudioFrom(sessionID, std::shared_ptr<Filter>(rtpSink));
      }
      else if(localMedia.type == "video")
      {
        fg_->receiveVideoFrom(sessionID, std::shared_ptr<Filter>(rtpSink), viewfactory_->getVideo(sessionID));
      }
      else
      {
        printDebug(DEBUG_ERROR, this, "Add Participant", "Unsupported incoming media type!",
                  {"type"}, QStringList() << localMedia.type);
      }
    }
    else
    {
      printDebug(DEBUG_ERROR, this, "Add Participant", "Incoming SDP transport protocol not supported.");
    }
  }
  else
  {
    printDebug(DEBUG_NORMAL, this, "Add Participant",
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
