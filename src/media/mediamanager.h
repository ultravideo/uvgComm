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

class FilterGraph;
class MediaSession;
struct MediaInfo;
class VideoInterface;

typedef int16_t PeerID;

class MediaManager : public QObject
{
  Q_OBJECT

public:
  MediaManager();
  ~MediaManager();

  // make sure viewfactory is iniated before this
  void init(QList<VideoInterface *> selfViews, StatisticsInterface *stats);
  void uninit();

  // registers a contact for activity monitoring
  void registerContact(in_addr ip);

  void addParticipant(uint32_t sessionID, const std::shared_ptr<SDPMessageInfo> peerInfo,
                      const std::shared_ptr<SDPMessageInfo> localInfo, VideoInterface *videoView,
                      bool iceController);

  void modifyParticipant(uint32_t sessionID, const std::shared_ptr<SDPMessageInfo> peerInfo,
                         const std::shared_ptr<SDPMessageInfo> localInfo, VideoInterface *videoView,
                         bool iceController);

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
  void iceSucceeded(QList<std::shared_ptr<ICEPair>>& streams,
                           quint32 sessionID);
  void iceFailed(quint32 sessionID);

signals:
  void iceMediaFailed(uint32_t sessionID);

private:

  void createCall(uint32_t sessionID,
                  std::shared_ptr<SDPMessageInfo> peerInfo,
                  const std::shared_ptr<SDPMessageInfo> localInfo, VideoInterface *videoView, bool followOurSDP);

  void createOutgoingMedia(uint32_t sessionID, const MediaInfo& localMedia,
                           QString peerAddress, const MediaInfo& remoteMedia, bool useOurSDP);
  void createIncomingMedia(uint32_t sessionID, const MediaInfo& localMedia,
                           QString localAddress, const MediaInfo& remoteMedia, VideoInterface *videoView, bool useOurSDP);

  QString rtpNumberToCodec(const MediaInfo& info);

  void sdpToStats(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> sdp, bool local);

  QString getMediaNettype(std::shared_ptr<SDPMessageInfo> sdp, int mediaIndex);
  QString getMediaAddrtype(std::shared_ptr<SDPMessageInfo> sdp, int mediaIndex);
  QString getMediaAddress(std::shared_ptr<SDPMessageInfo> sdp, int mediaIndex);

  // update MediaInfo of SDP after ICE has finished
  void setMediaPair(MediaInfo& media, std::shared_ptr<ICEInfo> mediaInfo, bool local);

  bool sessionChecks(std::shared_ptr<SDPMessageInfo> peerInfo,
                     const std::shared_ptr<SDPMessageInfo> localInfo) const;

  struct ParticipantMedia
  {
    std::unique_ptr<ICE> ice;
    std::shared_ptr<SDPMessageInfo> localInfo;
    std::shared_ptr<SDPMessageInfo> peerInfo;

    VideoInterface* videoView;
    bool followOurSDP;
  };

  StatisticsInterface* stats_;

  std::unique_ptr<FilterGraph> fg_;
  std::unique_ptr<Delivery> streamer_;

  std::map<uint32_t, ParticipantMedia> participants_;
};
