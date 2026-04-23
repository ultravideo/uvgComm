
#include "common.h"
#include "settingskeys.h"

#include "initiation/negotiation/sdptypes.h"

#include "logger.h"

// Didn't find sleep in QCore
#ifdef Q_OS_WIN
#include <winsock2.h> // for windows.h
#include <windows.h> // for Sleep
#endif

#include <QSettings>
#include <QMutex>
#include <QNetworkInterface>

#include <cstdlib>
#include <cmath>
#include <algorithm>


const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz"
                         "0123456789";

constexpr float SPEAKER_PORTION = 0.80f;


static QString settingsFile = "uvgComm.ini";

QString getSettingsFile()
{
  return settingsFile;
}


void setSettingsFile(QString filename)
{
  settingsFile = filename;
}


QString generateRandomString(uint32_t length)
{
  // TODO make this cryptographically secure to avoid collisions
  QString string;
  for( unsigned int i = 0; i < length; ++i )
  {
    string.append(alphabet.at(rand()%alphabet.size()));
  }
  return string;
}


bool settingEnabled(QString key)
{
  return settingValue(key) == 1;
}


int settingValue(QString key)
{
  QSettings settings(settingsFile, settingsFileFormat);

  if (!settings.value(key).isValid())
  {
    Logger::getLogger()->printWarning("Common", "Found faulty setting", {"Key"}, {key});
    return 0;
  }

  return settings.value(key).toInt();
}


QString settingString(QString key)
{
  QSettings settings(settingsFile, settingsFileFormat);

  if (!settings.value(key).isValid())
  {
    Logger::getLogger()->printWarning("Common", "Found faulty setting", {"Key"}, {key});
    return "";
  }

  return settings.value(key).toString();
}


bool settingBool(QString key)
{
  QSettings settings(settingsFile, settingsFileFormat);

  if (!settings.value(key).isValid())
  {
    Logger::getLogger()->printWarning("Common", "Found faulty setting",
                                    {"Key"}, {key});
    return "";
  }

  return settings.value(key).toBool();
}


QString getLocalUsername()
{
  QSettings settings(settingsFile, settingsFileFormat);

  return !settings.value(SettingsKey::localUsername).isNull()
      ? settings.value(SettingsKey::localUsername).toString() : "anonymous";
}


bool isLocalCandidate(std::shared_ptr<ICEInfo> info)
{
  QString candidateAddress = info->address;

  if (info->rel_address != "")
  {
    candidateAddress = info->rel_address;
  }

  return isLocalAddress(candidateAddress);
}

bool isLocalAddress(QString candidateAddress)
{
  for (const QHostAddress& localInterface : QNetworkInterface::allAddresses())
  {
    if (localInterface.toString() == candidateAddress)
    {
      return true;
    }
  }

  return false;
}


bool getSendAttribute(const MediaInfo &media, bool local)
{
  bool send = false;
  if (local)
  {
    send = (media.flagAttributes.empty()
                 || media.flagAttributes.at(0) == A_NO_ATTRIBUTE
                 || media.flagAttributes.at(0) == A_SENDRECV
                 || media.flagAttributes.at(0) == A_SENDONLY);
  }
  else
  {
    send = (media.flagAttributes.empty()
                 || media.flagAttributes.at(0) == A_NO_ATTRIBUTE
                 || media.flagAttributes.at(0) == A_SENDRECV
                 || media.flagAttributes.at(0) == A_RECVONLY);
  }

  return send;
}


bool getReceiveAttribute(const MediaInfo &media, bool local)
{
  bool receive = false;
  if (local)
  {
    receive = (media.flagAttributes.empty()
                 || media.flagAttributes.at(0) == A_NO_ATTRIBUTE
                 || media.flagAttributes.at(0) == A_SENDRECV
                 || media.flagAttributes.at(0) == A_RECVONLY);
  }
  else
  {
    receive = (media.flagAttributes.empty()
                 || media.flagAttributes.at(0) == A_NO_ATTRIBUTE
                 || media.flagAttributes.at(0) == A_SENDRECV
                 || media.flagAttributes.at(0) == A_SENDONLY);
  }

  return receive;
}


void getMediaAttributes(const MediaInfo &local, const MediaInfo &remote,
                                           bool followOurSDP,
                                           bool& send, bool& receive)
{
  if (followOurSDP)
  {
    receive = getReceiveAttribute(local, followOurSDP);
    send =    getSendAttribute(local, followOurSDP);
  }
  else
  {
    receive = getReceiveAttribute(remote, followOurSDP);
    send =    getSendAttribute(remote, followOurSDP);
  }
}


void setSDPAddress(QString inAddress, QString& address, QString& nettype, QString& addressType)
{
  if (!inAddress.isEmpty())
  {
    address = inAddress;
    nettype = "IN";

    // TODO: Improve the address detection
    if (inAddress.front() == '[') {
      address = inAddress.mid(1, inAddress.size() - 2);
      addressType = "IP6";
    } else {
      addressType = "IP4";
    }
  }
  else
  {
    Logger::getLogger()->printError("Common", "Failed to set SDP address");
  }
}


bool containCandidates(std::vector<std::shared_ptr<ICEPair>> &streams,
                            std::vector<std::shared_ptr<ICEPair>> allCandidates)
{
  int matchingStreams = 0;
  for (auto& candidatePair : allCandidates)
  {
    for (auto& readyConnection : streams)
    {
      if (sameCandidate(readyConnection->local, candidatePair->local) &&
          sameCandidate(readyConnection->remote, candidatePair->remote))
      {
        ++matchingStreams;
      }
    }
  }

  return streams.size() == matchingStreams;
}


bool sameCandidates(std::vector<std::shared_ptr<ICEPair> > newCandidates,
                         std::vector<std::shared_ptr<ICEPair> > oldCandidates)
{
  if (newCandidates.empty() || oldCandidates.empty())
  {
    return false;
  }

  for (auto& newCandidate: newCandidates)
  {
    bool candidatesFound = false;

    for (auto& oldCandidate: oldCandidates)
    {
      if (sameCandidate(newCandidate->local, oldCandidate->local) &&
          sameCandidate(newCandidate->remote, oldCandidate->remote))
      {
        // we have a match!
        candidatesFound = true;
      }
    }

    if(!candidatesFound)
    {
      // could not find a match for this candidate, which means we have to perform ICE
      return false;
    }
  }

  return true;
}


bool sameCandidate(std::shared_ptr<ICEInfo> firstCandidate,
                        std::shared_ptr<ICEInfo> secondCandidate)
{
  return firstCandidate->foundation == secondCandidate->foundation &&
      firstCandidate->component == secondCandidate->component &&
      firstCandidate->transport == secondCandidate->transport &&
      firstCandidate->address == secondCandidate->address &&
      firstCandidate->port == secondCandidate->port &&
      firstCandidate->type == secondCandidate->type &&
      firstCandidate->rel_address == secondCandidate->rel_address &&
      firstCandidate->rel_port == secondCandidate->rel_port;
}


void printIceCandidates(QString text, QList<std::shared_ptr<ICEInfo>> candidates)
{
  QStringList names;
  QStringList values;
  for (auto& candidate : candidates)
  {
    names.append("Candidate");
    if (candidate->type == "srflx" || candidate->type == "prflx")
    {
      values.append(candidate->rel_address + ":" + QString::number(candidate->rel_port));
    }
    else
    {
      values.append(candidate->address + ":" + QString::number(candidate->port));
    }
  }

  Logger::getLogger()->printNormal("Common", text, names, values);
}


uint32_t const EPOCH_NTP_DIFFERENCE = 2208988800;


uint64_t msecToNTP(int64_t msec)
{
  uint64_t msw = msec/1000 + EPOCH_NTP_DIFFERENCE;
  double modulo_msecs = msec%1000;
  uint32_t lsw = (modulo_msecs/1000)*UINT32_MAX;

  return (msw << 32 | lsw);
}


int64_t NTPToMsec(uint64_t ntp)
{
  uint64_t msw = ntp >> 32; // seconds
  uint32_t lsw = ntp & 0xffffffff; // partial seconds

  uint32_t msw_ms = msw*1000 - EPOCH_NTP_DIFFERENCE;
  uint32_t lsw_ms = double(1/lsw)*1000;

  return msw_ms + lsw_ms;
}


QSize galleryResolution(QSize baseResolution, uint32_t otherParticipants)
{
  if (otherParticipants == 0)
  {
    Logger::getLogger()->printError("Common", "Zero participants is not legal");
    return QSize(0,0);
  }

  int cols = std::ceil(std::sqrt(otherParticipants));
  int rows = std::ceil(float(otherParticipants)/cols);

  int width = baseResolution.width()/cols - (baseResolution.width()/cols)%8;
  int height = baseResolution.height()/rows - (baseResolution.height()/rows)%2;

  QSize resolution = QSize(width, height);

  if ((baseResolution.width()/cols)%8 != 0 || (baseResolution.height()/rows)%2 != 0)
  {
    Logger::getLogger()->printWarning("Common", "Target resolution not divisible by 8 or 2",
                                    {"Resolution reduction"},
                                    {QString::number((baseResolution.width()/cols)%8) + "x" +
                                     QString::number((baseResolution.height()/rows)%2)});
  }

  // the encoder only supports resolutions larger than or equal to 64x64
  if (resolution.width() < 64)
  {
    Logger::getLogger()->printWarning("Common", "Resolution too small",
                                    {"Width"}, {QString::number(resolution.width())});
    resolution.setWidth(64);
  }
  if (resolution.height() < 64)
  {
    Logger::getLogger()->printWarning("Common", "Resolution too small",
                                    {"Height"}, {QString::number(resolution.height())});
    resolution.setHeight(64);
  }

  return resolution;
}


QSize speakerResolution(QSize baseResolution, uint32_t otherParticipants)
{
  if (otherParticipants == 1)
  {
    // no listeners, only speaker
    return baseResolution;
  }

  return QSize(baseResolution.width(), baseResolution.height()* SPEAKER_PORTION);
}


QSize listenerResolution(QSize baseResolution, uint32_t otherParticipants)
{
  if (otherParticipants == 0)
  {
    Logger::getLogger()->printError("Common", "Zero participants is not legal");
    return QSize(0, 0);
  }
  if (otherParticipants == 1)
  {
    // no listeners, only speaker
    return speakerResolution(baseResolution, otherParticipants);
  }

  uint32_t otherListeners = otherParticipants - 1; // exclude speaker

  constexpr float LISTENER_PORTION = 1.0f - SPEAKER_PORTION;
  constexpr float TARGET_ASPECT_RATIO = 16.0f / 9.0f;

  int listenerRowHeight = baseResolution.height() * LISTENER_PORTION;
  int maxTileWidth = listenerRowHeight * TARGET_ASPECT_RATIO;

  // Total required width to fit all listeners side by side
  int listenerWidth = baseResolution.width()/otherListeners;

  if (listenerWidth > maxTileWidth)
  {
    // No cropping needed
    listenerWidth = maxTileWidth;
  }

  // Align to encoder constraints (multiples of 8/2)
  listenerWidth -= listenerWidth % 8;
  listenerRowHeight -= listenerRowHeight % 2;

  // Avoid too-small tiles as encoder cannot handle these
  if (listenerWidth < 64)
  {
    Logger::getLogger()->printWarning("Common", "Listener resolution too small",
                                    {"Width"}, {QString::number(listenerWidth)});
    listenerWidth = 64;
  }
  if (listenerRowHeight < 64)
  {
    Logger::getLogger()->printWarning("Common", "Listener resolution too small",
                                    {"Height"}, {QString::number(listenerRowHeight)});
    listenerRowHeight = 64;
  }

  return QSize(listenerWidth, listenerRowHeight);
}


int32_t galleryBitrate(QSize baseResolution, int baseBitrate, uint32_t otherParticipants)
{
  QSize resolution = galleryResolution(baseResolution, otherParticipants);

  double bitsPerPixel = (double)baseBitrate/(baseResolution.width()*baseResolution.height());
  return resolution.width()*resolution.height()*bitsPerPixel;
}

int32_t speakerBitrate(QSize baseResolution, int baseBitrate, uint32_t otherParticipants)
{
  QSize resolution = speakerResolution(baseResolution, otherParticipants);

  double bitsPerPixel = (double)baseBitrate/(baseResolution.width()*baseResolution.height());
  return resolution.width()*resolution.height()*bitsPerPixel;
}

int32_t listenerBitrate(QSize baseResolution, int baseBitrate, uint32_t otherParticipants)
{
  QSize resolution = listenerResolution(baseResolution, otherParticipants);

  double bitsPerPixel = (double)baseBitrate/(baseResolution.width()*baseResolution.height());
  return resolution.width()*resolution.height()*bitsPerPixel;
}


QHostAddress getLocalAddress(std::shared_ptr<ICEInfo> info)
{
  // use relay address
  if (info->type != "host" &&
      info->rel_address != "" &&
      info->rel_port != 0)
  {
    return QHostAddress(info->rel_address);
  }

         // don't use relay address
  return QHostAddress(info->address);
}


quint16 getLocalPort(std::shared_ptr<ICEInfo> info)
{
  // use relay port
  if (info->type != "host" &&
      info->rel_address != "" &&
      info->rel_port != 0)
  {
    return info->rel_port;
  }

         // don't use relay port
  return info->port;
}


uint32_t findSSRC(const MediaInfo &media)
{
  for (auto& attribute : media.multiAttributes)
  {
    if (attribute.first().type == A_SSRC)
    {
      return attribute.first().value.toULong();
    }
  }

  return 0;
}


bool findSSRCs(const MediaInfo &media, std::vector<uint32_t> &ssrc)
{
  for (auto& attributeList : media.multiAttributes)
  {
    for (auto& attribute : attributeList)
    {
      if (attribute.type == A_SSRC)
      {
        ssrc.push_back(attribute.value.toUInt());
      }
    }
  }

  return !ssrc.empty();
}


bool findCNAMEs(const MediaInfo &media, std::vector<QString> &cnames)
{
  QSet<QString> uniqueCnames;

  for (const auto &attributeList : media.multiAttributes)
  {
    for (const auto &attribute : attributeList)
    {
      if (attribute.type == A_CNAME && uniqueCnames.find(attribute.value) == uniqueCnames.end())
      {
        cnames.push_back(attribute.value);
      }
    }
  }

  return !cnames.empty();
}


QString findMID(const MediaInfo &media)
{
  for (auto& attribute : media.valueAttributes)
  {
    if (attribute.type == A_MID)
    {
      return attribute.value;
    }
  }

  return "";
}


double calculateVideoOverheadPercentage(uint32_t streamBitrateBps,
                                        uint32_t framerate_num,
                                        uint32_t framerate_den,
                                        bool ipv6,
                                        uint16_t mtu_bytes)
{
  constexpr double DEFAULT_RTP_OVERHEAD = 0.05; // 5%
  constexpr uint16_t IPV4_HEADER_SIZE = 20;
  constexpr uint16_t IPV6_HEADER_SIZE = 40;
  constexpr uint16_t UDP_HEADER_SIZE = 8;
  constexpr uint16_t RTP_HEADER_SIZE = 12;
  constexpr double RTCP_OVERHEAD = 0.05; // 5%
  
  // Validate inputs (error returns allowed)
  if (streamBitrateBps == 0 || framerate_num == 0 || framerate_den == 0)
  {
    Logger::getLogger()->printProgramError("Common", "Invalid parameters for overhead calc");
    return DEFAULT_RTP_OVERHEAD + RTCP_OVERHEAD;
  }

  // Compute frame rate as numerator/denominator
  double frameRate = static_cast<double>(framerate_num) / static_cast<double>(framerate_den);
  if (frameRate <= 0.0)
  {
    return DEFAULT_RTP_OVERHEAD + RTCP_OVERHEAD;
  }

  // Convert kbps to bps and compute average frame size in bits/bytes
  const double AVG_FRAME_BITS = streamBitrateBps / frameRate;
  const double AVG_FRAME_BYTES = AVG_FRAME_BITS / 8.0;

  // Select IP header size
  double ipHeader = static_cast<double>(IPV4_HEADER_SIZE);
  if (ipv6)
  {
    ipHeader = static_cast<double>(IPV6_HEADER_SIZE);
  }

  const double HEADER_BYTES = ipHeader + static_cast<double>(UDP_HEADER_SIZE) + static_cast<double>(RTP_HEADER_SIZE);
  const double PAYLOAD_PER_PACKET = static_cast<double>(mtu_bytes) - HEADER_BYTES;

  double overhead = 0.0;

  // If the average frame fits in a single packet, compute header fraction per frame
  if (AVG_FRAME_BYTES < PAYLOAD_PER_PACKET)
  {
    overhead = HEADER_BYTES / (AVG_FRAME_BYTES + HEADER_BYTES);
  }
  else // full rtp frames
  {
    double packetsPerFrame = std::ceil(AVG_FRAME_BYTES / PAYLOAD_PER_PACKET);
    double totalHeaderBytes = packetsPerFrame * HEADER_BYTES;
    overhead = totalHeaderBytes / (AVG_FRAME_BYTES + totalHeaderBytes);
  }

  overhead += RTCP_OVERHEAD;

  if (overhead < 0.06)
  {
    overhead = 0.06;
  }
  else if (overhead > 0.5)
  {
    Logger::getLogger()->printWarning("Common", "Very high transmission overhead detected, consider optimizing. Forcing 50%");
    overhead = 0.5;
  }

  return overhead;
}
