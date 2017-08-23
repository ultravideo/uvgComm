#pragma once

#include "mediamanager.h"
#include "callnegotiation.h"
#include "callwindow.h"
#include "participantinterface.h"

#include <QObject>

struct SDPMessageInfo;
class StatisticsInterface;

class CallManager : public QObject, public ParticipantInterface
{
  Q_OBJECT
public:
  CallManager();

  void init();

  virtual void callToParticipant(QString name, QString username, QString ip);
  virtual void chatWithParticipant(QString name, QString username, QString ip);

  void callOurselves(QString callID, std::shared_ptr<SDPMessageInfo> info);

  void createParticipant(QString& callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                         const std::shared_ptr<SDPMessageInfo> localInfo,
                         VideoWidget *videoWidget,
                         StatisticsInterface* stats);

  void removeParticipant(QString callID);

  void endCall();

  void uninit();

public slots:

  void incomingCall(QString callID, QString caller);

  void recordChangedSettings();

  void micState();
  void cameraState();

  void callNegotiated(QString callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                     std::shared_ptr<SDPMessageInfo> localInfo);

  void windowClosed();

  void acceptCall(QString callID);
  void rejectCall(QString callID);

private:

  bool roomForMoreParticipants() const;

  MediaManager media_; // Media processing and delivery
  CallNegotiation callNeg_; // SIP
  CallWindow window_; // GUI

  uint16_t portsOpen_;

  StatisticsInterface* stats_;
};

