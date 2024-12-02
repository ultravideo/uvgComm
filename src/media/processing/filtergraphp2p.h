#pragma once

#include "filtergraph.h"


class ScreenShareFilter;
class DisplayFilter;
class AudioCaptureFilter;
class AudioOutputFilter;

class SpeexAEC;
class AudioMixer;


class FilterGraphP2P : public FilterGraph
{
public:
  FilterGraphP2P();
  ~FilterGraphP2P();

  void setSelfViews(QList<VideoInterface*> selfViews);

  virtual void uninit();

  virtual void sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> videoFramedSource,
                   const MediaID &id);

  virtual void receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> videoSink,
                        VideoInterface *view,
                        const MediaID &id);

  virtual void sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> audioFramedSource,
                   const MediaID &id);
  virtual void receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> audioSink,
                        const MediaID &id);

  virtual void running(bool state);

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

  QAudioFormat createAudioFormat(uint8_t channels, uint32_t sampleRate);

  // --------------- Video stuff   --------------------
  GraphSegment cameraGraph_;
  GraphSegment screenShareGraph_;

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
