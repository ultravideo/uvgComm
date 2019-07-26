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

#include "media/delivery/kvzrtp/kvzrtp.h"
#include "media/delivery/live555/rtpstreamer.h"

MediaManager::MediaManager():
  stats_(nullptr),
  fg_(new FilterGraph()),
  session_(nullptr),
  streamer_(nullptr),
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
  printDebug(DEBUG_NORMAL, this, DC_STARTUP, "Initiating");
  viewfactory_ = viewfactory;

  stats_ = stats;
  fg_->init(viewfactory_->getVideo(0, 0), stats); // 0 is the selfview index. The view should be created by GUI
}

void MediaManager::uninit()
{
  printDebug(DEBUG_NORMAL, this, DC_SHUTDOWN, "Closing");
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

  if (streamer_ == nullptr)
  {
    QSettings settings("kvazzup.ini", QSettings::IniFormat);
    int kvzrtp = settings.value("sip/kvzrtp").toInt();

    if (kvzrtp == 1)
    {
      streamer_ = std::unique_ptr<IRTPStreamer> (new KvzRTP());
    }
    else
    {
      streamer_ = std::unique_ptr<IRTPStreamer> (new Live555RTP());
    }

    streamer_->init(stats_);
    streamer_->start();
  }

  if(peerInfo->connection_nettype == "IN")
  {
    if (stats_ != nullptr)
    {
      stats_->addParticipant(peerInfo->connection_address, "0", "0");
    }

    if(!streamer_->addPeer(sessionID))
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, DC_ADD_MEDIA, "Error creating RTP peer. Simultaneous destruction?.");
      return;
    }
  }
  else
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_ADD_MEDIA, "What are we using if not the internet!?");
    return;
  }

  printDebug(DEBUG_NORMAL, this, DC_STARTUP, "Start creating media.", {"Outgoing media", "Incoming media"},
            {QString::number(peerInfo->media.size()), QString::number(localInfo->media.size())});

  // create each agreed media stream
  for(auto media : peerInfo->media)
  {
    createOutgoingMedia(sessionID, media, peerInfo->connection_address);
  }

  for(auto media : localInfo->media)
  {
    createIncomingMedia(sessionID, media, localInfo->connection_address);
  }
  // crashes at the moment.
  //fg_->print();
}


void MediaManager::createOutgoingMedia(uint32_t sessionID, const MediaInfo& remoteMedia, QString globalAddress)
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
      QHostAddress address =  QHostAddress(globalAddress);
      if (remoteMedia.connection_address != "")
      {
        printDebug(DEBUG_NORMAL, this, DC_ADD_MEDIA, "Using media specific address.", {"Address"}, {address.toString()});
        address.setAddress(remoteMedia.connection_address);
      }
      else {
        printDebug(DEBUG_NORMAL, this, DC_ADD_MEDIA, "Using global address.", {"Address"}, {address.toString()});
      }

      std::shared_ptr<Filter> framedSource = streamer_->addSendStream(sessionID, address, remoteMedia.receivePort,
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

void MediaManager::createIncomingMedia(uint32_t sessionID, const MediaInfo &localMedia, QString globalAddress)
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
      QHostAddress address =  QHostAddress(globalAddress);
      if (localMedia.connection_address != "")
      {
        printDebug(DEBUG_NORMAL, this, DC_ADD_MEDIA, "Using media specific address for incoming.",
                  {"Type", "Address"}, {localMedia.type, address.toString()});
        address.setAddress(localMedia.connection_address);
      }
      else
      {
        printDebug(DEBUG_NORMAL, this, DC_ADD_MEDIA, "Using global address for incoming.",
                   {"Type", "Address"}, {localMedia.type, address.toString()});
      }

      std::shared_ptr<Filter> rtpSink = streamer_->addReceiveStream(sessionID, address, localMedia.receivePort,
                                                                    codec, localMedia.rtpNums.at(0));
      Q_ASSERT(rtpSink != nullptr);
      if(localMedia.type == "audio")
      {
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
  printDebug(DEBUG_NORMAL, "Media Manager", DC_REMOVE_MEDIA, "Session media removed",
            {"SessionID"}, {QString::number(sessionID)});
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
