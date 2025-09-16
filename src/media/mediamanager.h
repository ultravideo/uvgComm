#pragma once

#include "initiation/negotiation/sdptypes.h"
#include "delivery/ice.h"

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

class FilterGraphClient;
class FilterGraphSFU;
class MediaSession;
struct MediaInfo;
class VideoInterface;

class ResourceAllocator;

class Filter;

typedef int16_t PeerID;


struct ParticipantMedia
{
  std::unique_ptr<ICE> ice;
  std::shared_ptr<SDPMessageInfo> localInfo;
  std::shared_ptr<SDPMessageInfo> peerInfo;

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

  void newSession(uint32_t sessionID,
                  const std::shared_ptr<SDPMessageInfo> peerInfo,
                  const std::shared_ptr<SDPMessageInfo> localInfo,
                  bool iceController,
                  bool followOurSDP);

  void modifySession(uint32_t sessionID,
                     const std::shared_ptr<SDPMessageInfo> peerInfo,
                     const std::shared_ptr<SDPMessageInfo> localInfo,
                     bool iceController,
                     bool followOurSDP);

  void removeParticipant(uint32_t sessionID);

  // Functions that enable using uvgComm as just a streming client for whatever reason.
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
  void iceSucceeded(const uint32_t &ssrc, uint32_t sessionID,
                    MediaInfo local, MediaInfo remote);
  void iceFailed(const uint32_t& ssrc, uint32_t sessionID);

signals:
  void iceMediaFailed(uint32_t sessionID);

private:
  void clientMedia(uint32_t sessionID,
                   const MediaInfo& localMedia,
                   const MediaInfo& remoteMedia,
                   bool send,
                   bool receive);

  void clientSendMedia(uint32_t sessionID,
                       const MediaInfo& localMedia,
                       const MediaInfo& remoteMedia,
                       bool enabled,
                       QString codec,
                       uint32_t localSSRC);

  void clientReceiveMedia(uint32_t sessionID,
                          const MediaInfo& localMedia,
                          const MediaInfo& remoteMedia,
                          bool enabled,
                          QString codec,
                          uint32_t localSSRC,
                          uint32_t remoteSSRC,
                          QString remoteCNAME);

  void sfuMedia(uint32_t sessionID,
                const MediaInfo& localMedia,
                const MediaInfo& remoteMedia,
                bool send,
                bool receive);

  void sfuSendMedia(uint32_t sessionID,
                    const MediaInfo& localMedia,
                    const MediaInfo& remoteMedia,
                    bool enabled,
                    uint32_t localSSRC);

  void sfuReceiveMedia(uint32_t sessionID,
                       const MediaInfo& localMedia,
                       const MediaInfo& remoteMedia,
                       bool enabled,
                       QString remoteCNAME);

  QString rtpNumberToCodec(const MediaInfo& info);

  QString getMediaNettype(std::shared_ptr<SDPMessageInfo> sdp, int mediaIndex);
  QString getMediaAddrtype(std::shared_ptr<SDPMessageInfo> sdp, int mediaIndex);
  QString getMediaAddress(std::shared_ptr<SDPMessageInfo> sdp, int mediaIndex);

  bool sessionChecks(std::shared_ptr<SDPMessageInfo> peerInfo,
                     const std::shared_ptr<SDPMessageInfo> localInfo) const;

  uint32_t countParticipants(const std::shared_ptr<SDPMessageInfo> peerInfo);

  bool isP2P(const MediaInfo& localMedia, const MediaInfo& remoteMedia) const;

  std::pair<uint16_t, uint16_t> getResolution(const MediaInfo& localMedia);

  StatisticsInterface* stats_;

  std::unique_ptr<FilterGraphClient> clientFg_;
  std::unique_ptr<FilterGraphSFU> sfuFg_;

  std::unique_ptr<Delivery> streamer_;

  std::map<uint32_t, ParticipantMedia> sessions_;

  std::shared_ptr<VideoviewFactory> viewFactory_;

  std::shared_ptr<ResourceAllocator> hwResources_;

  int localInitialIndex_;

  std::map<uint32_t, QSet<QString>> seenCNames_;
};
