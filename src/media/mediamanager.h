#pragma once

#include "initiation/negotiation/sdptypes.h"
#include "delivery/ice.h"
#include "mediaid.h"

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
class VideoInterface;

typedef int16_t PeerID;


struct ParticipantMedia
{
  std::unique_ptr<ICE> ice;
  std::shared_ptr<SDPMessageInfo> localInfo;
  std::shared_ptr<SDPMessageInfo> peerInfo;

  QList<MediaID> allIDs;
  bool followOurSDP;
};

class MediaManager : public QObject
{
  Q_OBJECT

public:
  MediaManager();
  ~MediaManager();

  // make sure viewfactory is iniated before this
  void init(std::shared_ptr<VideoviewFactory> viewFactory,
            StatisticsInterface *stats);
  void uninit();

  // registers a contact for activity monitoring
  void registerContact(in_addr ip);

  void addParticipant(uint32_t sessionID,
                      const std::shared_ptr<SDPMessageInfo> peerInfo,
                      const std::shared_ptr<SDPMessageInfo> localInfo,
                      const QList<MediaID>& allIDs,
                      bool iceController, bool followOurSDP);

  void modifyParticipant(uint32_t sessionID,
                         const std::shared_ptr<SDPMessageInfo> peerInfo,
                         const std::shared_ptr<SDPMessageInfo> localInfo,
                         const QList<MediaID>& allIDs,
                         bool iceController, bool followOurSDP);

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

public slots:
  void iceSucceeded(const MediaID &id, uint32_t sessionID,
                    MediaInfo local, MediaInfo remote);
  void iceFailed(const MediaID& id, uint32_t sessionID);

signals:
  void iceMediaFailed(uint32_t sessionID);

private:

  void createMediaPair(uint32_t sessionID, const MediaID &id,
                       const MediaInfo &localMedia, const MediaInfo &remoteMedia,
                       VideoInterface *videoView);

  void createOutgoingMedia(uint32_t sessionID, const MediaInfo& localMedia,
                           const MediaInfo& remoteMedia, const MediaID &id, bool active);
  void createIncomingMedia(uint32_t sessionID, const MediaInfo& localMedia,
                           const MediaInfo& remoteMedia, const MediaID &id, VideoInterface *videoView, bool active);

  QString rtpNumberToCodec(const MediaInfo& info);

  void sdpToStats(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> sdp, bool local);

  QString getMediaNettype(std::shared_ptr<SDPMessageInfo> sdp, int mediaIndex);
  QString getMediaAddrtype(std::shared_ptr<SDPMessageInfo> sdp, int mediaIndex);
  QString getMediaAddress(std::shared_ptr<SDPMessageInfo> sdp, int mediaIndex);

  bool sessionChecks(std::shared_ptr<SDPMessageInfo> peerInfo,
                     const std::shared_ptr<SDPMessageInfo> localInfo) const;

  StatisticsInterface* stats_;

  std::unique_ptr<FilterGraph> fg_;
  std::unique_ptr<Delivery> streamer_;

  std::map<uint32_t, ParticipantMedia> participants_;

  std::shared_ptr<VideoviewFactory> viewFactory_;
};
