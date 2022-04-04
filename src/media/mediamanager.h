#pragma once

#include "initiation/negotiation/sdptypes.h"

#include <QObject>
#include <QMutex>
#include <QList>

#include <memory>
#include <map>

#ifndef _MSC_VER
#ifdef _WIN32
#include <inaddr.h>
#else
#include <netinet/ip.h>
#endif
#else
#include <WinSock2.h>
#endif

// Manages the Media delivery and Media Processing and the interaction between the two
// especially during construction.

class VideoviewFactory;
class StatisticsInterface;
class Delivery;

class FilterGraph;
class MediaSession;
struct MediaInfo;

typedef int16_t PeerID;

class MediaManager : public QObject
{
  Q_OBJECT

public:
  MediaManager();
  ~MediaManager();

  // make sure viewfactory is iniated before this
  void init(std::shared_ptr<VideoviewFactory> viewfactory, StatisticsInterface *stats);
  void uninit();

  // registers a contact for activity monitoring
  void registerContact(in_addr ip);

  void addParticipant(uint32_t sessionID, const std::shared_ptr<SDPMessageInfo> peerInfo,
                      const std::shared_ptr<SDPMessageInfo> localInfo);

  void removeParticipant(uint32_t sessionID);

  // Functions that enable using Kvazzup as just a streming client for whatever reason.
  void streamToIP(in_addr ip, uint16_t port);
  void receiveFromIP(in_addr ip, uint16_t port);

signals:
  void handleZRTPFailure(uint32_t sessionID);
  void handleNoEncryption();

  // somebody is calling
  void incomingCall(unsigned int participantID);

  // our call was not accepted
  void callRejected(unsigned int participantID);

  // somebody muted their mic
  void mutedMic(unsigned int participantID);

  // somebody left the call we are in
  void participantDropped(unsigned int participantID);

  // Somebody came online or went offline
  void statusChanged(unsigned int participantID, bool online);

  // the host has quit the call and we have been chosen to become the new host (ability to kick people)
  void becameHost();

  void updateVideoSettings();
  void updateAudioSettings();
  void updateAutomaticSettings();

private:

  void createOutgoingMedia(uint32_t sessionID, const MediaInfo& localMedia,
                           QString peerGlobalAddress, const MediaInfo& remoteMedia);
  void createIncomingMedia(uint32_t sessionID, const MediaInfo& localMedia,
                           QString localGlobalAddress, const MediaInfo& remoteMedia, uint32_t videoID);

  QString rtpNumberToCodec(const MediaInfo& info);

  void transportAttributes(const QList<SDPAttributeType> &attributes, bool& send, bool& recv);

  void sdpToStats(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> sdp, bool incoming);

  StatisticsInterface* stats_;

  std::unique_ptr<FilterGraph> fg_;
  std::unique_ptr<Delivery> streamer_;

  std::shared_ptr<VideoviewFactory> viewfactory_;
};
