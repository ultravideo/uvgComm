#include "globalsdpstate.h"

#include <QDateTime>

GlobalSDPState::GlobalSDPState():
  localAddress_(),
  localUsername_(""),
  firstAvailablePort_(18888)
{}

void GlobalSDPState::setLocalInfo(QHostAddress localAddress, QString username)
{
  localAddress_ = localAddress;
  localUsername_ = username;
}

void GlobalSDPState::setPortRange(uint16_t minport, uint16_t maxport, uint16_t maxRTPConnections)
{
  firstAvailablePort_ = minport;
  maxPort_ = maxport;
}

std::shared_ptr<SDPMessageInfo> GlobalSDPState::localInviteSDP()
{
  if(localAddress_ == QHostAddress::Null
     || localUsername_ == ""
     || firstAvailablePort_ == 0)
  {
    qWarning() << "WARNING: Necessary info not set for SDP generation:" << localAddress_.toString()
               << localUsername_ << firstAvailablePort_;
    return NULL;
  }

  // TODO: Get suitable SDP from media manager
  std::shared_ptr<SDPMessageInfo> newInfo = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
  newInfo->version = 0;
  newInfo->originator_username = localUsername_;
  newInfo->sess_id = QDateTime::currentMSecsSinceEpoch();
  newInfo->sess_v = QDateTime::currentMSecsSinceEpoch();
  newInfo->host_nettype = "IN";
  if(localAddress_.protocol() == QAbstractSocket::IPv6Protocol)
  {
    newInfo->host_addrtype = "IP6";
    newInfo->connection_addrtype = "IP6";
  }
  else
  {
    newInfo->host_addrtype = "IP4";
    newInfo->connection_addrtype = "IP4";
  }

  newInfo->host_address = localAddress_.toString();

  newInfo->sessionName = " ";
  newInfo->sessionDescription = "A Kvazzup Video Conference";

  newInfo->connection_address = localAddress_.toString();
  newInfo->connection_nettype = "IN";

  MediaInfo audio;
  MediaInfo video;
  audio.type = "audio";
  video.type = "video";
  audio.receivePort = firstAvailablePort_;
  video.receivePort = firstAvailablePort_ + 2;
  audio.proto = "RTP/AVP";
  video.proto = "RTP/AVP";
  audio.rtpNum = 96;
  video.rtpNum = 97;
  // we ignore nettype, addrtype and address, because we have a global c=

  RTPMap a_rtp;
  RTPMap v_rtp;
  a_rtp.rtpNum = 96;
  v_rtp.rtpNum = 97;
  a_rtp.clockFrequency = 48000;
  v_rtp.clockFrequency = 90000;
  a_rtp.codec = "opus";
  v_rtp.codec = "h265";

  audio.codecs.push_back(a_rtp);
  video.codecs.push_back(a_rtp);
  audio.activity = SENDRECV;
  video.activity = SENDRECV;

  newInfo->media.push_back(audio);
  newInfo->media.push_back(video);

  firstAvailablePort_ += 4; // video and audio rtp & rtcp
}

// returns NULL if suitable could not be found
std::shared_ptr<SDPMessageInfo> localResponseSDP(std::shared_ptr<SDPMessageInfo> remoteInviteSDP)
{
  // if sdp not acceptable, change origin line to local!

}

std::shared_ptr<SDPMessageInfo> remoteFinalSDP(std::shared_ptr<SDPMessageInfo> remoteInviteSDP)
{

}
