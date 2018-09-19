#include "mediamanager.h"

#include "filtergraph.h"
#include "rtpstreamer.h"
#include "filter.h"
#include "framedsourcefilter.h"
#include "rtpsinkfilter.h"
#include "gui/videoviewfactory.h"
#include "sip/sdptypes.h"

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
  qDebug() << "Iniating media manager";
  viewfactory_ = viewfactory;
  streamer_->init(stats);
  streamer_->start();
  fg_->init(viewfactory_->getVideo(0), stats); // 0 is the selfview index. The view should be created by GUI
}

void MediaManager::uninit()
{
  qDebug() << "Destroying media manager";
  // first filter graph, then streamer because of the rtpfilters
  fg_->running(false);
  fg_->uninit();

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
    qWarning() << "ERROR: Non zero start-time not supported!";
  }


  if(peerInfo->connection_nettype == "IN")
  {
    QHostAddress address;
    address.setAddress(peerInfo->connection_address);

    if(peerInfo->connection_addrtype == "IP4")
    {
      in_addr ip;
      ip.S_un.S_addr = qToBigEndian(address.toIPv4Address());

      // TODO: Make it possible to have a separate ip address for each mediastream by fixing this.
      if(!streamer_->addPeer(ip, sessionID))
      {
        qCritical() << "Error creating RTP peer. Simultaneous destruction?";
        return;
      }
    }
    else if(peerInfo->connection_addrtype == "IP6")
    {
      qDebug() << "ERROR: IPv6 not supported in media creation";
      return;
    }
  }
  else
  {
    qWarning() << "ERROR: What are we using if not the internet!?";
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

  transportAttributes(remoteMedia.attributes, send, recv);

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
        qDebug() << "ERROR: Unsupported media type in :" << remoteMedia.type;
      }
    }
    else
    {
      qDebug() << "ERROR: SDP transport protocol not supported.";
    }
  }
  else
  {
    qDebug() << "Not creating media because they don't seem to want any according to attribute";
  }
}

void MediaManager::createIncomingMedia(uint32_t sessionID, const MediaInfo &localMedia)
{
  bool send = true;
  bool recv = true;

  transportAttributes(localMedia.attributes, send, recv);
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
        qDebug() << "ERROR: Unsupported media type in :" << localMedia.type;
      }
    }
    else
    {
      qDebug() << "ERROR: SDP transport protocol not supported.";
    }
  }
  else
  {
    qDebug() << "Not creating media because they don't seem to want any according to attribute";
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
