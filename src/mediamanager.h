#pragma once
#include <QObject>
#include <memory>
#include <map>
#include <inaddr.h>

class VideoWidget;
class StatisticsInterface;

class FilterGraph;
class RTPStreamer;
class MediaSession;

typedef int16_t PeerID;

class MediaManager : public QObject
{
  Q_OBJECT

public:
  MediaManager(StatisticsInterface* stats);
  ~MediaManager();

  void init();
  void uninit();

  void updateSettings();

  // registers a contact for activity monitoring
  void registerContact(in_addr ip);

  void startCall(VideoWidget* selfView, QSize resolution);
  void addParticipant(QString callID, in_addr ip, uint16_t sendAudioPort, uint16_t recvAudioPort,
                      uint16_t sendVideoPort, uint16_t recvVideoPort, VideoWidget* view);

  void removeParticipant(QString callID);
  void endAllCalls();

  void streamToIP(in_addr ip, uint16_t port);
  void receiveFromIP(in_addr ip, uint16_t port, VideoWidget* view);

  // call changes. Returns state after toggle
  bool toggleMic();
  bool toggleCamera();

signals:

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

private:

  // callID, PeerID relation pairs
  std::map<QString,PeerID> ids_;

  StatisticsInterface* stats_;

  std::unique_ptr<FilterGraph> fg_;

  MediaSession* session_;

  std::unique_ptr<RTPStreamer> streamer_;

  bool mic_;
  bool camera_;
};
