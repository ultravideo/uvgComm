#pragma once

#include "rtpstreamer.h"
#include "filtergraph.h"

#include <QObject>
#include <memory>
#include <inaddr.h>

class VideoWidget;
class StatisticsInterface;

struct VideoConfig
{
  QSize resolution_; // scaled from 640x480
};

struct VideoOptions
{
  //QList<QCameraInfo> cameras_;
};

struct AudioConfig
{

};

struct AudioOptions
{

};

class CallManager : public QObject
{
  Q_OBJECT

public:
  CallManager(StatisticsInterface* stats);
  ~CallManager();

  void init();
  void uninit();

  // registers a contact for activity monitoring
  void registerContact(in_addr ip);

  void startCall(VideoWidget* selfView, QSize resolution);
  void addParticipant(in_addr ip, uint16_t audioPort,
                      uint16_t videoPort, VideoWidget* view);
  void kickParticipant();

  // callID in case more than one person is calling
  void joinCall(unsigned int callID);

  void endCall();

  void streamToIP(in_addr ip, uint16_t port);
  void receiveFromIP(in_addr ip, uint16_t port, VideoWidget* view);

  AudioOptions* giveAudioOption();
  void setAudioConfig(AudioConfig* config);

  VideoOptions* giveVideoOptions();
  void setVideoConfig(VideoConfig* config);

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


  std::vector<PeerID> ids_;

  StatisticsInterface* stats_;

  std::unique_ptr<FilterGraph> fg_;


  MediaSession* session_;

  std::unique_ptr<RTPStreamer> streamer_;

  bool mic_;
  bool camera_;
};
