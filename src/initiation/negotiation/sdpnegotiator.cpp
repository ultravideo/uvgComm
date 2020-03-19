#include "sdpnegotiator.h"

#include "mediacapabilities.h"

#include "common.h"

#include <QDateTime>
#include <QDebug>

SDPNegotiator::SDPNegotiator():
  localUsername_("")
{}


void SDPNegotiator::setLocalInfo(QString username)
{
  localUsername_ = username;
}


std::shared_ptr<SDPMessageInfo> SDPNegotiator::generateLocalSDP(QString localAddress)
{
  // TODO: This should ask media manager, what options it supports.
  qDebug() << "Generating new SDP message with our address as:" << localAddress;

  if(localAddress == ""
     || localAddress == "0.0.0.0"
     || localUsername_ == "")
  {
    qWarning() << "WARNING: Necessary info not set for SDP generation:"
               << localAddress
               << localUsername_;
    return nullptr;
  }

  // TODO: Get suitable SDP from media manager
  std::shared_ptr<SDPMessageInfo> newInfo = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
  newInfo->version = 0;
  generateOrigin(newInfo, localAddress);
  setConnectionAddress(newInfo, localAddress);

  newInfo->sessionName = SESSION_NAME;
  newInfo->sessionDescription = SESSION_DESCRIPTION;

  newInfo->timeDescriptions.push_back(TimeInfo{0,0, "", "", {}});

  MediaInfo audio;
  MediaInfo video;

  if(!generateAudioMedia(audio) || !generateVideoMedia(video))
  {
    return nullptr;
  }

  newInfo->media = {audio, video};

  return newInfo;
}



std::shared_ptr<SDPMessageInfo> SDPNegotiator::negotiateSDP(SDPMessageInfo& remoteSDPOffer,
                                                          QString localAddress)
{
  // At this point we should have checked if their offer is acceptable.
  // Now we just have to generate our answer.

  std::shared_ptr<SDPMessageInfo> newInfo = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);

  // we assume our answer will be different so new origin
  generateOrigin(newInfo, localAddress);
  setConnectionAddress(newInfo, localAddress);

  newInfo->version = 0;
  newInfo->sessionName = remoteSDPOffer.sessionName;
  newInfo->sessionDescription = remoteSDPOffer.sessionDescription;
  newInfo->timeDescriptions = remoteSDPOffer.timeDescriptions;

  // Now the hard part. Select best codecs and set our corresponding media ports.
  for (auto& remoteMedia : remoteSDPOffer.media)
  {
    MediaInfo ourMedia;
    ourMedia.type = remoteMedia.type;
    ourMedia.receivePort = 0; // TODO: ICE Should set this to one of its candidates
    ourMedia.proto = remoteMedia.proto;
    ourMedia.title = remoteMedia.title;
    if (remoteMedia.flagAttributes.empty())
    {
      ourMedia.flagAttributes = {A_SENDRECV};
    }
    else if (remoteMedia.flagAttributes.back() == A_SENDONLY)
    {
      ourMedia.flagAttributes = {A_RECVONLY};
    }
    else if (remoteMedia.flagAttributes.back() == A_RECVONLY)
    {
      ourMedia.flagAttributes = {A_SENDONLY};
    }
    else {
      ourMedia.flagAttributes = remoteMedia.flagAttributes;
    }

    // set our bitrate, not implemented
    // set our encryptionKey, not implemented

    if (remoteMedia.type == "audio")
    {
      QList<uint8_t> supportedNums = PREDEFINED_AUDIO_CODECS;
      QList<RTPMap> supportedCodecs = DYNAMIC_AUDIO_CODECS;

      selectBestCodec(remoteMedia.rtpNums, remoteMedia.codecs,
                      supportedNums, supportedCodecs,
                      ourMedia.rtpNums, ourMedia.codecs);

    }
    else if (remoteMedia.type == "video")
    {
      QList<uint8_t> supportedNums = PREDEFINED_VIDEO_CODECS;
      QList<RTPMap> supportedCodecs = DYNAMIC_VIDEO_CODECS;

      selectBestCodec(remoteMedia.rtpNums, remoteMedia.codecs,
                      supportedNums, supportedCodecs,
                      ourMedia.rtpNums, ourMedia.codecs);
    }
    newInfo->media.append(ourMedia);
  }


  return newInfo;
}


bool SDPNegotiator::selectBestCodec(QList<uint8_t>& remoteNums,       QList<RTPMap> &remoteCodecs,
                                  QList<uint8_t>& supportedNums,    QList<RTPMap> &supportedCodecs,
                                  QList<uint8_t>& outMatchingNums,  QList<RTPMap> &outMatchingCodecs)
{
  for (auto& remoteCodec : remoteCodecs)
  {
    for (auto& supportedCodec : supportedCodecs)
    {
      if(remoteCodec.codec == supportedCodec.codec)
      {
        outMatchingCodecs.append(remoteCodec);
        printDebug(DEBUG_NORMAL, "Negotiation",  "Found suitable codec.");

        outMatchingNums.push_back(remoteCodec.rtpNum);

        return true;
      }
    }
  }

  for (auto& rtpNumber : remoteNums)
  {
    for (auto& supportedNum : supportedNums)
    {
      if(rtpNumber == supportedNum)
      {
        outMatchingNums.append(rtpNumber);
        printDebug(DEBUG_NORMAL, "Negotiation",  "Found suitable RTP number.");
        return true;
      }
    }
  }

  printDebug(DEBUG_ERROR, "Negotiation",
             "Could not find suitable codec or RTP number for media.");

  return false;
}


void SDPNegotiator::generateOrigin(std::shared_ptr<SDPMessageInfo> sdp,
                                 QString localAddress)
{
  sdp->originator_username = localUsername_;
  sdp->sess_id = QDateTime::currentMSecsSinceEpoch();
  sdp->sess_v = QDateTime::currentMSecsSinceEpoch();
  sdp->host_nettype = "IN";
  sdp->host_address = localAddress;
  if (localAddress.front() == "[")
  {
    sdp->host_address = localAddress.mid(1, localAddress.size() - 2);
    sdp->host_addrtype = "IP6";
  }
  else {
    sdp->host_addrtype = "IP4";
  }
}


void SDPNegotiator::setConnectionAddress(std::shared_ptr<SDPMessageInfo> sdp,
                                       QString localAddress)
{
  sdp->connection_address = localAddress;
  sdp->connection_nettype = "IN";
  if (localAddress.front() == "[")
  {
    sdp->connection_address = localAddress.mid(1, localAddress.size() - 2);
    sdp->connection_addrtype = "IP6";
  }
  else
  {
    sdp->connection_addrtype = "IP4";
  }
}

bool SDPNegotiator::generateAudioMedia(MediaInfo &audio)
{
  // we ignore nettype, addrtype and address, because we use a global c=
  audio = {"audio", 0, "RTP/AVP", {},
           "", "", "", "", {},"", DYNAMIC_AUDIO_CODECS, {A_SENDRECV}, {}};

  // add all the dynamic numbers first because we want to favor dynamic type codecs.
  for(RTPMap codec : audio.codecs)
  {
    audio.rtpNums.push_back(codec.rtpNum);
  }

  audio.rtpNums += PREDEFINED_AUDIO_CODECS;
  return true;
}


bool SDPNegotiator::generateVideoMedia(MediaInfo& video)
{
  // we ignore nettype, addrtype and address, because we use a global c=
  video = {"video", 0, "RTP/AVP", {},
           "", "", "", "", {},"", DYNAMIC_VIDEO_CODECS, {A_SENDRECV}, {}};

  for(RTPMap codec : video.codecs)
  {
    video.rtpNums.push_back(codec.rtpNum);
  }

  // just for completeness, we will probably never support any of the pre-set video types.
  video.rtpNums += PREDEFINED_VIDEO_CODECS;
  return true;
}


bool SDPNegotiator::checkSDPOffer(SDPMessageInfo &offer)
{
  // TODO: check everything.

  bool hasOpus = false;
  bool hasH265 = false;

  if(offer.connection_address == "0.0.0.0")
  {
    qDebug() << "Got bad global address from SDP:" << offer.connection_address;
    return false;
  }

  if(offer.version != 0)
  {
    qDebug() << "Their offer had not 0 version:" << offer.version;
    return false;
  }

  QStringList debugCodecsFound = {};
  for(MediaInfo media : offer.media)
  {
    for(RTPMap rtp : media.codecs)
    {
      if(rtp.codec == "opus")
      {
        debugCodecsFound << "opus";
        hasOpus = true;
      }
      else if(rtp.codec == "h265")
      {
        debugCodecsFound << "h265";
        hasH265 = true;
      }
    }
  }

  printDebug(DEBUG_NORMAL, "Negotiation",
             "Found following codecs in SDP", {"Codecs"}, debugCodecsFound);

  if (offer.timeDescriptions.size() >= 1)
  {
    if (offer.timeDescriptions.at(0).startTime != 0 ||
        offer.timeDescriptions.at(0).stopTime != 0)
    {
      printDebug(DEBUG_ERROR, "Negotiation",
                 "They offered us a session with limits. Unsupported.");
      return false;
    }
  }
  else {
    printDebug(DEBUG_PROGRAM_ERROR, "Negotiation",
               "they included wrong number of Time Descriptions. Should be detected earlier.");
    return false;
  }


  return hasOpus && hasH265;
}


void SDPNegotiator::setMediaPair(MediaInfo& media, std::shared_ptr<ICEInfo> mediaInfo)
{
  if (mediaInfo == nullptr)
  {
    return;
  }

  media.connection_address = mediaInfo->address;
  media.receivePort        = mediaInfo->port;
}


