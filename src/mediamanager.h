#pragma once

#include <QObject>
#include <QMutex>

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
  MediaManager();
  ~MediaManager();

  void init(VideoWidget *selfView, StatisticsInterface *stats);
  void uninit();

  void updateSettings();

  // registers a contact for activity monitoring
  void registerContact(in_addr ip);

  void addParticipant(QString callID, in_addr ip, uint16_t sendAudioPort, uint16_t recvAudioPort,
                      uint16_t sendVideoPort, uint16_t recvVideoPort, VideoWidget* view);

  void removeParticipant(QString callID);
  void endAllCalls();

  void streamToIP(in_addr ip, uint16_t port);
  void receiveFromIP(in_addr ip, uint16_t port, VideoWidget* view);

  // call changes. Returns state after toggle
  bool toggleMic();
  bool toggleCamera();

  // use this function to avoid a race condition with ports
  bool reservePorts();

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

  uint16_t portsForCallID(QString callID) const
  {
    if(ids_.find(callID) != ids_.end())
    {
      return portsPerParticipant_;
    }
    return 0;
  }

  // callID, PeerID relation pairs
  std::map<QString,PeerID> ids_;

  StatisticsInterface* stats_;

  std::unique_ptr<FilterGraph> fg_;

  MediaSession* session_;

  std::unique_ptr<RTPStreamer> streamer_;

  bool mic_;
  bool camera_;

  uint16_t portsPerParticipant_;
  uint16_t maxPortsOpen_;
  uint16_t portsInUse_;
  uint16_t portsReserved_;

  QMutex portsMutex_;
};
