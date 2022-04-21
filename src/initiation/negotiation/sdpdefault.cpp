#include "sdpdefault.h"

#include "mediacapabilities.h"

#include "common.h"
#include "logger.h"

#include <QDateTime>


void setSDPAddress(QString inAddress, QString& sdpAddress,
                   QString& type, QString& addressType);

void generateOrigin(std::shared_ptr<SDPMessageInfo> sdp, QString localAddress);

bool generateAudioMedia(MediaInfo &audio);
bool generateVideoMedia(MediaInfo &video);

std::shared_ptr<SDPMessageInfo> generateDefaultSDP(QString localAddress)
{
  // TODO: The desired media formats should come from outside initiation as a parameter

  Logger::getLogger()->printNormal("SDPNegotiationHelper",
                                   "Generating new SDP message with our address",
                                   "Local address", localAddress);

  QString localUsername = getLocalUsername();

  if(localAddress == ""
     || localAddress == "0.0.0.0"
     || localUsername == "")
  {
    Logger::getLogger()->printWarning("SDPNegotiationHelper",
                                      "Necessary info not set for SDP generation",
                                      {"username"}, {localUsername});

    return nullptr;
  }

  std::shared_ptr<SDPMessageInfo> newInfo
      = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
  newInfo->version = 0;
  generateOrigin(newInfo, localAddress);

  setSDPAddress(localAddress, newInfo->connection_address,
                newInfo->connection_nettype, newInfo->connection_addrtype);

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


void generateOrigin(std::shared_ptr<SDPMessageInfo> sdp,
                    QString localAddress)
{
  // RFC 2327, section 6
  sdp->originator_username = getLocalUsername();

  // TODO: NTP timestamp is recommended (secs and picosecs since 1900)
  sdp->sess_id = QDateTime::currentMSecsSinceEpoch();
  sdp->sess_v = QDateTime::currentMSecsSinceEpoch();

  setSDPAddress(localAddress, sdp->host_address, sdp->host_nettype, sdp->host_addrtype);
}


void setSDPAddress(QString inAddress, QString& sdpAddress, QString& type, QString& addressType)
{
  sdpAddress = inAddress;
  type = "IN";

  // TODO: Improve the address detection
  if (inAddress.front() == '[')
  {
    sdpAddress = inAddress.mid(1, inAddress.size() - 2);
    addressType = "IP6";
  }
  else
  {
    addressType = "IP4";
  }
}


bool generateAudioMedia(MediaInfo &audio)
{
  // we ignore nettype, addrtype and address, because we use a global c=
  audio = {"audio", 0, "RTP/AVP", {},
           "", "", "", "", {},"", DYNAMIC_AUDIO_CODECS, {A_SENDRECV}, {}};

  // add all the dynamic numbers first because we want to favor dynamic type codecs.
  for(RTPMap& codec : audio.codecs)
  {
    audio.rtpNums.push_back(codec.rtpNum);
  }

  audio.rtpNums += PREDEFINED_AUDIO_CODECS;
  return true;
}


bool generateVideoMedia(MediaInfo& video)
{
  // we ignore nettype, addrtype and address, because we use a global c=
  video = {"video", 0, "RTP/AVP", {},
           "", "", "", "", {},"", DYNAMIC_VIDEO_CODECS, {A_SENDRECV}, {}};

  for(RTPMap& codec : video.codecs)
  {
    video.rtpNums.push_back(codec.rtpNum);
  }

  // just for completeness, we will probably never support any of the pre-set video types.
  video.rtpNums += PREDEFINED_VIDEO_CODECS;
  return true;
}
