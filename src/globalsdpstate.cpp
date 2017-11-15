#include "globalsdpstate.h"

// TODO: this should not use parser
#include "sipparser.h"

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

void GlobalSDPState::generateSDP()
{
  if(localAddress_ == QHostAddress::Null
     || localUsername_ == ""
     || firstAvailablePort_ == 0)
  {
    qWarning() << "WARNING: Necessary info not set for SDP generation:" << localAddress_.toString()
               << localUsername_ << firstAvailablePort_;
    return;
  }

  // TODO: Get suitable SDP from media manager or at least from somewhere else
  QString sdp_str = "v=0 \r\n"
                   "o=" + localUsername_ + " 1234 12345 IN IP4 " + localAddress_.toString() + "\r\n"
                   "s=Kvazzup\r\n"
                   "t=0 0\r\n"
                   "c=IN IP4 " + localAddress_.toString() + "\r\n"
                   "m=video " + QString::number(firstAvailablePort_) + " RTP/AVP 97\r\n"
                   "m=audio " + QString::number(firstAvailablePort_ + 2) + " RTP/AVP 96\r\n";

  firstAvailablePort_ += 4; // video and audio + rtcp for both

  ourInfo_ = parseSDPMessage(sdp_str);

  if(ourInfo_ == NULL)
  {
    qWarning() << "WARNING: Failed to generate SDP info from following sdp message:" << sdp_str;
  }
}

std::shared_ptr<SDPMessageInfo> GlobalSDPState::getSDP()
{

}

bool GlobalSDPState::isSDPSuitable(std::shared_ptr<SDPMessageInfo> peerSDP)
{
  Q_ASSERT(peerSDP);
  if(ourInfo_ == NULL)
  {
    qDebug() << "WARNING: Can't test SDP suitability because local SDP has not been generated";
    return false;
  }
  //TODO: WARNING THIS DOES NOT DO WAHT IT IS SUPPOSED TO DO. IT CHECKS THE SPD INSTEAD OF COMPARING IT TO EXISTING


  // TODO check codec

  //checks if the stream has both audio and video. TODO: Both are actually not necessary
  bool audio = false;
  bool video = false;

  for(auto media : peerSDP->media)
  {
    if(media.type == "video")
    {
      video = true;
    }
    if(media.type == "audio")
    {
      audio = true;
    }
  }

  return audio && video;
}
