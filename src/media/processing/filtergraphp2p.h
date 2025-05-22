#pragma once

#include "filtergraph.h"

class ScreenShareFilter;
class DisplayFilter;
class AudioCaptureFilter;
class AudioOutputFilter;

class SpeexAEC;
class AudioMixer;
class KvazaarFilter;
class LibYUVConverter;
class HybridFilter;
class HybridSlaveFilter;


class FilterGraphP2P : public FilterGraph
{
public:
  FilterGraphP2P();
  ~FilterGraphP2P();

  void setSelfViews(QList<VideoInterface*> selfViews);

  virtual void uninit();

  virtual void sendVideoto(uint32_t sessionID,
                           std::shared_ptr<Filter> sender,
                           uint32_t localSSRC,
                           uint32_t remoteSSRC,
                           const QString& remoteCNAME,
                           bool isP2P);

  virtual void receiveVideoFrom(uint32_t sessionID,
                                std::shared_ptr<Filter> receiver,
                                VideoInterface* view,
                                uint32_t remoteSSRC,
                                QString cname);

  virtual void sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> sender, uint32_t localSSRC);
  virtual void receiveAudioFrom(uint32_t sessionID,
                                std::shared_ptr<Filter> receiver,
                                uint32_t remoteSSRC,
                                QString cname);

  virtual void running(bool state);

  void setConferenceSize(uint32_t otherParticipants);
  void updateConferenceSize();

public slots:

  void updateVideoSettings();
  void updateAudioSettings();

protected:
  virtual void destroyPeer(Peer* peer);

  virtual void lastPeerRemoved();

private:

  void selectVideoSource();

  void mic(bool state);
  void camera(bool state);
  void screenShare(bool shareState);

  // iniates camera and attaches a self view to it.
  void initCameraSelfView();

  // iniates encoder and attaches it
  void initVideoSend();

  // iniates encoder and attaches it
  void initializeAudioInput(bool opus);
  void initializeAudioOutput(bool opus);

  void addHybridFilter(std::shared_ptr<HybridFilter> hybrid, GraphSegment &segment);
  void addHybridSlave(std::shared_ptr<HybridSlaveFilter> slave, GraphSegment &segment);

  QAudioFormat createAudioFormat(uint8_t channels, uint32_t sampleRate);

  // --------------- Video stuff   --------------------
  GraphSegment cameraGraph_;
  GraphSegment screenShareGraph_;

  std::shared_ptr<LibYUVConverter> libyuv_;
  std::shared_ptr<KvazaarFilter> kvazaar_;
  std::shared_ptr<HybridFilter> hybrid_;
  std::vector<std::shared_ptr<HybridSlaveFilter>> slaves_;

  std::shared_ptr<DisplayFilter> selfviewFilter_;
  VideoInterface* roiInterface_; // this is the roi surface from settings

  QString videoFormat_;
  bool videoSendIniated_;

  // --------------- Audio stuff   ----------------
  GraphSegment audioInputGraph_;  // mic and stuff after it
  GraphSegment audioOutputGraph_; // stuff before speakers and speakers

  // these are shared between filters
  std::shared_ptr<SpeexAEC> aec_;
  std::shared_ptr<AudioMixer> mixer_;

  bool audioInputInitialized_;
  bool audioOutputInitialized_;

  std::shared_ptr<AudioCaptureFilter> audioCapture_;
  std::shared_ptr<AudioOutputFilter>  audioOutput_;

  // audio configs
  QAudioFormat format_;
};
